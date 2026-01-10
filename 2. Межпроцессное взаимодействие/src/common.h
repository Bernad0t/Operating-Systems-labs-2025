#pragma once

#include <string>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>
#include <cstring>
#include <errno.h>

enum class ConnType {
    MMAP,
    SHM,
    FIFO
};

// Функция для отправки сигнала SIGUSR1 хосту
bool SendHandshakeSignal(pid_t host_pid);

// Функция для обработки сигнала handshake на хосте
void SetupHandshakeHandler(void (*handler)(int));

// Функция для ожидания сигнала с таймаутом
bool WaitForSignalWithTimeout(int timeout_seconds = 5);

