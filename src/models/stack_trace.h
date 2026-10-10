#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace minesleuth {

struct StackFrame {
    std::string raw;
    std::optional<std::string> className;
    std::optional<std::string> methodName;
    std::optional<std::string> sourceFile;
    std::optional<int> lineNumber;
    bool nativeMethod = false;

    std::string signature() const {
        if (!className.has_value()) {
            return {};
        }
        std::string out = *className;
        if (methodName.has_value()) {
            out += '.';
            out += *methodName;
        }
        return out;
    }
};

enum class TraceSegmentKind {
    Root,
    Cause,
    Suppressed
};

inline const char* toString(TraceSegmentKind kind) {
    switch (kind) {
        case TraceSegmentKind::Root: return "root";
        case TraceSegmentKind::Cause: return "caused by";
        case TraceSegmentKind::Suppressed: return "suppressed";
    }
    return "unknown";
}

struct TraceSegment {
    TraceSegmentKind kind = TraceSegmentKind::Root;
    std::string exceptionClass;
    std::string message;
    std::string headerRaw;
    std::vector<StackFrame> frames;
    int omittedFrames = 0;
    int depth = 0;

    std::string header() const {
        if (message.empty()) {
            return exceptionClass;
        }
        return exceptionClass + ": " + message;
    }
};

struct StackTrace {
    std::vector<TraceSegment> segments;
    std::size_t startLine = 0;
    std::size_t endLine = 0;

    // Every source line consumed by the parser except a header line that was
    // seeded from an existing log message (that text lives in LogEntry::message).
    std::string rawText;

    bool empty() const { return segments.empty(); }

    const TraceSegment* rootSegment() const {
        for (const TraceSegment& segment : segments) {
            if (segment.kind == TraceSegmentKind::Root) {
                return &segment;
            }
        }
        return segments.empty() ? nullptr : &segments.front();
    }

    std::string rootExceptionClass() const {
        const TraceSegment* root = rootSegment();
        return root == nullptr ? std::string{} : root->exceptionClass;
    }

    std::string rootMessage() const {
        const TraceSegment* root = rootSegment();
        return root == nullptr ? std::string{} : root->message;
    }

    std::string rootHeader() const {
        const TraceSegment* root = rootSegment();
        if (root == nullptr) {
            return {};
        }
        return root->headerRaw.empty() ? root->header() : root->headerRaw;
    }

    std::string topFrame() const {
        const TraceSegment* root = rootSegment();
        if (root != nullptr) {
            for (const StackFrame& frame : root->frames) {
                std::string signature = frame.signature();
                if (!signature.empty()) {
                    return signature;
                }
            }
        }
        for (const TraceSegment& segment : segments) {
            for (const StackFrame& frame : segment.frames) {
                std::string signature = frame.signature();
                if (!signature.empty()) {
                    return signature;
                }
            }
        }
        return {};
    }

    std::size_t frameCount() const {
        std::size_t total = 0;
        for (const TraceSegment& segment : segments) {
            total += segment.frames.size();
        }
        return total;
    }

    bool containsException(const std::string& exceptionClass) const {
        for (const TraceSegment& segment : segments) {
            if (segment.exceptionClass == exceptionClass) {
                return true;
            }
        }
        return false;
    }
};

}  // namespace minesleuth
