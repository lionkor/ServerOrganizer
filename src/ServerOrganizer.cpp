#include "ServerOrganizer.h"
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <sys/wait.h>

static const std::string help_str = "list of all commands:\n"
                                    "* help - displays this help\n"
                                    "* status <identifier> - displays the status of a worker\n"
                                    "* list - displays a list of all workers\n"
                                    "* register <identifier> <executable-path> [working-dir]- registers a new worker\n"
                                    "* remove <identifier> - removes the worker, SIGTERMs it if it's still running\n"
                                    "* autorestart <identifier> <on/off> - turns autorestart on crash/exit on or off\n"
                                    "* query <identifer> <key> - querys the worker for a value. possible keys are `pid`, `status`, `autorestart`, `exited`, `signalled`. The return values for `query` are made to be easily machine-readable.\n"
                                    "* restart <identifier> - restarts the given worker. Will SIGTERM/SIGKILL if the worker is still running.";

std::string ServerOrganizer::command_help(const std::vector<std::string>& args) {
    if (args.empty()) {
        return help_str;
    } else {
        return "`help` takes no arguments";
    }
}

std::string ServerOrganizer::command_status(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        if (m_monitors.contains(args.at(0))) {
            auto& monitor = m_monitors.at(args.at(0));
            if (monitor.exited) {
                return "\"" + args.at(0) + "\" exited with code " + std::to_string(monitor.status);
            } else if (monitor.signalled) {
                return "\"" + args.at(0) + "\" exited via " + std::string(strsignal(monitor.status));
            } else {
                return "\"" + args.at(0) + "\" is running";
            }
        } else {
            return "worker \"" + args.at(0) + "\" unknown";
        }
    } else {
        return "usage: 'status <identifier>'";
    }
}

std::string ServerOrganizer::command_register(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return "invalid arguments, expected at least `identifier` and `executable-path` arguments";
    }
    if (m_monitors.contains(args.at(0))) {
        return "identifier \"" + args.at(0) + "\" is already used";
    }
}

void ServerOrganizer::run_client(ServerOrganizer::Client&& client) {
    info("client connected");
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
    info("ServerOrganizer v1.0 Headless Server");
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
    std::thread restart_thread(&ServerOrganizer::restart_thread_main, this);
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
    restart_thread.join();
}

