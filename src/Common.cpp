#include "Common.h"

std::string get_date_time_string() {
    time_t now = time(nullptr);
    struct tm tstruct { };
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
    return buf;
}
std::string generate_logfile_name() {
    time_t now = time(nullptr);
    struct tm tstruct { };
    char buf[128];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "ServerOrganizer_HeadlessServer_%F_%H%M%S.log", &tstruct);
    return std::string(buf);
}
std::array<char, sizeof(Message)> Message::serialize() {
    return std::to_array(data);
}
Message Message::deserialize(const std::array<char, 1024>& array) {
    Message msg {};
    std::copy(array.begin(), array.end(), std::begin(msg.data));
    return msg;
}
std::string Message::to_string() {
    return data;
}
Message::Message() {
    std::fill(std::begin(data), std::end(data), '\0');
}
Message Message::from_string(const std::string& str) {
    Message msg {};
    std::copy_n(str.begin(), std::min<size_t>(str.size(), 1024), std::begin(msg.data));
    return msg;
}
