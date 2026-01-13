#pragma once

#include "conn_base.h"
#include <string>

class ConnFifo : public ConnBase {
public:
    ConnFifo(const std::string& id, bool create);
    ~ConnFifo() override;
    
    bool Read(void *buf, size_t count) override;
    bool Write(const void *buf, size_t count) override;
    bool IsValid() const override;
    void Close() override;

private:
    int fifo_read_fd_ = -1;
    int fifo_write_fd_ = -1;
    std::string fifo_host_to_client_;
    std::string fifo_client_to_host_;
    std::string sem_read_name_;
    std::string sem_write_name_;
    std::string pid_file_;
    std::string id_;
    bool is_valid_ = false;
    
    static const size_t BUFFER_SIZE = 4096;
};

