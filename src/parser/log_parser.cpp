#include "log_parser.h"
#include <fstream>
#include <iostream>
#include <regex>

namespace minesleuth {

LogLevel LogParser::parseLevel(const std::string& level_str) {
    if (level_str == "INFO") return LogLevel::INFO;
    if (level_str == "WARN") return LogLevel::WARN;
    if (level_str == "ERROR") return LogLevel::ERROR;
    if (level_str == "FATAL") return LogLevel::FATAL;
    if (level_str == "DEBUG") return LogLevel::DEBUG;
    if (level_str == "TRACE") return LogLevel::TRACE;
    return LogLevel::UNKNOWN;
}

LogEntry LogParser::parseLine(const std::string& line) {
    LogEntry entry;
    entry.raw_line = line;
    
    // Standard Minecraft log format: [HH:MM:SS] [Thread/Level]: Message
    // Or with date: [DDMMMYY HH:MM:SS.SSS] [Thread/Level] [Source]: Message
    static const std::regex log_regex(R"(\[([^\]]+)\] \[([^/\]]+)/([^\]]+)\](?: \[([^\]]+)\])?: (.*))");
    std::smatch match;
    
    if (std::regex_match(line, match, log_regex)) {
        entry.timestamp = match[1];
        entry.thread = match[2];
        entry.level = parseLevel(match[3]);
        if (match[4].matched) {
            entry.source = match[4];
        }
        entry.message = match[5];
    } else {
        // Fallback for lines that don't match exactly, e.g. stack traces
        entry.level = LogLevel::UNKNOWN;
        entry.message = line;
    }
    
    return entry;
}

bool LogParser::parseFile(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        LogEntry entry = parseLine(line);
        entries_.push_back(entry);
        
        stats_.total_entries++;
        switch (entry.level) {
            case LogLevel::INFO: stats_.infos++; break;
            case LogLevel::WARN: stats_.warnings++; break;
            case LogLevel::ERROR: stats_.errors++; break;
            case LogLevel::FATAL: stats_.fatals++; break;
            default: break;
        }
    }
    
    return true;
}

} // namespace minesleuth
