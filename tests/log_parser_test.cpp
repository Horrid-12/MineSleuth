#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "parser/log_parser.h"
#include "test_helpers.h"

using namespace minesleuth;
using minesleuth::test::fixturePath;
using minesleuth::test::readLines;

TEST(LogParserTest, ParsesFabricLogWithStackTrace) {
    LogParser parser;
    ASSERT_TRUE(parser.parseFile(fixturePath("fabric_modpack.log")));

    const std::vector<LogEntry>& entries = parser.getEntries();
    ASSERT_EQ(entries.size(), 13u);

    EXPECT_EQ(entries[0].level, LogLevel::INFO);
    EXPECT_EQ(entries[0].timestamp, "14:32:10");
    EXPECT_EQ(entries[0].thread, "main");
    EXPECT_EQ(entries[0].message, "Loading Minecraft 1.20.1 with Fabric Loader 0.15.11");

    const LogEntry* traced = nullptr;
    for (const LogEntry& entry : entries) {
        if (entry.trace.has_value()) {
            traced = &entry;
            break;
        }
    }
    ASSERT_NE(traced, nullptr);
    EXPECT_EQ(traced->message, "Could not start mod coolutil");
    ASSERT_TRUE(traced->trace.has_value());

    const StackTrace& trace = *traced->trace;
    ASSERT_EQ(trace.segments.size(), 2u);
    EXPECT_EQ(trace.rootExceptionClass(), "java.lang.RuntimeException");
    EXPECT_TRUE(trace.containsException("java.lang.NoClassDefFoundError"));
    EXPECT_EQ(trace.segments[0].frames.size(), 2u);
    EXPECT_EQ(trace.segments[0].omittedFrames, 12);
    EXPECT_EQ(trace.segments[1].omittedFrames, 8);
    EXPECT_NE(trace.rawText.find("\tat net.fabricmc"), std::string::npos);

    const LogStatistics& stats = parser.getStatistics();
    EXPECT_EQ(stats.total_entries, 13u);
    EXPECT_EQ(stats.infos, 5u);
    EXPECT_EQ(stats.warnings, 1u);
    EXPECT_EQ(stats.errors, 7u);
    EXPECT_EQ(stats.fatals, 0u);
    EXPECT_EQ(stats.stack_traces, 1u);
}

TEST(LogParserTest, PreservesEverySourceLine) {
    const char* fixtures[] = {"fabric_modpack.log", "forge_unrecognized.log",
                              "suppressed_trace.log", "malformed.log"};
    for (const char* name : fixtures) {
        SCOPED_TRACE(name);
        const std::vector<std::string> source = readLines(fixturePath(name));
        ASSERT_FALSE(source.empty());

        LogParser parser;
        ASSERT_TRUE(parser.parseFile(fixturePath(name)));

        std::set<std::string> accounted;
        for (const LogEntry& entry : parser.getEntries()) {
            accounted.insert(entry.raw_line);
            accounted.insert(entry.message);
            if (!entry.trace.has_value()) {
                continue;
            }
            std::string remaining = entry.trace->rawText;
            while (!remaining.empty()) {
                const std::size_t newline = remaining.find('\n');
                const std::string line = remaining.substr(0, newline);
                accounted.insert(line);
                if (newline == std::string::npos) {
                    break;
                }
                remaining.erase(0, newline + 1);
            }
        }

        for (const std::string& line : source) {
            if (line.empty()) {
                continue;
            }
            EXPECT_TRUE(accounted.count(line) > 0) << "lost source line: " << line;
        }
    }
}

TEST(LogParserTest, HandlesMalformedLinesWithoutFailing) {
    LogParser parser;
    ASSERT_TRUE(parser.parseFile(fixturePath("malformed.log")));

    const std::vector<LogEntry>& entries = parser.getEntries();
    ASSERT_EQ(entries.size(), 5u);
    EXPECT_EQ(entries[0].level, LogLevel::UNKNOWN);
    EXPECT_EQ(entries[0].message, "this file is not a real log[14:32:10] [main/INFO]: partial line");
    EXPECT_EQ(entries[2].message, "random prose without any structure");
    EXPECT_EQ(entries[4].level, LogLevel::UNKNOWN);
    EXPECT_EQ(entries[4].message, "unknown level name");
    EXPECT_EQ(parser.getStatistics().total_entries, 5u);
}

TEST(LogParserTest, ParsesSuppressedAndCausedByChain) {
    LogParser parser;
    ASSERT_TRUE(parser.parseFile(fixturePath("suppressed_trace.log")));

    const LogEntry& fatal = parser.getEntries().at(0);
    EXPECT_EQ(fatal.level, LogLevel::FATAL);
    ASSERT_TRUE(fatal.trace.has_value());

    const StackTrace& trace = *fatal.trace;
    ASSERT_EQ(trace.segments.size(), 3u);
    EXPECT_EQ(trace.segments[0].kind, TraceSegmentKind::Root);
    EXPECT_EQ(trace.segments[1].kind, TraceSegmentKind::Suppressed);
    EXPECT_EQ(trace.segments[2].kind, TraceSegmentKind::Cause);
    EXPECT_EQ(trace.segments[1].omittedFrames, 3);
    EXPECT_EQ(trace.segments[2].omittedFrames, 1);
    EXPECT_TRUE(trace.containsException("java.io.FileNotFoundException"));
    EXPECT_EQ(parser.getStatistics().fatals, 1u);
    EXPECT_EQ(parser.getStatistics().stack_traces, 1u);
}

TEST(LogParserTest, AcceptsEmptyFile) {
    LogParser parser;
    ASSERT_TRUE(parser.parseFile(fixturePath("empty.log")));
    EXPECT_TRUE(parser.getEntries().empty());
    EXPECT_EQ(parser.getStatistics().total_entries, 0u);
}

TEST(LogParserTest, RejectsMissingFile) {
    LogParser parser;
    EXPECT_FALSE(parser.parseFile(fixturePath("definitely_missing.log")));
}
