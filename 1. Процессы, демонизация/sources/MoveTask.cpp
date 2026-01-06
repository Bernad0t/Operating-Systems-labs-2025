#include "MoveTask.hpp"
#include "FileUtils.hpp"
#include <syslog.h>

MoveTask::MoveTask(std::string f, std::string t, int sec)
    : src(std::move(f)), dst(std::move(t)), seconds(sec),
      lastRun(std::chrono::steady_clock::now()) {}

int MoveTask::interval() const {
    return seconds;
}

void MoveTask::execute() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastRun).count() < seconds)
        return;

    syslog(LOG_INFO, "MoveTask: moving files from %s to %s", src.c_str(), dst.c_str());
    FileUtils::moveAll(src, dst);
    lastRun = now;
}