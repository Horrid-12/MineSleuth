#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

#include "models/analysis_result.h"
#include "models/crash_report.h"
#include "models/diagnostic.h"
#include "models/error_group.h"
#include "models/log_entry.h"

namespace minesleuth {

struct RuleContext {
    const std::vector<LogEntry>& entries;
    const std::vector<ErrorGroup>& groups;
    const std::vector<ExceptionOccurrence>& exceptions;
    const CrashReport* crashReport = nullptr;
    std::unordered_set<std::size_t> matchedEntryIndices;
};

class DiagnosticRule {
public:
    virtual ~DiagnosticRule() = default;

    virtual std::string id() const = 0;
    virtual std::vector<Diagnostic> evaluate(const RuleContext& context) const = 0;

    // Fallback rules run after the others and only see unmatched entries.
    virtual bool isFallback() const { return false; }

    // Rules that only observe repetition (rather than explain a cause) must not
    // suppress the fallback diagnosis for the entries they touch.
    virtual bool attributesCause() const { return true; }
};

}  // namespace minesleuth
