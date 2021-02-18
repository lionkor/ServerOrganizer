#include "ServerOrganizer.h"
#include <cstring>

std::string ServerOrganizer::command_help(const std::vector<std::string>& args) {
    if (args.empty()) {
        return "no help available yet";
    } else {
        return "error: 'help' takes no arguments";
    }
}
std::string ServerOrganizer::command_status(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        return "no help available yet";
    } else {
        return "error: invalid usage of 'status'; usage: 'status <identifier>'";
    }
    return "";
}
void ServerOrganizer::run_client(ServerOrganizer::Client&& client) {
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
Message ServerOrganizer::process_message(Message&& msg) {
    info("got command: \"" + msg.to_string() + "\"");
    Message response {};
    auto str = msg.to_string();
    auto command = str.substr(0, str.find_first_of(' '));
    if (str == "kickme") {
        response = Message::from_string(Command::Detach);
    } else if (m_command_function_map.contains(command)) {
        response = Message::from_string(m_command_function_map.at(command)(extract_args(str)));
    } else {
        response = Message::from_string("unknown command");
    }
    return response;
}
ServerOrganizer::ServerOrganizer() {
}

int ServerOrganizer::run() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr {
        AF_UNIX, { }
    };
    info("socket created");
    strncpy(addr.sun_path, SOCKET_FILENAME, sizeof(addr.sun_path) - 1);
    int ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret != 0) {
        error("failed to bind: " + std::string(std::strerror(errno)) + " - this is usually caused by the server not shutting down properly. use --clean to force start.");
        return -1;
    }
    info("socket bound");
    std::vector<std::thread> clients;
    while (!m_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        listen(fd, 1);
        Client client {};
        client.socket_fd = accept(fd, &client.sock_addr, &client.addrlen);
        if (client.socket_fd < 0) {
            error("could not accept(): " + std::string(std::strerror(errno)));
        }
        std::thread& t = clients.emplace_back(&ServerOrganizer::run_client, this, client);
        t.detach();
    }
}

ServerOrganizer::~ServerOrganizer() {
    unlink(SOCKET_FILENAME);
}

std::string ServerOrganizer::command_register(const std::vector<std::string>& args) {
    return "registered";
}
