#pragma once
#include "Task.hpp"
#include <string>
#include <chrono>

class MoveTask : public Task {
public:
    MoveTask(std::string from, std::string to, int sec);

    void execute() override;
    int interval() const override;

private:
    std::string src;
    std::string dst;
    int seconds;
    std::chrono::time_point<std::chrono::steady_clock> lastRun;
};