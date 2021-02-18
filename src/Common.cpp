#include "Common.h"

std::string get_date_time_string() {
    time_t now = time(nullptr);
    struct tm tstruct { };
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
    return buf;
}
std::string generate_logfile_name(const std::string& prefix) {
    time_t now = time(nullptr);
    struct tm tstruct { };
    char buf[128];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), (prefix + "_%F_%H%M%S.log").c_str(), &tstruct);
    return std::string(buf);
}
std::vector<std::string> extract_args(const std::string& command) {
    std::vector<std::string> args;
    split(command, args, ' ');
    args.erase(args.begin());
    return args;
}
std::string trim_copy(std::string s) {
    trim(s);
    return s;
}
std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}
std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}
void trim(std::string& s) {
    ltrim(s);
    rtrim(s);
}
void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(),
        s.end());
}
void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
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
