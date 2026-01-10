#include "conn_base.h"
#include <time.h>
#include <errno.h>
#include <cstring>
#include <iostream>

bool ConnBase::WaitWithTimeout(sem_t *sem, int timeout_seconds) {
    if (!sem) return false;
    
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        std::cerr << "Error: clock_gettime failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    ts.tv_sec += timeout_seconds;
    
    int result = sem_timedwait(sem, &ts);
    if (result == -1) {
        if (errno == ETIMEDOUT) {
            std::cerr << "Error: Semaphore wait timeout (" << timeout_seconds << " seconds)" << std::endl;
            return false;
        }
        std::cerr << "Error: sem_timedwait failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

void ConnBase::Post(sem_t *sem) {
    if (sem) {
        sem_post(sem);
    }
}

