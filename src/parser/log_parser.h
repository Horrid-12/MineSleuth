#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "../models/log_entry.h"

namespace minesleuth {

struct LogStatistics {
    size_t total_entries = 0;
    size_t errors = 0;
    size_t warnings = 0;
    size_t infos = 0;
    size_t fatals = 0;
};

class LogParser {
public:
    LogParser() = default;
    
    bool parseFile(const std::filesystem::path& file_path);
    
    const std::vector<LogEntry>& getEntries() const { return entries_; }
    const LogStatistics& getStatistics() const { return stats_; }
    
private:
    std::vector<LogEntry> entries_;
    LogStatistics stats_;
    
    LogEntry parseLine(const std::string& line);
    LogLevel parseLevel(const std::string& level_str);
};

} // namespace minesleuth
