#include "Daemon.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Ошибка: не указан путь к конфигурационному файлу!" << std::endl;
        std::cerr << "Использование: " << argv[0] << " <путь_к_конфигу>" << std::endl;
        return 1;
    }

    std::filesystem::path configPath(argv[1]);

    if (!std::filesystem::exists(configPath)) {
        std::cerr << "Ошибка: файл не найден: " << configPath.string() << std::endl;
        return 1;
    }

    std::cout << "Daemon started. Config path: " << configPath.string() << std::endl;
    Daemon::Instance().run(configPath.string());
    return 0;
}