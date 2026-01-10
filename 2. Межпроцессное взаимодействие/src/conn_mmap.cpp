#define _POSIX_C_SOURCE 200112L
#include "conn_mmap.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <errno.h>

ConnMmap::ConnMmap(const std::string& id, bool create) : is_host_(create), id_(id) {
    mem_size_ = BUFFER_SIZE + sizeof(size_t); // buffer + size field
    
    sem_read_name_ = "/mmap_read_sem_" + id;
    sem_write_name_ = "/mmap_write_sem_" + id;
    shm_file_ = "/tmp/mmap_shm_" + id;
    pid_file_ = "/tmp/mmap_pid_" + id;
    
    if (create) {
        // Для независимых процессов используем файл-based mmap
        fd_ = open(shm_file_.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
        if (fd_ == -1) {
            std::cerr << "Error: Cannot create shared memory file: " << strerror(errno) << std::endl;
            return;
        }
        
        // Устанавливаем размер файла
        if (ftruncate(fd_, mem_size_) == -1) {
            std::cerr << "Error: ftruncate failed: " << strerror(errno) << std::endl;
            close(fd_);
            fd_ = -1;
            return;
        }
        
        // Маппируем файл в память
        shared_mem_ = mmap(nullptr, mem_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (shared_mem_ == MAP_FAILED) {
            std::cerr << "Error: mmap failed: " << strerror(errno) << std::endl;
            close(fd_);
            fd_ = -1;
            unlink(shm_file_.c_str());
            return;
        }
        memset(shared_mem_, 0, mem_size_);
        
        // Создаем семафоры
        sem_unlink(sem_read_name_.c_str());
        sem_unlink(sem_write_name_.c_str());
        
        read_sem_ = sem_open(sem_read_name_.c_str(), O_CREAT | O_EXCL, 0644, 0);
        write_sem_ = sem_open(sem_write_name_.c_str(), O_CREAT | O_EXCL, 0644, 1);
        
        if (read_sem_ == SEM_FAILED || write_sem_ == SEM_FAILED) {
            std::cerr << "Error: sem_open failed: " << strerror(errno) << std::endl;
            if (read_sem_ != SEM_FAILED) sem_close(read_sem_);
            if (write_sem_ != SEM_FAILED) sem_close(write_sem_);
            munmap(shared_mem_, mem_size_);
            shared_mem_ = nullptr;
            close(fd_);
            fd_ = -1;
            unlink(shm_file_.c_str());
            return;
        }
        
        // Создаем файл с именем файла для клиента
        int pid_fd = open(pid_file_.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (pid_fd != -1) {
            write(pid_fd, shm_file_.c_str(), shm_file_.length());
            close(pid_fd);
        }
        
        std::cout << "Host: Created mmap connection, id=" << id << std::endl;
        is_valid_ = true;
    } else {
        // Клиент: читаем имя файла из pid файла
        fd_ = open(pid_file_.c_str(), O_RDONLY);
        if (fd_ == -1) {
            std::cerr << "Error: Cannot open pid file: " << strerror(errno) << std::endl;
            return;
        }
        
        char file_path[256];
        ssize_t bytes = read(fd_, file_path, sizeof(file_path) - 1);
        close(fd_);
        fd_ = -1;
        
        if (bytes <= 0) {
            std::cerr << "Error: Cannot read file path from pid file" << std::endl;
            return;
        }
        file_path[bytes] = '\0';
        
        // Открываем файл shared memory
        std::string shm_file_path(file_path);
        fd_ = open(shm_file_path.c_str(), O_RDWR);
        if (fd_ == -1) {
            std::cerr << "Error: Cannot open shared memory file: " << strerror(errno) << std::endl;
            return;
        }
        
        shared_mem_ = mmap(nullptr, mem_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (shared_mem_ == MAP_FAILED) {
            std::cerr << "Error: mmap failed (client): " << strerror(errno) << std::endl;
            close(fd_);
            fd_ = -1;
            return;
        }
        
        // Открываем семафоры
        read_sem_ = sem_open(sem_read_name_.c_str(), 0);
        write_sem_ = sem_open(sem_write_name_.c_str(), 0);
        
        if (read_sem_ == SEM_FAILED || write_sem_ == SEM_FAILED) {
            std::cerr << "Error: sem_open failed (client): " << strerror(errno) << std::endl;
            munmap(shared_mem_, mem_size_);
            shared_mem_ = nullptr;
            close(fd_);
            fd_ = -1;
            return;
        }
        
        std::cout << "Client: Connected to mmap, id=" << id << std::endl;
        is_valid_ = true;
    }
}

ConnMmap::~ConnMmap() {
    Close();
}

bool ConnMmap::Read(void *buf, size_t count) {
    if (!is_valid_ || !shared_mem_) {
        std::cerr << "Error: Connection not valid for reading" << std::endl;
        return false;
    }
    
    if (!WaitWithTimeout(read_sem_)) {
        return false;
    }
    
    size_t* size_ptr = static_cast<size_t*>(shared_mem_);
    size_t data_size = *size_ptr;
    if (data_size > count) {
        data_size = count;
    }
    
    memcpy(buf, static_cast<char*>(shared_mem_) + sizeof(size_t), data_size);
    *size_ptr = 0; // Очищаем после чтения
    
    Post(write_sem_);
    return true;
}

bool ConnMmap::Write(const void *buf, size_t count) {
    if (!is_valid_ || !shared_mem_) {
        std::cerr << "Error: Connection not valid for writing" << std::endl;
        return false;
    }
    
    if (!WaitWithTimeout(write_sem_)) {
        return false;
    }
    
    size_t* size_ptr = static_cast<size_t*>(shared_mem_);
    if (count > BUFFER_SIZE) {
        count = BUFFER_SIZE;
    }
    
    *size_ptr = count;
    memcpy(static_cast<char*>(shared_mem_) + sizeof(size_t), buf, count);
    
    Post(write_sem_); // Освобождаем write sem перед сигналом read
    Post(read_sem_);  // Сигнализируем что данные готовы
    return true;
}

bool ConnMmap::IsValid() const {
    return is_valid_ && shared_mem_ != nullptr;
}

void ConnMmap::Close() {
    if (shared_mem_ && shared_mem_ != MAP_FAILED) {
        munmap(shared_mem_, mem_size_);
        shared_mem_ = nullptr;
    }
    
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
    
    if (read_sem_) {
        sem_close(read_sem_);
        if (is_host_) {
            sem_unlink(sem_read_name_.c_str());
        }
        read_sem_ = nullptr;
    }
    
    if (write_sem_) {
        sem_close(write_sem_);
        if (is_host_) {
            sem_unlink(sem_write_name_.c_str());
        }
        write_sem_ = nullptr;
    }
    
    if (is_host_) {
        unlink(shm_file_.c_str());
        unlink(pid_file_.c_str());
    }
    
    is_valid_ = false;
}

