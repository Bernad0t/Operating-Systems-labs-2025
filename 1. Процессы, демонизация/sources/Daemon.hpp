#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Task.hpp"

class Daemon final {
public:
    // Единственная точка доступа
    static Daemon& Instance();

    // Запуск демона
    void run(const std::string& configPath);

    // Запрещаем копирование и перемещение
    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;
    Daemon(Daemon&&) = delete;
    Daemon& operator=(Daemon&&) = delete;

private:
    // Приватный конструктор
    Daemon();

    // Демонизация
    void daemonize();

    // Загрузка конфигурации
    void loadConfig();

    // Статический обработчик сигналов
    static void signalHandler(int sig);

private:
    std::string configPath_;
    std::vector<std::unique_ptr<Task>> tasks_;
};