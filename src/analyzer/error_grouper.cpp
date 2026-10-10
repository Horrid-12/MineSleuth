#include "analyzer/error_grouper.h"

#include <algorithm>
#include <regex>
#include <unordered_map>

#include "analyzer/exception_extractor.h"
#include "utils/strings.h"

namespace minesleuth {
namespace {

int levelRank(LogLevel level) {
    switch (level) {
        case LogLevel::FATAL: return 6;
        case LogLevel::ERROR: return 5;
        case LogLevel::WARN: return 4;
        case LogLevel::INFO: return 3;
        case LogLevel::DEBUG: return 2;
        case LogLevel::TRACE: return 1;
        default: return 0;
    }
}

}  // namespace

std::string ErrorGrouper::normalizeMessage(const std::string& message) {
    static const std::regex uuidRegex(
        R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
    static const std::regex pathRegex(
        R"([^\s,;:'"()<>|]*[\\/][^\s,;:'"()<>|]*\.[A-Za-z0-9]+)");
    static const std::regex hexRegex(R"(0x[0-9a-fA-F]+)");
    static const std::regex digitRegex(R"(\d+)");

    std::string out = message;
    out = std::regex_replace(out, uuidRegex, "<uuid>");
    out = std::regex_replace(out, pathRegex, "<path>");
    out = std::regex_replace(out, hexRegex, "<addr>");
    out = std::regex_replace(out, digitRegex, "#");
    return strings::collapseWhitespace(out);
}

std::string ErrorGrouper::signatureFor(const LogEntry& entry) {
    std::string exceptionClass;
    std::string topFrame;
    if (entry.trace.has_value() && ExceptionExtractor::isPlausibleException(*entry.trace)) {
        exceptionClass = entry.trace->rootExceptionClass();
        topFrame = entry.trace->topFrame();
    }
    std::string signature = strings::toLower(exceptionClass);
    signature.push_back('|');
    signature += strings::toLower(normalizeMessage(entry.message));
    signature.push_back('|');
    signature += strings::toLower(topFrame);
    return signature;
}

std::vector<ErrorGroup> ErrorGrouper::group(const std::vector<LogEntry>& entries) const {
    std::vector<ErrorGroup> groups;
    std::unordered_map<std::string, std::size_t> index;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const LogEntry& entry = entries[i];
        if (!isProblemEntry(entry)) {
            continue;
        }

        const std::string signature = signatureFor(entry);
        auto [it, inserted] = index.try_emplace(signature, groups.size());
        if (inserted) {
            ErrorGroup fresh;
            fresh.signature = signature;
            fresh.displayMessage = entry.message;
            fresh.messageTemplate = normalizeMessage(entry.message);
            fresh.severity = entry.level;
            if (!entry.source.empty()) {
                fresh.logger = entry.source;
            }
            if (entry.trace.has_value() && ExceptionExtractor::isPlausibleException(*entry.trace)) {
                fresh.exceptionClass = entry.trace->rootExceptionClass();
                fresh.sampleTrace = *entry.trace;
                const std::string topFrame = entry.trace->topFrame();
                if (!topFrame.empty()) {
                    fresh.topFrame = topFrame;
                }
            }
            if (!entry.timestamp.empty()) {
                fresh.firstTimestamp = entry.timestamp;
                fresh.lastTimestamp = entry.timestamp;
            }
            groups.push_back(std::move(fresh));
        }

        ErrorGroup& group = groups[it->second];
        group.count++;
        group.entryIndices.push_back(i);
        if (levelRank(entry.level) > levelRank(group.severity)) {
            group.severity = entry.level;
        }
        if (!entry.timestamp.empty()) {
            group.lastTimestamp = entry.timestamp;
            if (!group.firstTimestamp.has_value()) {
                group.firstTimestamp = entry.timestamp;
            }
        }
        if (!group.sampleTrace.has_value() && entry.trace.has_value() &&
            ExceptionExtractor::isPlausibleException(*entry.trace)) {
            group.sampleTrace = *entry.trace;
            group.exceptionClass = entry.trace->rootExceptionClass();
            const std::string topFrame = entry.trace->topFrame();
            if (!topFrame.empty()) {
                group.topFrame = topFrame;
            }
        }
    }

    std::stable_sort(groups.begin(), groups.end(), [](const ErrorGroup& a, const ErrorGroup& b) {
        if (a.count != b.count) {
            return a.count > b.count;
        }
        if (levelRank(a.severity) != levelRank(b.severity)) {
            return levelRank(a.severity) > levelRank(b.severity);
        }
        return a.signature < b.signature;
    });

    return groups;
}

}  // namespace minesleuth
