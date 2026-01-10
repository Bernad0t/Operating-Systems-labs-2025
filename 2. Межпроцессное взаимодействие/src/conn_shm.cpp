#define _POSIX_C_SOURCE 200112L
#include "conn_shm.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <errno.h>

ConnShm::ConnShm(const std::string& id, bool create) : id_(id) {
    is_host_ = create;
    mem_size_ = BUFFER_SIZE + sizeof(size_t); // buffer + size field
    
    shm_name_ = "/shm_" + id;
    sem_read_name_ = "/shm_read_sem_" + id;
    sem_write_name_ = "/shm_write_sem_" + id;
    pid_file_ = "/tmp/shm_pid_" + id;
    
    if (create) {
        // Создаем именованную shared memory с помощью shm_open
        shm_unlink(shm_name_.c_str()); // Удаляем если существует
        
        shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR | O_EXCL, 0644);
        if (shm_fd_ == -1) {
            std::cerr << "Error: shm_open failed: " << strerror(errno) << std::endl;
            return;
        }
        
        // Устанавливаем размер
        if (ftruncate(shm_fd_, mem_size_) == -1) {
            std::cerr << "Error: ftruncate failed: " << strerror(errno) << std::endl;
            close(shm_fd_);
            shm_fd_ = -1;
            shm_unlink(shm_name_.c_str());
            return;
        }
        
        // Маппируем в память
        shared_mem_ = mmap(nullptr, mem_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (shared_mem_ == MAP_FAILED) {
            std::cerr << "Error: mmap failed: " << strerror(errno) << std::endl;
            close(shm_fd_);
            shm_fd_ = -1;
            shm_unlink(shm_name_.c_str());
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
            close(shm_fd_);
            shm_fd_ = -1;
            shm_unlink(shm_name_.c_str());
            return;
        }
        
        // Создаем файл с именем для клиента
        int pid_fd = open(pid_file_.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (pid_fd != -1) {
            write(pid_fd, shm_name_.c_str(), shm_name_.length());
            close(pid_fd);
        }
        
        std::cout << "Host: Created shm connection, id=" << id << std::endl;
        is_valid_ = true;
    } else {
        // Клиент: читаем имя shared memory из pid файла
        int pid_fd = open(pid_file_.c_str(), O_RDONLY);
        if (pid_fd == -1) {
            std::cerr << "Error: Cannot open pid file: " << strerror(errno) << std::endl;
            return;
        }
        
        char shm_name_buf[256];
        ssize_t bytes = read(pid_fd, shm_name_buf, sizeof(shm_name_buf) - 1);
        close(pid_fd);
        
        if (bytes <= 0) {
            std::cerr << "Error: Cannot read shm name from pid file" << std::endl;
            return;
        }
        shm_name_buf[bytes] = '\0';
        shm_name_ = std::string(shm_name_buf);
        
        // Открываем shared memory
        shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
        if (shm_fd_ == -1) {
            std::cerr << "Error: shm_open failed (client): " << strerror(errno) << std::endl;
            return;
        }
        
        // Маппируем в память
        shared_mem_ = mmap(nullptr, mem_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (shared_mem_ == MAP_FAILED) {
            std::cerr << "Error: mmap failed (client): " << strerror(errno) << std::endl;
            close(shm_fd_);
            shm_fd_ = -1;
            return;
        }
        
        // Открываем семафоры
        read_sem_ = sem_open(sem_read_name_.c_str(), 0);
        write_sem_ = sem_open(sem_write_name_.c_str(), 0);
        
        if (read_sem_ == SEM_FAILED || write_sem_ == SEM_FAILED) {
            std::cerr << "Error: sem_open failed (client): " << strerror(errno) << std::endl;
            munmap(shared_mem_, mem_size_);
            shared_mem_ = nullptr;
            close(shm_fd_);
            shm_fd_ = -1;
            return;
        }
        
        std::cout << "Client: Connected to shm, id=" << id << std::endl;
        is_valid_ = true;
    }
}

ConnShm::~ConnShm() {
    Close();
}

bool ConnShm::Read(void *buf, size_t count) {
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

bool ConnShm::Write(const void *buf, size_t count) {
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
    
    Post(write_sem_); // Освобождаем write sem
    Post(read_sem_);  // Сигнализируем что данные готовы
    return true;
}

bool ConnShm::IsValid() const {
    return is_valid_ && shared_mem_ != nullptr;
}

void ConnShm::Close() {
    if (shared_mem_ && shared_mem_ != MAP_FAILED) {
        munmap(shared_mem_, mem_size_);
        shared_mem_ = nullptr;
    }
    
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;
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
            shm_unlink(shm_name_.c_str());
        }
        write_sem_ = nullptr;
    }
    
    if (is_host_) {
        unlink(pid_file_.c_str());
    }
    
    is_valid_ = false;
}

