#include "Daemon.hpp"
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc != 2)
        return 1;

    auto configPath = std::filesystem::absolute(argv[1]);
    Daemon::Instance().run(configPath.string());

    return 0;
}