#ifndef SERVERORGANIZER_COMMON_H
#define SERVERORGANIZER_COMMON_H

#include <array>
#include <ctime>
#include <string>

static constexpr auto SOCKET_FILENAME = "sohs_socket_1_0";

std::string generate_logfile_name();
std::string get_date_time_string();

struct Message {
    char data[1024] {};

    Message();
    std::array<char, 1024> serialize();
    static Message deserialize(const std::array<char, 1024>& array);
    static Message from_string(const std::string& str);
    std::string to_string();
};

#endif //SERVERORGANIZER_COMMON_H
