#include "FileUtils.hpp"
#include <filesystem>
#include <syslog.h>

namespace fs = std::filesystem;

void FileUtils::moveAll(const std::string& from, const std::string& to) {
    try {
        // Проверка исходной директории
        if (!fs::exists(from)) {
            syslog(LOG_ERR, "Source directory does not exist: %s", from.c_str());
            return;
        }
        if (!fs::is_directory(from)) {
            syslog(LOG_ERR, "Source is not a directory: %s", from.c_str());
            return;
        }

        // Создание целевой директории
        try {
            fs::create_directories(to);
        } catch (const std::filesystem::filesystem_error& e) {
            syslog(LOG_ERR, "Failed to create directory %s: %s", to.c_str(), e.what());
            return;
        }

        // Перемещение файлов
        for (const auto& entry : fs::directory_iterator(from)) {
            if (!entry.is_regular_file()) {
                syslog(LOG_WARNING, "Skipping non-regular file: %s", entry.path().c_str());
                continue;
            }

            try {
                auto destPath = fs::path(to) / entry.path().filename();
                
                // Удаление целевого файла, если он существует
                if (fs::exists(destPath)) {
                    fs::remove(destPath);
                }

                fs::rename(entry.path(), destPath);
                syslog(LOG_INFO, "Moved %s to %s", entry.path().c_str(), destPath.c_str());
            } catch (const std::filesystem::filesystem_error& e) {
                syslog(LOG_ERR, "Failed to move %s: %s", entry.path().c_str(), e.what());
            }
        }
    } catch (const std::exception& e) {
        syslog(LOG_ERR, "Unexpected error in moveAll: %s", e.what());
    }
}
