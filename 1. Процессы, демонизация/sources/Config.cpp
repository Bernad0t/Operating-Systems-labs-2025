#include "Config.hpp"
#include "MoveTask.hpp"
#include <fstream>

Config::Config(const std::string& path) : configPath(path) {}

std::vector<std::unique_ptr<Task>> Config::load() {
    std::ifstream in(configPath);
    std::vector<std::unique_ptr<Task>> tasks;

    std::string f1, f2;
    int t;

    while (in >> f1 >> f2 >> t) {
        tasks.push_back(std::make_unique<MoveTask>(f1, f2, t));
    }
    return tasks;
}