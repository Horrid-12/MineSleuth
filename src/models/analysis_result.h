#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "models/crash_report.h"
#include "models/diagnostic.h"
#include "models/error_group.h"
#include "models/log_entry.h"
#include "models/log_statistics.h"
#include "models/mod_registry.h"

namespace minesleuth {

enum class InputFormat {
    LogFile,
    CrashReport,
    Unknown
};

inline const char* toString(InputFormat format) {
    switch (format) {
        case InputFormat::LogFile: return "log file";
        case InputFormat::CrashReport: return "crash report";
        case InputFormat::Unknown: return "unknown";
    }
    return "unknown";
}

struct ExceptionOccurrence {
    std::size_t entryIndex = 0;
    StackTrace trace;
};

struct AnalysisResult {
    bool ok = false;
    std::string error;
    InputFormat format = InputFormat::Unknown;
    std::vector<LogEntry> entries;
    LogStatistics statistics;
    std::optional<CrashReport> crashReport;
    std::vector<ExceptionOccurrence> exceptions;
    std::vector<ErrorGroup> groups;
    std::vector<Diagnostic> diagnostics;
    ModRegistry modRegistry;
};

}  // namespace minesleuth
