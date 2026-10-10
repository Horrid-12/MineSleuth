#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "models/log_entry.h"
#include "models/log_statistics.h"

namespace minesleuth {

class LogParser {
public:
    LogParser() = default;

    bool parseFile(const std::filesystem::path& file_path);

    const std::vector<LogEntry>& getEntries() const { return entries_; }
    const LogStatistics& getStatistics() const { return stats_; }

private:
    std::vector<LogEntry> entries_;
    LogStatistics stats_;

    bool tryParseLine(const std::string& line, LogEntry& out) const;
    LogLevel parseLevel(const std::string& level_str) const;
    void addEntry(LogEntry entry);
};

}  // namespace minesleuth
