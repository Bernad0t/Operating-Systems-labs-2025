#include "FileUtils.hpp"
#include <filesystem>

namespace fs = std::filesystem;

void FileUtils::moveAll(const std::string& from, const std::string& to) {
    fs::create_directories(to);
    for (const auto& entry : fs::directory_iterator(from)) {
        fs::rename(entry.path(), fs::path(to) / entry.path().filename());
    }
}