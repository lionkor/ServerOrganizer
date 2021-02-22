#ifndef SERVERORGANIZER_COMMON_H
#define SERVERORGANIZER_COMMON_H

#include <algorithm>
#include <array>
#include <ctime>
#include <iterator>
#include <string>

static constexpr auto SOCKET_FILENAME = "/tmp/.sohs_socket_1_0";

std::string generate_logfile_name(const std::string& prefix);
std::string get_date_time_string();
std::string get_time_string();

struct Message {
    char data[1024] {};

    Message();
    std::array<char, 1024> serialize();
    static Message deserialize(const std::array<char, 1024>& array);
    static Message from_string(const std::string& str);
    std::string to_string();
};

namespace Command {
static inline const std::string Detach = "_do_detach_now";
}

// from https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
// trim from start (in place)
void ltrim(std::string& s);
// trim from end (in place)
void rtrim(std::string& s);
// trim from both ends (in place)
void trim(std::string& s);
// trim from start (copying)
std::string ltrim_copy(std::string s);
// trim from end (copying)
std::string rtrim_copy(std::string s);
// trim from both ends (copying)
std::string trim_copy(std::string s);

// original
std::vector<std::string> extract_args(const std::string& command);

// from http://www.martinbroadhurst.com/how-to-split-a-string-in-c.html
template<class Container>
void split(const std::string& str, Container& cont, char delim = ' ') {
    std::size_t current, previous = 0;
    current = str.find(delim);
    while (current != std::string::npos) {
        cont.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find(delim, previous);
    }
    cont.push_back(str.substr(previous, current - previous));
}

// these are shared interfaces, implemented differently on client- and server-side
void error(const std::string& str);
void warn(const std::string& str);
void info(const std::string& str);

#endif //SERVERORGANIZER_COMMON_H
