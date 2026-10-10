#pragma once

#include <cstddef>
#include <vector>

#include "models/analysis_result.h"
#include "models/log_entry.h"
#include "models/stack_trace.h"

namespace minesleuth {

class ExceptionExtractor {
public:
    std::vector<ExceptionOccurrence> extract(const std::vector<LogEntry>& entries) const;

    static bool isPlausibleException(const StackTrace& trace);
};

// True for entries that describe a problem: elevated severity or a real stack trace.
bool isProblemEntry(const LogEntry& entry);

}  // namespace minesleuth
