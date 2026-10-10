#pragma once

#include <filesystem>

#include "diagnostics/rule_engine.h"
#include "models/analysis_result.h"
#include "models/crash_report.h"
#include "models/log_entry.h"

namespace minesleuth {

class LogAnalyzer {
public:
    LogAnalyzer();
    explicit LogAnalyzer(RuleEngine engine);

    AnalysisResult analyzeFile(const std::filesystem::path& file_path) const;
    AnalysisResult analyzeEntries(std::vector<LogEntry> entries) const;
    AnalysisResult analyzeCrashReport(const CrashReport& report) const;

    const RuleEngine& engine() const { return engine_; }

private:
    void finalize(AnalysisResult& result) const;

    RuleEngine engine_;
};

}  // namespace minesleuth
