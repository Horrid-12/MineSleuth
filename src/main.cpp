#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

#include "analyzer/log_analyzer.h"
#include "models/analysis_result.h"

namespace {

constexpr std::size_t kMaxGroupsShown = 15;
constexpr std::size_t kMaxSuggestionWidth = 96;

std::string truncate(const std::string& text, std::size_t limit) {
    std::string out;
    for (char c : text) {
        if (c == '\n') {
            out.push_back(' ');
            continue;
        }
        out.push_back(c);
        if (out.size() >= limit) {
            return out + "...";
        }
    }
    return out;
}

void printSummary(const std::filesystem::path& file_path,
                  const minesleuth::AnalysisResult& result) {
    const minesleuth::LogStatistics& stats = result.statistics;
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════╗\n";
    std::cout << "║          MINESLEUTH              ║\n";
    std::cout << "╠══════════════════════════════════╣\n";
    std::cout << "║ File: " << std::left << std::setw(27)
              << file_path.filename().string() << "║\n";
    std::cout << "║ Entries:              " << std::right << std::setw(10)
              << stats.total_entries << " ║\n";
    std::cout << "║ Errors:               " << std::right << std::setw(10) << stats.errors
              << " ║\n";
    std::cout << "║ Warnings:             " << std::right << std::setw(10) << stats.warnings
              << " ║\n";
    std::cout << "║ Fatal exceptions:     " << std::right << std::setw(10) << stats.fatals
              << " ║\n";
    std::cout << "╚══════════════════════════════════╝\n";
    std::cout << "Format: " << minesleuth::toString(result.format)
              << " | Stack traces: " << stats.stack_traces
              << " | Exceptions extracted: " << result.exceptions.size() << "\n";
}

void printCrashContext(const minesleuth::AnalysisResult& result) {
    if (!result.crashReport.has_value()) {
        return;
    }
    const minesleuth::CrashReport& report = *result.crashReport;
    std::cout << "\nCrash report\n";
    if (!report.description.empty()) {
        std::cout << "  Description: " << truncate(report.description, 200) << "\n";
    }
    for (const auto& detail : report.systemDetails) {
        std::cout << "  " << detail.first << ": " << truncate(detail.second, 160) << "\n";
    }
    if (!report.mods.empty()) {
        std::cout << "  Mods (" << report.mods.size() << "):";
        std::size_t shown = 0;
        for (const minesleuth::ModInfo& mod : report.mods) {
            if (shown >= 12) {
                std::cout << " ...";
                break;
            }
            std::cout << " " << mod.id;
            if (!mod.version.empty()) {
                std::cout << " " << mod.version;
            }
            ++shown;
        }
        std::cout << "\n";
    }
}

void printGroups(const minesleuth::AnalysisResult& result) {
    std::size_t occurrences = 0;
    for (const minesleuth::ErrorGroup& group : result.groups) {
        occurrences += group.count;
    }

    std::cout << "\nGrouped errors (" << result.groups.size() << " unique, " << occurrences
              << " occurrences)\n";
    if (result.groups.empty()) {
        std::cout << "  No repeated or failed operations were detected.\n";
        return;
    }

    std::size_t shown = 0;
    for (const minesleuth::ErrorGroup& group : result.groups) {
        if (shown >= kMaxGroupsShown) {
            std::cout << "  ... and " << (result.groups.size() - shown) << " more groups\n";
            break;
        }
        ++shown;

        std::cout << "  " << shown << ". [x" << group.count << "] "
                  << minesleuth::toString(group.severity) << "  ";
        // The group message may already start with the exception class (for
        // example when the message was itself the trace header).
        const bool messageHasClass =
            !group.exceptionClass.empty() &&
            group.displayMessage.compare(0, group.exceptionClass.size(),
                                          group.exceptionClass) == 0;
        if (!group.exceptionClass.empty() && !messageHasClass) {
            std::cout << group.exceptionClass;
            if (!group.displayMessage.empty()) {
                std::cout << ": " << truncate(group.displayMessage, 90);
            }
        } else {
            std::cout << truncate(group.displayMessage, 100);
        }
        std::cout << "\n     ";

        if (!group.entryIndices.empty() && !result.entries.empty()) {
            const minesleuth::LogEntry& first = result.entries[group.entryIndices.front()];
            std::cout << "line " << first.line_number;
            if (!first.timestamp.empty()) {
                std::cout << " at " << first.timestamp;
            }
        }
        if (group.topFrame.has_value()) {
            std::cout << " | top frame: " << *group.topFrame;
        }
        std::cout << "\n";
    }
}

void printDiagnostics(const minesleuth::AnalysisResult& result) {
    std::cout << "\nDiagnostics (" << result.diagnostics.size() << ")\n";
    if (result.diagnostics.empty()) {
        std::cout << "  No known-issue rules matched this input.\n";
        return;
    }

    for (const minesleuth::Diagnostic& diagnostic : result.diagnostics) {
        std::cout << "  [" << minesleuth::toString(diagnostic.severity) << "/"
                  << minesleuth::toString(diagnostic.confidence) << "] " << diagnostic.ruleId
                  << " - " << diagnostic.title;
        std::cout << " (" << diagnostic.sourceLabel << " line " << diagnostic.sourceLine;
        if (diagnostic.occurrences > 1) {
            std::cout << ", " << diagnostic.occurrences << " occurrences";
        }
        std::cout << ")\n";
        if (!diagnostic.evidence.empty()) {
            std::cout << "      Evidence: " << truncate(diagnostic.evidence, 200) << "\n";
        }
        if (diagnostic.mod.identified) {
            std::cout << "      Mod:     " << truncate(diagnostic.mod.label(), 120) << " ("
                      << diagnostic.mod.basisText() << ")\n";
        }
        if (!diagnostic.explanation.empty()) {
            std::cout << "      Because:  " << truncate(diagnostic.explanation, 300) << "\n";
        }
        for (const std::string& suggestion : diagnostic.suggestions) {
            std::cout << "      Fix:      - " << truncate(suggestion, kMaxSuggestionWidth) << "\n";
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_minecraft_log_or_crash_report>\n";
        return 1;
    }

    const std::filesystem::path input_path(argv[1]);
    minesleuth::LogAnalyzer analyzer;
    const minesleuth::AnalysisResult result = analyzer.analyzeFile(input_path);

    if (!result.ok) {
        std::cerr << "Analysis failed: " << result.error << "\n";
        return 1;
    }

    printSummary(input_path, result);
    printCrashContext(result);
    printGroups(result);
    printDiagnostics(result);
    return 0;
}
