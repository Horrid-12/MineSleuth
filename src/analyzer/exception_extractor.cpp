#include "analyzer/exception_extractor.h"

#include <cctype>

namespace minesleuth {

bool ExceptionExtractor::isPlausibleException(const StackTrace& trace) {
    if (trace.empty()) {
        return false;
    }
    for (const TraceSegment& segment : trace.segments) {
        if (!segment.frames.empty() || segment.omittedFrames > 0) {
            return true;
        }
    }
    const TraceSegment* root = trace.rootSegment();
    if (root == nullptr) {
        return false;
    }
    if (trace.segments.size() > 1) {
        return true;
    }
    const std::string& name = root->exceptionClass;
    const auto hasSuffix = [&name](const char* suffix) {
        const std::size_t length = std::char_traits<char>::length(suffix);
        if (name.size() < length) {
            return false;
        }
        for (std::size_t i = 0; i < length; ++i) {
            const char a = name[name.size() - length + i];
            const char b = suffix[i];
            if (std::tolower(static_cast<unsigned char>(a)) !=
                std::tolower(static_cast<unsigned char>(b))) {
                return false;
            }
        }
        return true;
    };
    return hasSuffix("Exception") || hasSuffix("Error") || hasSuffix("Throwable") ||
           hasSuffix("Failure") || hasSuffix("Problem");
}

bool isProblemEntry(const LogEntry& entry) {
    switch (entry.level) {
        case LogLevel::WARN:
        case LogLevel::ERROR:
        case LogLevel::FATAL:
            return true;
        default:
            break;
    }
    return entry.trace.has_value() && ExceptionExtractor::isPlausibleException(*entry.trace);
}

std::vector<ExceptionOccurrence> ExceptionExtractor::extract(
    const std::vector<LogEntry>& entries) const {
    std::vector<ExceptionOccurrence> occurrences;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].trace.has_value()) {
            continue;
        }
        if (!isPlausibleException(*entries[i].trace)) {
            continue;
        }
        occurrences.push_back(ExceptionOccurrence{i, *entries[i].trace});
    }
    return occurrences;
}

}  // namespace minesleuth
