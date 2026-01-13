#pragma once

#include "conn_base.h"
#include <string>

class ConnMmap : public ConnBase {
public:
    ConnMmap(const std::string& id, bool create);
    ~ConnMmap() override;
    
    bool Read(void *buf, size_t count) override;
    bool Write(const void *buf, size_t count) override;
    bool IsValid() const override;
    void Close() override;

private:
    void* shared_mem_ = nullptr;
    size_t mem_size_ = 0;
    int fd_ = -1;
    std::string sem_read_name_;
    std::string sem_write_name_;
    std::string shm_file_;
    std::string pid_file_;
    std::string id_;
    bool is_valid_ = false;
    
    static const size_t BUFFER_SIZE = 4096;
};

