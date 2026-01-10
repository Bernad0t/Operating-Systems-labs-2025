#define _POSIX_C_SOURCE 200112L
#include "conn_fifo.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <errno.h>

ConnFifo::ConnFifo(const std::string& id, bool create) : is_host_(create), id_(id) {
    fifo_host_to_client_ = "/tmp/fifo_host_to_client_" + id;
    fifo_client_to_host_ = "/tmp/fifo_client_to_host_" + id;
    sem_read_name_ = "/fifo_read_sem_" + id;
    sem_write_name_ = "/fifo_write_sem_" + id;
    pid_file_ = "/tmp/fifo_pid_" + id;
    
    if (create) {
        // Создаем именованные каналы
        unlink(fifo_host_to_client_.c_str());
        unlink(fifo_client_to_host_.c_str());
        
        if (mkfifo(fifo_host_to_client_.c_str(), 0644) == -1) {
            std::cerr << "Error: mkfifo failed (host_to_client): " << strerror(errno) << std::endl;
            return;
        }
        
        if (mkfifo(fifo_client_to_host_.c_str(), 0644) == -1) {
            std::cerr << "Error: mkfifo failed (client_to_host): " << strerror(errno) << std::endl;
            unlink(fifo_host_to_client_.c_str());
            return;
        }
        
        // Создаем семафоры
        sem_unlink(sem_read_name_.c_str());
        sem_unlink(sem_write_name_.c_str());
        
        read_sem_ = sem_open(sem_read_name_.c_str(), O_CREAT | O_EXCL, 0644, 0);
        write_sem_ = sem_open(sem_write_name_.c_str(), O_CREAT | O_EXCL, 0644, 1);
        
        if (read_sem_ == SEM_FAILED || write_sem_ == SEM_FAILED) {
            std::cerr << "Error: sem_open failed: " << strerror(errno) << std::endl;
            if (read_sem_ != SEM_FAILED) sem_close(read_sem_);
            if (write_sem_ != SEM_FAILED) sem_close(write_sem_);
            unlink(fifo_host_to_client_.c_str());
            unlink(fifo_client_to_host_.c_str());
            return;
        }
        
        // Открываем каналы для хоста: запись в host->client, чтение из client->host
        fifo_write_fd_ = open(fifo_host_to_client_.c_str(), O_WRONLY);
        if (fifo_write_fd_ == -1) {
            std::cerr << "Error: Cannot open fifo for writing: " << strerror(errno) << std::endl;
            sem_close(read_sem_);
            sem_close(write_sem_);
            sem_unlink(sem_read_name_.c_str());
            sem_unlink(sem_write_name_.c_str());
            unlink(fifo_host_to_client_.c_str());
            unlink(fifo_client_to_host_.c_str());
            return;
        }
        
        fifo_read_fd_ = open(fifo_client_to_host_.c_str(), O_RDONLY);
        if (fifo_read_fd_ == -1) {
            std::cerr << "Error: Cannot open fifo for reading: " << strerror(errno) << std::endl;
            close(fifo_write_fd_);
            fifo_write_fd_ = -1;
            sem_close(read_sem_);
            sem_close(write_sem_);
            sem_unlink(sem_read_name_.c_str());
            sem_unlink(sem_write_name_.c_str());
            unlink(fifo_host_to_client_.c_str());
            unlink(fifo_client_to_host_.c_str());
            return;
        }
        
        // Создаем файл с именами каналов для клиента
        int pid_fd = open(pid_file_.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (pid_fd != -1) {
            std::string fifo_info = fifo_client_to_host_ + "\n" + fifo_host_to_client_ + "\n";
            write(pid_fd, fifo_info.c_str(), fifo_info.length());
            close(pid_fd);
        }
        
        std::cout << "Host: Created fifo connection, id=" << id << std::endl;
        is_valid_ = true;
    } else {
        // Клиент: читаем имена каналов из pid файла
        int pid_fd = open(pid_file_.c_str(), O_RDONLY);
        if (pid_fd == -1) {
            std::cerr << "Error: Cannot open pid file: " << strerror(errno) << std::endl;
            return;
        }
        
        char fifo_info[512];
        ssize_t bytes = read(pid_fd, fifo_info, sizeof(fifo_info) - 1);
        close(pid_fd);
        
        if (bytes <= 0) {
            std::cerr << "Error: Cannot read fifo names from pid file" << std::endl;
            return;
        }
        fifo_info[bytes] = '\0';
        
        // Парсим имена каналов
        char* line1 = fifo_info;
        char* line2 = strchr(line1, '\n');
        if (!line2) {
            std::cerr << "Error: Invalid fifo info format" << std::endl;
            return;
        }
        *line2 = '\0';
        line2++;
        
        fifo_client_to_host_ = std::string(line1);
        fifo_host_to_client_ = std::string(line2);
        
        // Открываем каналы для клиента: запись в client->host, чтение из host->client
        fifo_write_fd_ = open(fifo_client_to_host_.c_str(), O_WRONLY);
        if (fifo_write_fd_ == -1) {
            std::cerr << "Error: Cannot open fifo for writing (client): " << strerror(errno) << std::endl;
            return;
        }
        
        fifo_read_fd_ = open(fifo_host_to_client_.c_str(), O_RDONLY);
        if (fifo_read_fd_ == -1) {
            std::cerr << "Error: Cannot open fifo for reading (client): " << strerror(errno) << std::endl;
            close(fifo_write_fd_);
            fifo_write_fd_ = -1;
            return;
        }
        
        // Открываем семафоры
        read_sem_ = sem_open(sem_read_name_.c_str(), 0);
        write_sem_ = sem_open(sem_write_name_.c_str(), 0);
        
        if (read_sem_ == SEM_FAILED || write_sem_ == SEM_FAILED) {
            std::cerr << "Error: sem_open failed (client): " << strerror(errno) << std::endl;
            close(fifo_read_fd_);
            close(fifo_write_fd_);
            fifo_read_fd_ = -1;
            fifo_write_fd_ = -1;
            return;
        }
        
        std::cout << "Client: Connected to fifo, id=" << id << std::endl;
        is_valid_ = true;
    }
}

ConnFifo::~ConnFifo() {
    Close();
}

bool ConnFifo::Read(void *buf, size_t count) {
    if (!is_valid_ || fifo_read_fd_ == -1) {
        std::cerr << "Error: Connection not valid for reading" << std::endl;
        return false;
    }
    
    if (!WaitWithTimeout(read_sem_)) {
        return false;
    }
    
    // Читаем размер данных
    size_t data_size = 0;
    ssize_t bytes_read = read(fifo_read_fd_, &data_size, sizeof(data_size));
    if (bytes_read != sizeof(data_size)) {
        std::cerr << "Error: Failed to read size from fifo" << std::endl;
        Post(write_sem_);
        return false;
    }
    
    if (data_size > count) {
        data_size = count;
    }
    
    // Читаем данные
    bytes_read = read(fifo_read_fd_, buf, data_size);
    if (bytes_read != static_cast<ssize_t>(data_size)) {
        std::cerr << "Error: Failed to read data from fifo" << std::endl;
        Post(write_sem_);
        return false;
    }
    
    Post(write_sem_);
    return true;
}

bool ConnFifo::Write(const void *buf, size_t count) {
    if (!is_valid_ || fifo_write_fd_ == -1) {
        std::cerr << "Error: Connection not valid for writing" << std::endl;
        return false;
    }
    
    if (!WaitWithTimeout(write_sem_)) {
        return false;
    }
    
    if (count > BUFFER_SIZE) {
        count = BUFFER_SIZE;
    }
    
    // Записываем размер данных
    ssize_t bytes_written = write(fifo_write_fd_, &count, sizeof(count));
    if (bytes_written != sizeof(count)) {
        std::cerr << "Error: Failed to write size to fifo" << std::endl;
        Post(write_sem_);
        return false;
    }
    
    // Записываем данные
    bytes_written = write(fifo_write_fd_, buf, count);
    if (bytes_written != static_cast<ssize_t>(count)) {
        std::cerr << "Error: Failed to write data to fifo" << std::endl;
        Post(write_sem_);
        return false;
    }
    
    Post(write_sem_); // Освобождаем write sem
    Post(read_sem_);  // Сигнализируем что данные готовы
    return true;
}

bool ConnFifo::IsValid() const {
    return is_valid_ && fifo_read_fd_ != -1 && fifo_write_fd_ != -1;
}

void ConnFifo::Close() {
    if (fifo_read_fd_ != -1) {
        close(fifo_read_fd_);
        fifo_read_fd_ = -1;
    }
    
    if (fifo_write_fd_ != -1) {
        close(fifo_write_fd_);
        fifo_write_fd_ = -1;
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
            unlink(fifo_host_to_client_.c_str());
            unlink(fifo_client_to_host_.c_str());
            unlink(pid_file_.c_str());
        }
        write_sem_ = nullptr;
    }
    
    is_valid_ = false;
}

