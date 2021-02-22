#ifndef SERVERORGANIZER_SERVERORGANIZER_H
#define SERVERORGANIZER_SERVERORGANIZER_H

#include "Common.h"
#include <functional>
#include <map>
#include <queue>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

struct Monitor {
    std::thread thread;
    bool exited { false };
    bool signalled { false };
    int status { 0 };
    int pid { 0 };
    bool autorestart { false };
    // keep this around for autorestart
    std::vector<std::string> launch_args;
    void set_status(int _status);
    void set_signalled(int _signal);
    // returns true if SIGTERM was used
    bool terminate();
};

class ServerOrganizer {
public:
    struct Client {
        struct sockaddr sock_addr;
        socklen_t addrlen;
        int socket_fd;

        std::string to_string() const;
    };

    ServerOrganizer();
    ~ServerOrganizer();

    std::string command_help(const std::vector<std::string>& args);
    std::string command_register(const std::vector<std::string>& args);
    std::string command_status(const std::vector<std::string>& args);
    std::string command_remove(const std::vector<std::string>& args);
    std::string command_autorestart(const std::vector<std::string>& args);
    std::string command_query(const std::vector<std::string>& args);
    std::string command_list(const std::vector<std::string>& args);
    std::string command_restart(const std::vector<std::string>& args);

    Message process_message(Message&& msg);

    int run();
    void run_client(Client&& client);

private:
    void restart_thread_main();
    std::string internal_register(const std::string& identifier, const std::string& executable, const std::string& working_dir, bool autorestart, const std::vector<std::string>& args);

    std::atomic_bool m_shutdown = false;
    std::map<std::string, std::function<std::string(const std::vector<std::string>&)>> m_command_function_map = {
        { "help", { [this](const auto& vec) -> std::string { return command_help(vec); } } },
        { "list", { [this](const auto& vec) -> std::string { return command_list(vec); } } },
        { "status", { [this](const auto& vec) -> std::string { return command_status(vec); } } },
        { "register", { [this](const auto& vec) -> std::string { return command_register(vec); } } },
        { "remove", { [this](const auto& vec) -> std::string { return command_remove(vec); } } },
        { "autorestart", { [this](const auto& vec) -> std::string { return command_autorestart(vec); } } },
        { "query", { [this](const auto& vec) -> std::string { return command_query(vec); } } },
        { "restart", { [this](const auto& vec) -> std::string { return command_restart(vec); } } },
    };
    std::map<std::string, Monitor> m_monitors;
    std::queue<std::vector<std::string>> m_restart_queue;
};

#endif //SERVERORGANIZER_SERVERORGANIZER_H
