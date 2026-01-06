#pragma once
#include <vector>
#include <memory>
#include "Task.hpp"

class Config {
public:
    explicit Config(const std::string& path);
    std::vector<std::unique_ptr<Task>> load();

private:
    std::string configPath;
};