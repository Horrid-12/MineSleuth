#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "parser/crash_report_parser.h"
#include "test_helpers.h"

using namespace minesleuth;
using minesleuth::test::fixturePath;

TEST(CrashReportParserTest, ParsesVanillaOutOfMemoryReport) {
    CrashReportParser parser;
    const CrashReport report = parser.parseFile(fixturePath("crash_report_oom.txt"));
    ASSERT_TRUE(report.valid) << report.parseError;

    EXPECT_NE(report.title.find("Minecraft Crash Report"), std::string::npos);
    EXPECT_EQ(report.description, "Ticking entity");
    EXPECT_EQ(report.descriptionLine, 4u);
    EXPECT_EQ(report.systemDetail("Minecraft Version"), "1.20.4");
    EXPECT_EQ(report.systemDetail("Java Version"), "17.0.8, Eclipse Adoptium");
    EXPECT_EQ(report.systemDetail("Operating System"), "Windows 11 (amd64) version 10.0");
    EXPECT_EQ(report.systemDetails.size(), 7u);

    ASSERT_EQ(report.exceptions.size(), 1u);
    const StackTrace& trace = report.exceptions[0];
    EXPECT_EQ(trace.rootExceptionClass(), "java.lang.OutOfMemoryError");
    EXPECT_EQ(trace.rootMessage(), "Java heap space");
    ASSERT_EQ(trace.segments.size(), 1u);
    EXPECT_EQ(trace.segments[0].frames.size(), 2u);
    EXPECT_EQ(trace.segments[0].omittedFrames, 12);
    EXPECT_EQ(trace.startLine, 6u);
    EXPECT_TRUE(report.mods.empty());
}

TEST(CrashReportParserTest, ParsesFabricReportWithModList) {
    CrashReportParser parser;
    const CrashReport report = parser.parseFile(fixturePath("crash_report_fabric.txt"));
    ASSERT_TRUE(report.valid) << report.parseError;

    EXPECT_EQ(report.description, "Mod loading failed");
    EXPECT_EQ(report.systemDetail("Operating System"), "Linux (amd64) version 6.5.0");
    EXPECT_EQ(report.systemDetail("Fabric Mods"), "");

    ASSERT_EQ(report.mods.size(), 3u);
    const ModInfo* coolutil = report.findMod("coolutil");
    ASSERT_NE(coolutil, nullptr);
    EXPECT_EQ(coolutil->name, "Cool Util");
    EXPECT_EQ(coolutil->version, "4.5.6");

    const ModInfo* examplemod = report.findMod("examplemod");
    ASSERT_NE(examplemod, nullptr);
    EXPECT_EQ(examplemod->name, "Example Mod");
    EXPECT_EQ(examplemod->version, "1.2.3");

    ASSERT_EQ(report.exceptions.size(), 1u);
    const StackTrace& trace = report.exceptions[0];
    EXPECT_EQ(trace.rootExceptionClass(), "net.fabricmc.loader.impl.FormattedException");
    ASSERT_EQ(trace.segments.size(), 2u);
    EXPECT_EQ(trace.segments[1].kind, TraceSegmentKind::Cause);
    EXPECT_TRUE(trace.containsException("java.lang.ClassNotFoundException"));
}

TEST(CrashReportParserTest, CapturesIndentedDescriptionLines) {
    CrashReportParser parser;
    const CrashReport report = parser.parseLines({
        "---- Minecraft Crash Report ----",
        "Description: First line",
        "  second line",
        "",
        "java.lang.RuntimeException: boom",
        "\tat example.Mod.run(Mod.java:1)",
    });
    ASSERT_TRUE(report.valid) << report.parseError;
    EXPECT_EQ(report.description, "First line second line");
    ASSERT_EQ(report.exceptions.size(), 1u);
    EXPECT_EQ(report.exceptions[0].rootExceptionClass(), "java.lang.RuntimeException");
}

TEST(CrashReportParserTest, RejectsUnrelatedText) {
    CrashReportParser parser;
    const CrashReport report = parser.parseLines({"random text", "another line"});
    EXPECT_FALSE(report.valid);
    EXPECT_FALSE(report.parseError.empty());
    EXPECT_TRUE(report.exceptions.empty());
}

TEST(CrashReportParserTest, RejectsEmptyFile) {
    CrashReportParser parser;
    const CrashReport report = parser.parseFile(fixturePath("empty.log"));
    EXPECT_FALSE(report.valid);
    EXPECT_NE(report.parseError.find("empty"), std::string::npos);
}
