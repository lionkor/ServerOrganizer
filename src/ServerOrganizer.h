#ifndef SERVERORGANIZER_SERVERORGANIZER_H
#define SERVERORGANIZER_SERVERORGANIZER_H

#include "Common.h"
#include <functional>
#include <map>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

class ServerOrganizer {
public:
    struct Client {
        struct sockaddr sock_addr;
        socklen_t addrlen;
        int socket_fd;
    };

    ServerOrganizer();
    ~ServerOrganizer();

    std::string command_help(const std::vector<std::string>& args);
    std::string command_register(const std::vector<std::string>& args);
    std::string command_status(const std::vector<std::string>& args);

    Message process_message(Message&& msg);

    int run();
    void run_client(Client&& client);

private:
    std::atomic_bool m_shutdown = false;
    std::map<std::string, std::function<std::string(const std::vector<std::string>&)>> m_command_function_map = {
        { "help", { [this](const auto& vec) -> std::string { return command_help(vec); } } },
        { "status", { [this](const auto& vec) -> std::string { return command_status(vec); } } },
        { "register", { [this](const auto& vec) -> std::string { return command_register(vec); } } },
    };
};

#endif //SERVERORGANIZER_SERVERORGANIZER_H
