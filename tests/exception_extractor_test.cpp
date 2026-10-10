#include <gtest/gtest.h>

#include <string>

#include "analyzer/exception_extractor.h"
#include "models/log_entry.h"
#include "parser/stack_trace_parser.h"

using namespace minesleuth;

namespace {

LogEntry makeEntry(LogLevel level, const std::string& message) {
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.raw_line = message;
    entry.line_number = 1;
    return entry;
}

LogEntry makeTracedEntry(LogLevel level, const std::string& message, const std::string& header,
                         const std::string& frame) {
    LogEntry entry = makeEntry(level, message);
    StackTraceBuilder builder;
    builder.seedRoot(header, 1, true);
    builder.accept(frame, 2);
    entry.trace = builder.finish();
    return entry;
}

}  // namespace

TEST(ExceptionExtractorTest, ExtractsEntriesThatCarryPlausibleTraces) {
    std::vector<LogEntry> entries;
    entries.push_back(makeTracedEntry(LogLevel::ERROR, "Could not start", "java.lang.RuntimeException: boom",
                                      "\tat example.Mod.run(Mod.java:1)"));
    entries.push_back(makeEntry(LogLevel::INFO, "plain info line"));

    const ExceptionOccurrence occurrence = ExceptionExtractor().extract(entries).at(0);
    EXPECT_EQ(occurrence.entryIndex, 0u);
    EXPECT_EQ(occurrence.trace.rootExceptionClass(), "java.lang.RuntimeException");
}

TEST(ExceptionExtractorTest, RejectsFramelessNonExceptionHeaders) {
    StackTrace trace;
    TraceSegment segment;
    segment.exceptionClass = "com.example.Registry";
    segment.headerRaw = "com.example.Registry: loaded 12 entries";
    trace.segments.push_back(segment);

    EXPECT_FALSE(ExceptionExtractor::isPlausibleException(trace));
}

TEST(ExceptionExtractorTest, AcceptsFramelessClassicExceptions) {
    StackTrace trace;
    TraceSegment segment;
    segment.exceptionClass = "java.lang.RuntimeException";
    trace.segments.push_back(segment);

    EXPECT_TRUE(ExceptionExtractor::isPlausibleException(trace));
    EXPECT_FALSE(ExceptionExtractor::isPlausibleException(StackTrace{}));
}

TEST(ExceptionExtractorTest, ClassifiesProblemEntries) {
    EXPECT_TRUE(isProblemEntry(makeEntry(LogLevel::WARN, "careful")));
    EXPECT_TRUE(isProblemEntry(makeEntry(LogLevel::ERROR, "broken")));
    EXPECT_TRUE(isProblemEntry(makeEntry(LogLevel::FATAL, "dead")));
    EXPECT_FALSE(isProblemEntry(makeEntry(LogLevel::INFO, "fine")));
    EXPECT_FALSE(isProblemEntry(makeEntry(LogLevel::DEBUG, "noise")));

    LogEntry infoWithTrace =
        makeTracedEntry(LogLevel::INFO, "quiet", "java.lang.IllegalStateException: hidden",
                        "\tat example.Mod.run(Mod.java:1)");
    EXPECT_TRUE(isProblemEntry(infoWithTrace));
}
