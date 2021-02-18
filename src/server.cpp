#include "Common.h"
#include "ServerOrganizer.h"
#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

static inline std::ofstream logfile;

void info(const std::string& str) {
    std::cout << "[" << get_date_time_string() << "] [INFO] " << str << std::endl;
    logfile << "[" << get_date_time_string() << "] [INFO] " << str << std::endl;
}

void warn(const std::string& str) {
    std::cout << "[" << get_date_time_string() << "] [WARNING] " << str << std::endl;
    logfile << "[" << get_date_time_string() << "] [WARNING] " << str << std::endl;
}

void error(const std::string& str) {
    std::cout << "[" << get_date_time_string() << "] [ERROR] " << str << std::endl;
    logfile << "[" << get_date_time_string() << "] [ERROR] " << str << std::endl;
}

static void signal_handler(int sig) {
    switch (sig) {
    case SIGTERM:
        info("exiting through SIGTERM");
        unlink(SOCKET_FILENAME);
        exit(0);
    case SIGINT:
        info("exiting through SIGINT");
        unlink(SOCKET_FILENAME);
        exit(0);
    default:
        return;
    }
}

// headless server
int main(int argc, char* argv[]) {
    bool clean = false;
    std::string working_directory = ".";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--clean") {
            clean = true;
        } else if (arg == "--dir" && argc > i + 1) {
            working_directory = argv[i + 1];
            i += 1;
        } else {
            std::cout << "argument \"" + arg + "\" unknown or missing parameters" << std::endl;
            return -1;
        }
    }
    std::cout << "ServerOrganizer v1.0 Headless Server" << std::endl;
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    struct stat st { };
    std::filesystem::path cwd = getcwd(nullptr, 0);
    if (!working_directory.starts_with('/')) {
        cwd /= working_directory;
    } else {
        cwd = working_directory;
    }
    int ret = chdir(cwd.c_str());
    if (ret != 0) {
        std::cout << "invalid working directory: " << cwd.string() << std::endl;
        return -1;
    }
    if (stat("logs", &st) != 0) {
        mkdir("logs", 0700);
    }
    // DONT LOG BEFORE THIS POINT
    logfile.open("logs/" + generate_logfile_name("ServerOrganizer_HeadlessServer"));
    info("working directory: " + cwd.string());
    if (clean) {
        info("cleaning up previous runs");
        if (stat(SOCKET_FILENAME, &st) == 0) {
            ret = unlink(SOCKET_FILENAME);
            if (ret != 0) {
                error("unlinking \"" + std::string(SOCKET_FILENAME) + "\" failed: " + std::string(std::strerror(errno))
                    + ". if the file exists, removing it manually will fix this issue.");
            }
        } else {
            info("socket file not found, not removing it");
        }
    }
    ServerOrganizer s_o_instance;
    return s_o_instance.run();
}