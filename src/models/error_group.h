#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "models/log_entry.h"
#include "models/stack_trace.h"

namespace minesleuth {

struct ErrorGroup {
    std::string signature;
    std::string exceptionClass;
    std::string displayMessage;
    std::string messageTemplate;
    std::optional<std::string> topFrame;
    std::optional<std::string> logger;
    LogLevel severity = LogLevel::ERROR;
    std::size_t count = 0;
    std::vector<std::size_t> entryIndices;
    std::optional<std::string> firstTimestamp;
    std::optional<std::string> lastTimestamp;
    std::optional<StackTrace> sampleTrace;
};

}  // namespace minesleuth
