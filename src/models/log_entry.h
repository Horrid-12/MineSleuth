#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "models/stack_trace.h"

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

inline const char* toString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

struct LogEntry {
    std::string timestamp;
    LogLevel level = LogLevel::UNKNOWN;
    std::string thread;
    std::string source;
    std::string message;
    std::string raw_line;
    std::size_t line_number = 0;
    std::optional<StackTrace> trace;
};

}  // namespace minesleuth
