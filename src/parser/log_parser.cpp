#include "parser/log_parser.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <utility>

#include "parser/stack_trace_parser.h"

namespace minesleuth {

LogLevel LogParser::parseLevel(const std::string& level_str) const {
    if (level_str == "INFO") return LogLevel::INFO;
    if (level_str == "WARN") return LogLevel::WARN;
    if (level_str == "ERROR") return LogLevel::ERROR;
    if (level_str == "FATAL") return LogLevel::FATAL;
    if (level_str == "DEBUG") return LogLevel::DEBUG;
    if (level_str == "TRACE") return LogLevel::TRACE;
    return LogLevel::UNKNOWN;
}

bool LogParser::tryParseLine(const std::string& line, LogEntry& out) const {
    // Optional timestamp, then [Thread/Level], then an optional logger/source group.
    static const std::regex log_regex(
        R"((?:\[([^\]]+)\] )?\[([^/\]]+)/([^\]]+)\](?: \[([^\]]+)\])?: (.*))");

    std::smatch match;
    if (!std::regex_match(line, match, log_regex)) {
        return false;
    }

    out.raw_line = line;
    out.timestamp = match[1].matched ? match[1].str() : std::string{};
    out.thread = match[2];
    out.level = parseLevel(match[3]);
    out.source = match[4].matched ? match[4].str() : std::string{};
    out.message = match[5];
    return true;
}

void LogParser::addEntry(LogEntry entry) {
    if (entry.trace.has_value() && !entry.trace->empty()) {
        stats_.stack_traces++;
    }
    stats_.total_entries++;
    switch (entry.level) {
        case LogLevel::INFO: stats_.infos++; break;
        case LogLevel::WARN: stats_.warnings++; break;
        case LogLevel::ERROR: stats_.errors++; break;
        case LogLevel::FATAL: stats_.fatals++; break;
        default: break;
    }
    entries_.push_back(std::move(entry));
}

bool LogParser::parseFile(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return false;
    }

    entries_.clear();
    stats_ = LogStatistics{};

    StackTraceBuilder builder;
    std::optional<LogEntry> current;
    std::string line;
    std::size_t lineNumber = 0;

    auto finishTrace = [&]() {
        if (builder.active() && current.has_value()) {
            current->trace = builder.finish();
        }
        builder.reset();
    };
    auto pushCurrent = [&]() {
        if (current.has_value()) {
            addEntry(std::move(*current));
            current.reset();
        }
    };
    auto startStrayEntry = [&](const std::string& text, std::size_t number) {
        LogEntry stray;
        stray.raw_line = text;
        stray.message = text;
        stray.level = LogLevel::UNKNOWN;
        stray.line_number = number;
        current = std::move(stray);
    };

    while (std::getline(file, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            finishTrace();
            continue;
        }

        LogEntry parsed;
        if (tryParseLine(line, parsed)) {
            finishTrace();
            pushCurrent();
            parsed.line_number = lineNumber;
            if (StackTraceParser::looksLikeExceptionHeader(parsed.message)) {
                builder.seedRoot(parsed.message, lineNumber, false);
            }
            current = std::move(parsed);
            continue;
        }

        TraceSegment headerProbe;
        const bool startsNewTrace =
            StackTraceParser::parseExceptionHeader(line, headerProbe) &&
            headerProbe.kind == TraceSegmentKind::Root;

        if (startsNewTrace) {
            // A bare exception header right after a log line belongs to that line.
            if (current.has_value() && !current->trace.has_value() && !builder.active()) {
                builder.seedRoot(line, lineNumber, true);
                continue;
            }
            finishTrace();
            pushCurrent();
            startStrayEntry(line, lineNumber);
            // The stray entry's message is the header itself, so rawText starts after it.
            builder.seedRoot(line, lineNumber, false);
            continue;
        }

        if (builder.accept(line, lineNumber)) {
            continue;
        }

        finishTrace();
        pushCurrent();
        startStrayEntry(line, lineNumber);
    }

    finishTrace();
    pushCurrent();
    return true;
}

}  // namespace minesleuth
