#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "diagnostics/diagnostic_rule.h"
#include "models/crash_report.h"
#include "models/diagnostic.h"
#include "models/error_group.h"
#include "models/log_entry.h"
#include "models/mod_registry.h"

namespace minesleuth {

class RuleEngine {
public:
    RuleEngine() = default;
    explicit RuleEngine(std::vector<std::unique_ptr<DiagnosticRule>> rules);

    static RuleEngine withDefaultRules();

    // registry optionally supplies the mod list the analyzer already built; a
    // null pointer rebuilds one from the entries and crash report.
    std::vector<Diagnostic> evaluate(const std::vector<LogEntry>& entries,
                                     const std::vector<ErrorGroup>& groups,
                                     const std::vector<ExceptionOccurrence>& exceptions,
                                     const CrashReport* crashReport = nullptr,
                                     const ModRegistry* registry = nullptr) const;

    std::size_t ruleCount() const { return rules_.size(); }

private:
    std::vector<std::unique_ptr<DiagnosticRule>> rules_;
};

}  // namespace minesleuth
