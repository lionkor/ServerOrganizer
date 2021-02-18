#include "Common.h"
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

static inline std::ofstream logfile;

void info(const std::string& str, std::ostream& os = std::cout) {
    os << "[" << get_date_time_string() << "] [INFO] " << str << std::endl;
    if (os.rdbuf() == std::cout.rdbuf())
        info(str, logfile);
}

void warn(const std::string& str, std::ostream& os = std::cout) {
    os << "[" << get_date_time_string() << "] [WARNING] " << str << std::endl;
    if (os.rdbuf() == std::cout.rdbuf())
        warn(str, logfile);
}

void error(const std::string& str, std::ostream& os = std::cout) {
    os << "[" << get_date_time_string() << "] [ERROR] " << str << std::endl;
    if (os.rdbuf() == std::cout.rdbuf())
        error(str, logfile);
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

std::atomic_bool s_shutdown = false;

struct Client {
    struct sockaddr sock_addr;
    socklen_t addrlen;
    int socket_fd;
};

namespace serverside_commands {
std::string help(const std::string&) {
    return "no help available yet";
}
std::string register_new(const std::string&) {
    return "registered";
}
}

std::map<std::string, std::function<std::string(const std::string&)>> command_function_map = {
    { "help", serverside_commands::help },
    { "register", serverside_commands::register_new },
};

Message process_message(Message&& msg) {
    info("got command: \"" + msg.to_string() + "\"");
    Message response {};
    auto str = msg.to_string();
    if (str == "kickme") {
        response = Message::from_string(Command::Detach);
    } else if (command_function_map.contains(str)) {
        response = Message::from_string(command_function_map.at(str)(str));
    } else {
        response = Message::from_string("unknown command");
    }
    return response;
}

void run_client(Client&& client) {
    info("client connected");
    // make socket non-blocking
    // fcntl(client.socket_fd, F_SETFL, O_NONBLOCK);
    int err = 0;
    socklen_t len = sizeof(err);
    while (getsockopt(client.socket_fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
        if (recv(client.socket_fd, nullptr, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {
            info("client connection died");
            break;
        }
        std::array<char, 1024> data {};
        int ret = recv(client.socket_fd, data.data(), data.size(), MSG_WAITALL);
        if (ret != data.size()) {
            error("error: received invalid size message: " + std::to_string(ret) + ", with error: " + std::string(std::strerror(errno)));
            break;
        }
        Message response_message = process_message(Message::deserialize(data));
        auto response = response_message.serialize();
        ret = send(client.socket_fd, response.data(), response.size(), MSG_NOSIGNAL);
        if (ret != response.size()) {
            error("error during send: " + std::string(std::strerror(errno)));
            break;
        }
        if (response_message.to_string() == Command::Detach) {
            info("kicked client with detach request, closing connection");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    shutdown(client.socket_fd, SHUT_RDWR);
    close(client.socket_fd);
    info("client disconnected");
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
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr {
        AF_UNIX, { }
    };
    info("socket created");
    strncpy(addr.sun_path, SOCKET_FILENAME, sizeof(addr.sun_path) - 1);
    ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret != 0) {
        error("failed to bind: " + std::string(std::strerror(errno)) + " - this is usually caused by the server not shutting down properly. use --clean to force start.");
        return -1;
    }
    info("socket bound");
    std::vector<std::thread> clients;
    while (!s_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        listen(fd, 1);
        Client client {};
        client.socket_fd = accept(fd, &client.sock_addr, &client.addrlen);
        if (client.socket_fd < 0) {
            error("could not accept(): " + std::string(std::strerror(errno)));
        }
        std::thread& t = clients.emplace_back(run_client, client);
        t.detach();
    }
    unlink(SOCKET_FILENAME);
}