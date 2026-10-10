#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "models/stack_trace.h"

namespace minesleuth {

class StackTraceParser {
public:
    static bool parseExceptionHeader(std::string_view line, TraceSegment& out);
    static bool looksLikeExceptionHeader(std::string_view line);
    static bool looksLikeFrame(std::string_view line);
    static bool parseOmission(std::string_view line, int& omittedFrames);
    static StackFrame parseFrame(std::string_view line);
};

// Accumulates one exception block while source lines are scanned line by line.
// seedRoot() starts a block from an existing log message; accept() extends the
// active block and reports whether the line belonged to it.
// Invariant: LogEntry::message followed by StackTrace::rawText reproduces the
// source block, so seedRoot() only appends the header to rawText when that
// header text is not already the entry's message.
class StackTraceBuilder {
public:
    void reset();
    void seedRoot(std::string_view headerLine, std::size_t lineNumber,
                  bool includeHeaderInRawText);
    bool accept(std::string_view line, std::size_t lineNumber);

    bool active() const { return !segments_.empty(); }
    std::size_t startLine() const { return startLine_; }
    StackTrace finish();

private:
    void appendSegment(TraceSegment segment, std::string_view sourceLine, std::size_t lineNumber);
    void appendRaw(std::string_view line, std::size_t lineNumber);

    std::vector<TraceSegment> segments_;
    std::vector<std::string> rawLines_;
    std::size_t startLine_ = 0;
    std::size_t endLine_ = 0;
    int causeCount_ = 0;
    int suppressedCount_ = 0;
};

}  // namespace minesleuth
