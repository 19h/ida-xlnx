#pragma once
#include <fstream>
#include <string>
inline void log_msg(const std::string& msg) {
    std::ofstream out("/tmp/loader_debug.log", std::ios::app);
    out << msg;
}
