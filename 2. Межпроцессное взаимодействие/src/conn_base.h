#pragma once

#include <cstddef>
#include <string>
#include <semaphore.h>

// Базовый класс для всех типов соединений
class ConnBase {
public:
    virtual ~ConnBase() = default;
    virtual bool Read(void *buf, size_t count) = 0;
    virtual bool Write(const void *buf, size_t count) = 0;
    virtual bool IsValid() const = 0;
    virtual void Close() = 0;

protected:
    sem_t *read_sem_ = nullptr;
    sem_t *write_sem_ = nullptr;
    bool is_host_ = false;
    
    bool WaitWithTimeout(sem_t *sem, int timeout_seconds = 5);
    void Post(sem_t *sem);
};

