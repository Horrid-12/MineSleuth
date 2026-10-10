#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "analyzer/error_grouper.h"
#include "models/log_entry.h"
#include "parser/stack_trace_parser.h"

using namespace minesleuth;

namespace {

LogEntry makeEntry(LogLevel level, const std::string& message, std::size_t line = 1) {
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.raw_line = message;
    entry.line_number = line;
    entry.timestamp = "14:32:0" + std::to_string(line % 10);
    return entry;
}

LogEntry makeTracedEntry(LogLevel level, const std::string& message, const std::string& header,
                         const std::string& frame, std::size_t line = 1) {
    LogEntry entry = makeEntry(level, message, line);
    StackTraceBuilder builder;
    builder.seedRoot(header, line, true);
    builder.accept(frame, line);
    entry.trace = builder.finish();
    return entry;
}

std::vector<ErrorGroup> groupOf(const std::vector<LogEntry>& entries) {
    return ErrorGrouper().group(entries);
}

}  // namespace

TEST(ErrorGrouperTest, GroupsNormalizedDuplicates) {
    std::vector<LogEntry> entries;
    entries.push_back(makeEntry(LogLevel::ERROR, "Failed to load resource textures/block/x1.png", 1));
    entries.push_back(makeEntry(LogLevel::ERROR, "Failed to load resource textures/block/x2.png", 2));
    entries.push_back(makeEntry(LogLevel::ERROR, "Failed to load resource textures/block/x3.png", 3));

    const std::vector<ErrorGroup> groups = groupOf(entries);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].count, 3u);
    EXPECT_EQ(groups[0].entryIndices.size(), 3u);
    EXPECT_EQ(groups[0].entryIndices.front(), 0u);
    EXPECT_EQ(groups[0].severity, LogLevel::ERROR);
    EXPECT_EQ(groups[0].messageTemplate, "Failed to load resource <path>");
    ASSERT_TRUE(groups[0].firstTimestamp.has_value());
    EXPECT_EQ(*groups[0].firstTimestamp, "14:32:01");
    EXPECT_EQ(*groups[0].lastTimestamp, "14:32:03");
}

TEST(ErrorGrouperTest, KeepsDistinctMessagesSeparate) {
    std::vector<LogEntry> entries;
    entries.push_back(makeEntry(LogLevel::ERROR, "Mod alpha failed"));
    entries.push_back(makeEntry(LogLevel::ERROR, "Mod beta failed"));

    EXPECT_EQ(groupOf(entries).size(), 2u);
}

TEST(ErrorGrouperTest, NormalizesUuidVariants) {
    std::vector<LogEntry> entries;
    entries.push_back(makeEntry(LogLevel::ERROR,
                                "Connection to 4f9b2a1e-7c3d-4e8a-9b21-5d6f7a8b9c0d refused"));
    entries.push_back(makeEntry(LogLevel::ERROR,
                                "Connection to 11111111-2222-3333-4444-555555555555 refused"));

    const std::vector<ErrorGroup> groups = groupOf(entries);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].count, 2u);
    EXPECT_EQ(groups[0].messageTemplate, "Connection to <uuid> refused");
}

TEST(ErrorGrouperTest, RollsUpSeverityAcrossOccurrences) {
    std::vector<LogEntry> entries;
    entries.push_back(makeEntry(LogLevel::WARN, "Same problem here"));
    entries.push_back(makeEntry(LogLevel::ERROR, "Same problem here"));

    const std::vector<ErrorGroup> groups = groupOf(entries);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].severity, LogLevel::ERROR);
    EXPECT_EQ(groups[0].count, 2u);
}

TEST(ErrorGrouperTest, SeparatesDifferentTopFrames) {
    std::vector<LogEntry> entries;
    entries.push_back(makeTracedEntry(LogLevel::ERROR, "Tick failed", "java.lang.RuntimeException: tick",
                                      "\tat example.alpha.Alpha.run(Alpha.java:1)"));
    entries.push_back(makeTracedEntry(LogLevel::ERROR, "Tick failed", "java.lang.RuntimeException: tick",
                                      "\tat example.beta.Beta.run(Beta.java:1)", 2));

    const std::vector<ErrorGroup> groups = groupOf(entries);
    ASSERT_EQ(groups.size(), 2u);
    ASSERT_TRUE(groups[0].topFrame.has_value());
    ASSERT_TRUE(groups[1].topFrame.has_value());
    EXPECT_NE(*groups[0].topFrame, *groups[1].topFrame);
}

TEST(ErrorGrouperTest, SkipsInformationalEntries) {
    std::vector<LogEntry> entries;
    entries.push_back(makeEntry(LogLevel::INFO, "All good"));
    entries.push_back(makeEntry(LogLevel::DEBUG, "Tracing internals"));
    entries.push_back(makeEntry(LogLevel::UNKNOWN, "Unstructured line"));

    EXPECT_TRUE(groupOf(entries).empty());
}

TEST(ErrorGrouperTest, GroupsProblemEntriesAcrossLevels) {
    std::vector<LogEntry> entries;
    entries.push_back(makeEntry(LogLevel::INFO, "quiet line"));
    entries.push_back(makeEntry(LogLevel::ERROR, "loud failure", 2));
    entries.push_back(makeEntry(LogLevel::WARN, "careful now", 3));

    const std::vector<ErrorGroup> groups = groupOf(entries);
    ASSERT_EQ(groups.size(), 2u);
}
