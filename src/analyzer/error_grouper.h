#pragma once

#include <string>
#include <vector>

#include "models/error_group.h"
#include "models/log_entry.h"

namespace minesleuth {

class ErrorGrouper {
public:
    std::vector<ErrorGroup> group(const std::vector<LogEntry>& entries) const;

    static std::string normalizeMessage(const std::string& message);
    static std::string signatureFor(const LogEntry& entry);
};

}  // namespace minesleuth