ServerOrganizer::~ServerOrganizer() {
    unlink(SOCKET_FILENAME);
}
std::string ServerOrganizer::command_remove(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return "`remove` expects argument `identifier`";
    }
    auto name = args.at(0);
    if (m_monitors.contains(name)) {
        bool sigtermed = m_monitors.at(name).terminate();
        int status = m_monitors.at(name).status;
        m_monitors.erase(name);
        if (sigtermed) {
            return "worker \"" + args.at(0) + "\" was still running, so it was terminated with SIGTERM/SIGKILL and then removed";
        } else {
            return "worker \"" + args.at(0) + "\" removed";
        }
    } else {
        return "worker \"" + args.at(0) + "\" not found, nothing removed";
    }
}
std::string ServerOrganizer::command_autorestart(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        return "`autorestart` takes arguments `identifier` and `on/off`";
    }
    auto name = args.at(0);
    if (m_monitors.contains(name)) {
        auto onoff = args.at(1);
        if (onoff == "on") {
            m_monitors.at(name).autorestart = true;
            return "autorestart turned ON for worker \"" + name + "\"";
        } else if (onoff == "off") {
            m_monitors.at(name).autorestart = false;
            return "autorestart turned OFF for worker \"" + name + "\"";
        } else {
            return R"(argument `on/off` expects either "on" or "off" (no quotes))";
        }
    } else {
        return "worker \"" + name + "\"" + " not found";
    }
}
void ServerOrganizer::restart_thread_main() {
    while (!m_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (m_restart_queue.empty()) {
            continue;
        } else {
            auto value = m_restart_queue.front();
            m_restart_queue.pop();
            command_remove({ value.at(0) });
            command_register(value);
        }
    }
}
std::string ServerOrganizer::command_query(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        return "ERROR - invalid arguments";
    }
    if (!m_monitors.contains(args.at(0))) {
        return "ERROR - unknown worker";
    }
    auto& monitor = m_monitors.at(args.at(0));
    auto key = args.at(1);
    if (key == "pid") {
        return std::to_string(monitor.pid);
    } else if (key == "status") {
        return std::to_string(monitor.status);
    } else if (key == "exited") {
        return monitor.exited ? "true" : "false";
    } else if (key == "signalled") {
        return monitor.signalled ? "true" : "false";
    } else if (key == "autorestart") {
        return monitor.autorestart ? "true" : "false";
    } else {
        return "ERROR - unknown key";
    }
}
std::string ServerOrganizer::command_list(const std::vector<std::string>& args) {
    if (!args.empty()) {
        return "`list` takes no arguments";
    }
    std::stringstream list;
    list << "list of all workers:\n";
    for (const auto& pair : m_monitors) {
        list << pair.first;
        if (pair.second.exited) {
            list << " (exited code " << pair.second.status << ")";
        } else if (pair.second.signalled) {
            list << " (exited via " << strsignal(pair.second.status) << ")";
        } else {
            list << " (running)";
        }
        list << "\n";
    }
    auto result = list.str();
    result.erase(result.size() - 1);
    return result;
}
std::string ServerOrganizer::command_restart(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return "`restart` only takes one argument `identifier`";
    }
    auto name = args.at(0);
    if (m_monitors.contains(name)) {
        m_restart_queue.push(m_monitors.at(name).launch_args);
        return "queued \"" + name + "\" to be restarted";
    } else {
        return "worker \"" + name + "\" unknown";
    }
}
std::string ServerOrganizer::internal_register(const std::string& identifier, const std::string& executable, const std::string& working_dir, bool autorestart, const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == 0) {
        // child
        if (args.size() == 3) {
            int ret = chdir(args.at(2).c_str());
            if (ret != 0) {
                exit(55);
            }
        }

        constexpr char temp_dir[] = "/tmp/ServerOrganizer";
        // write this file to /tmp/ServerOrganizer/<identifier>.log
        const std::string file_path = std::string(temp_dir) + "/" + args.at(0) + ".log";
        struct stat st { };
        // first ensure that the directory exists
        if (stat(temp_dir, &st) != 0) {
            int ret = mkdir(temp_dir, 0700);
            if (ret != 0) {
                error("mkdir failed: " + std::string(strerror(errno)));
                exit(-1);
            }
        } else {
            // directory exists, so let's see if the file exists
            // and delete it if it does
            if (stat(file_path.c_str(), &st) != 0) {
                int ret = unlink(file_path.c_str());
                if (ret != 0) {
                    // this isn't fatal, don't exit
                    warn("unlink failed: " + std::string(strerror(errno)));
                }
            }
        }
        // now we grab a file descriptor to this file
        int fd = open(file_path.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            error("open failed: " + std::string(strerror(errno)));
            exit(-1);
        }
        // replace stdout's filedescriptor with the file's one
        int ret = dup2(fd, STDOUT_FILENO);
        if (ret == -1) {
            error("dup2 for stdout failed: " + std::string(strerror(errno)));
            exit(-1);
        }
        ret = dup2(fd, STDERR_FILENO);
        if (ret == -1) {
            error("dup2 for stderr failed: " + std::string(strerror(errno)));
            exit(-1);
        }
        ret = close(fd);
        if (ret != 0) {
            // not fatal, don't handle at all since we don't have stdout anymore
        }
        auto name = args.at(1).c_str();
        execl(name, name, nullptr);
        exit(0);
    } else {
        // still in the parent
        auto [iter_value_pair, replaced] = m_monitors.insert({ args.at(0), Monitor {} });
        auto& monitor = iter_value_pair->second;
        monitor.pid = pid;
        monitor.launch_args = args;
        monitor.thread = std::thread([&] {
            while (true) {
                waitpid(pid, &monitor.status, 0);
                if (WIFEXITED(monitor.status)) {
                    monitor.set_status(WEXITSTATUS(monitor.status));
                    break;
                } else if (WIFSIGNALED(monitor.status)) {
                    monitor.set_signalled(WTERMSIG(monitor.status));
                    break;
                } else if (WIFSTOPPED(monitor.status) || WIFCONTINUED(monitor.status)) {
                    // we dont care
                    continue;
                }
            }
            if (monitor.autorestart) {
                m_restart_queue.push(monitor.launch_args);
            }
        });
        monitor.thread.detach();
    }
    info("started new process (pid " + std::to_string(pid) + ") as " + args.at(0));
    // parent
    return "registered \"" + args.at(0) + "\"";
}
void Monitor::set_status(int _status) {
    signalled = false;
    exited = true;
    status = _status;
}
void Monitor::set_signalled(int _signal) {
    exited = false;
    signalled = true;
    status = _signal;
}
bool Monitor::terminate() {
    if (!exited && !signalled) {
        int ret = kill(pid, SIGTERM);
        if (ret != 0) {
            error("kill(" + std::to_string(pid) + ", SIGTERM) failed: " + std::string(std::strerror(errno)));
            warn("SIGKILL will now be used in another attempt to stop " + std::to_string(pid));
            ret = kill(pid, SIGKILL);
            if (ret != 0) {
                error("kill(" + std::to_string(pid) + ", SIGTERM) failed: " + std::string(std::strerror(errno)));
            }
        }
        // the pid is waited on in the monitor's thread and exited / signalled statuses
        // are set by it, too, and we just expect it to do the job instead of setting it
        // here.
        return true;
    }
    return false;
}
