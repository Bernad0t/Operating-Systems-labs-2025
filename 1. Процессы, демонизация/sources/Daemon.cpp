#include "Daemon.hpp"
#include "Config.hpp"

#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <cstdlib>
#include <sys/stat.h> 

Daemon::Daemon() = default;

Daemon& Daemon::Instance() {
    static Daemon instance;  // Meyers Singleton
    return instance;
}

void Daemon::signalHandler(int sig) {
    if (sig == SIGHUP) {
        syslog(LOG_INFO, "SIGHUP received: reloading config");
        Daemon::Instance().loadConfig();
    }
    else if (sig == SIGTERM) {
        syslog(LOG_INFO, "SIGTERM received: exiting daemon");
        closelog();
        std::_Exit(0);
    }
}

void Daemon::daemonize() {
    pid_t pid = fork();
    if (pid < 0)
        std::_Exit(EXIT_FAILURE);
    if (pid > 0)
        std::_Exit(EXIT_SUCCESS);

    if (setsid() < 0)
        std::_Exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0)
        std::_Exit(EXIT_FAILURE);
    if (pid > 0)
        std::_Exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void Daemon::loadConfig() {
    Config cfg(configPath_);
    tasks_ = cfg.load();
}

void Daemon::run(const std::string& configPath) {
    configPath_ = configPath;

    daemonize();

    openlog("daemon15", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    signal(SIGHUP, Daemon::signalHandler);
    signal(SIGTERM, Daemon::signalHandler);

    syslog(LOG_INFO, "Daemon started");
    loadConfig();

    while (true) {
        for (auto& task : tasks_)
            task->execute();
        sleep(1);
    }
}