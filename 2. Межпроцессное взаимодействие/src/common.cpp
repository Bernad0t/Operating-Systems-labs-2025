#include "common.h"
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <cstring>
#include <iostream>

static volatile sig_atomic_t signal_received = 0;

static void handshake_signal_handler(int sig) {
    if (sig == SIGUSR1) {
        signal_received = 1;
    }
}

void SetHandshakeReceived() {
    signal_received = 1;
}

bool SendHandshakeSignal(pid_t host_pid) {
    if (kill(host_pid, SIGUSR1) == -1) {
        std::cerr << "Error: Failed to send handshake signal: " << strerror(errno) << std::endl;
        return false;
    }
    std::cout << "Client: Sent handshake signal to host (PID: " << host_pid << ")" << std::endl;
    return true;
}

void SetupHandshakeHandler(void (*handler)(int)) {
    struct sigaction sa;
    sa.sa_handler = handler ? handler : handshake_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGUSR1, &sa, nullptr) == -1) {
        std::cerr << "Error: Failed to setup signal handler: " << strerror(errno) << std::endl;
    }
}

bool WaitForSignalWithTimeout(int timeout_seconds) {
    signal_received = 0;
    
    struct timespec start, current;
    clock_gettime(CLOCK_REALTIME, &start);
    
    while (!signal_received) {
        clock_gettime(CLOCK_REALTIME, &current);
        double elapsed = (current.tv_sec - start.tv_sec) + (current.tv_nsec - start.tv_nsec) / 1e9;
        
        if (elapsed >= timeout_seconds) {
            std::cerr << "Error: Handshake timeout (" << timeout_seconds << " seconds)" << std::endl;
            return false;
        }
        
        usleep(10000); // 10ms
    }
    
    std::cout << "Host: Received handshake signal" << std::endl;
    return true;
}

