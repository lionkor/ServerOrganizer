#include "Common.h"
#include <commandline/commandline.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

static Commandline com {};

void info(const std::string& str) {
    com.write("[" + get_date_time_string() + "] [INFO] " + str);
}

void warn(const std::string& str) {
    com.write("[" + get_date_time_string() + "] [WARNING] " + str);
}

void error(const std::string& str) {
    com.write("[" + get_date_time_string() + "] [ERROR] " + str);
}

int socket_fd = -1;
bool attached { false };

namespace commands {
static constexpr auto help_str = "list of all commands:\n"
                                 "* attach - attempts to attach to a running instance of the ServerOrganizer headless server)\n"
                                 "* help - displays this help\n";
void attach(const std::string&) {
    struct stat st { };
    if (socket_fd != -1) {
        error("already attached");
    } else if (stat(SOCKET_FILENAME, &st) != 0) {
        error("could not attach - ensure that we're in the server's working directory");
    } else {
        info("attaching...");
        socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr {
            AF_UNIX, { }
        };
        strncpy(addr.sun_path, SOCKET_FILENAME, sizeof(addr.sun_path) - 1);
        int ret = connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr));
        if (ret != 0) {
            error("failed to connect: " + std::string(std::strerror(errno)));
            return;
        }
        attached = true;
        info("attached");
    }
}
void help(const std::string&) {
    info(help_str);
}
}

void detach() {
    info("detaching...");
    ::shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);
    socket_fd = -1;
    attached = false;
    com.set_prompt("local > ");
    info("detached");
}

int main() {
    std::map<std::string, std::function<void(const std::string&)>> command_function_map = {
        { "attach", commands::attach },
        { "help", commands::help },
    };
    com.set_prompt("local > ");
    com.enable_history();
    com.set_history_limit(40);
    com.enable_write_to_file(generate_logfile_name());
    info("ServerOrganizer v1.0");
    struct stat st { };
    bool shutdown { false };
    while (!shutdown) {
        if (com.has_command()) {
            auto command = com.get_command();
            com.write(com.prompt() + command);
            if (attached) {
                if (command == "exit") {
                    detach();
                } else {
                    Message msg = Message::from_string(command);
                    auto data = msg.serialize();
                    int ret = send(socket_fd, data.data(), data.size(), MSG_NOSIGNAL);
                    if (ret != 0) {
                        error("error during send: " + std::string(std::strerror(errno)));
                        info("detaching due to error");
                        detach();
                    }
                }
            } else {
                if (command == "exit") {
                    shutdown = true;
                } else if (command_function_map.contains(command)) {
                    command_function_map.at(command)(command);
                    if (attached) {
                        com.set_prompt("server > ");
                    } else {
                        com.set_prompt("local > ");
                    }
                } else {
                    info("command \"" + command + "\" not found");
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}
