#pragma once
#include <string>

namespace minesleuth {

enum class LogLevel {
    INFO,
    WARN,
    ERROR,
    FATAL,
    DEBUG,
    TRACE,
    UNKNOWN
};

struct LogEntry {
    std::string timestamp;
    LogLevel level;
    std::string thread;
    std::string source;
    std::string message;
    std::string raw_line;
};

} // namespace minesleuth
