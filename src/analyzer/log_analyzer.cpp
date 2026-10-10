#include "analyzer/log_analyzer.h"

#include <utility>

#include "analyzer/error_grouper.h"
#include "analyzer/exception_extractor.h"
#include "analyzer/mod_attribution.h"
#include "parser/crash_report_parser.h"
#include "parser/input_detector.h"
#include "parser/log_parser.h"

namespace minesleuth {

LogAnalyzer::LogAnalyzer() : engine_(RuleEngine::withDefaultRules()) {}

LogAnalyzer::LogAnalyzer(RuleEngine engine) : engine_(std::move(engine)) {}

void LogAnalyzer::finalize(AnalysisResult& result) const {
    ExceptionExtractor extractor;
    result.exceptions = extractor.extract(result.entries);

    ErrorGrouper grouper;
    result.groups = grouper.group(result.entries);
    const CrashReport* report = result.crashReport.has_value() ? &*result.crashReport : nullptr;
    result.modRegistry = ModAttributor::buildRegistry(result.entries, report);
    result.diagnostics = engine_.evaluate(result.entries, result.groups, result.exceptions, report,
                                          &result.modRegistry);
    result.ok = true;
}

AnalysisResult LogAnalyzer::analyzeEntries(std::vector<LogEntry> entries) const {
    AnalysisResult result;
    result.format = InputFormat::LogFile;
    result.statistics.total_entries = entries.size();
    for (const LogEntry& entry : entries) {
        switch (entry.level) {
            case LogLevel::INFO: result.statistics.infos++; break;
            case LogLevel::WARN: result.statistics.warnings++; break;
            case LogLevel::ERROR: result.statistics.errors++; break;
            case LogLevel::FATAL: result.statistics.fatals++; break;
            default: break;
        }
        if (entry.trace.has_value() && !entry.trace->empty()) {
            result.statistics.stack_traces++;
        }
    }
    result.entries = std::move(entries);
    finalize(result);
    return result;
}

AnalysisResult LogAnalyzer::analyzeCrashReport(const CrashReport& report) const {
    AnalysisResult result;
    result.format = InputFormat::CrashReport;
    result.crashReport = report;

    for (const StackTrace& trace : report.exceptions) {
        LogEntry entry;
        entry.level = LogLevel::ERROR;
        entry.line_number = trace.startLine;
        entry.raw_line = trace.rootHeader();
        entry.message = trace.rootHeader();
        entry.trace = trace;
        result.statistics.total_entries++;
        result.statistics.errors++;
        result.statistics.stack_traces++;
        result.entries.push_back(std::move(entry));
    }

    finalize(result);
    return result;
}

AnalysisResult LogAnalyzer::analyzeFile(const std::filesystem::path& file_path) const {
    const InputFormat detected = detectInputFormat(file_path);

    if (detected == InputFormat::CrashReport) {
        CrashReportParser crashParser;
        CrashReport report = crashParser.parseFile(file_path);
        if (report.valid) {
            return analyzeCrashReport(report);
        }
        // Detection matched but the report is unusable: fall through to log parsing.
    }

    LogParser parser;
    if (!parser.parseFile(file_path)) {
        AnalysisResult failed;
        failed.format = detected;
        failed.error = "Could not open file: " + file_path.string();
        return failed;
    }

    AnalysisResult result;
    result.format = InputFormat::LogFile;
    result.entries = parser.getEntries();
    result.statistics = parser.getStatistics();

    if (result.entries.empty()) {
        result.format = InputFormat::Unknown;
        result.error = "No log entries could be extracted (the file is empty): " +
                       file_path.string();
        return result;
    }

    finalize(result);
    return result;
}

}  // namespace minesleuth
