#include <gtest/gtest.h>

#include <string>

#include "parser/stack_trace_parser.h"

using namespace minesleuth;

TEST(StackTraceParserTest, ParsesSimpleFrame) {
    const StackFrame frame = StackTraceParser::parseFrame("\tat com.foo.Bar.baz(Bar.java:12)");
    ASSERT_TRUE(frame.className.has_value());
    EXPECT_EQ(*frame.className, "com.foo.Bar");
    ASSERT_TRUE(frame.methodName.has_value());
    EXPECT_EQ(*frame.methodName, "baz");
    ASSERT_TRUE(frame.sourceFile.has_value());
    EXPECT_EQ(*frame.sourceFile, "Bar.java");
    ASSERT_TRUE(frame.lineNumber.has_value());
    EXPECT_EQ(*frame.lineNumber, 12);
    EXPECT_FALSE(frame.nativeMethod);
    EXPECT_EQ(frame.signature(), "com.foo.Bar.baz");
}

TEST(StackTraceParserTest, ParsesNativeMethodFrame) {
    const StackFrame frame =
        StackTraceParser::parseFrame("\tat java.io.FileInputStream.open0(Native Method)");
    ASSERT_TRUE(frame.nativeMethod);
    EXPECT_EQ(*frame.className, "java.io.FileInputStream");
    EXPECT_EQ(*frame.methodName, "open0");
    EXPECT_FALSE(frame.lineNumber.has_value());
}

TEST(StackTraceParserTest, ParsesModulePrefixedFrame) {
    const StackFrame frame =
        StackTraceParser::parseFrame("\tat java.base/java.lang.Thread.run(Thread.java:840)");
    ASSERT_TRUE(frame.className.has_value());
    EXPECT_EQ(*frame.className, "java.lang.Thread");
    EXPECT_EQ(*frame.methodName, "run");
    ASSERT_TRUE(frame.lineNumber.has_value());
    EXPECT_EQ(*frame.lineNumber, 840);
    EXPECT_NE(frame.raw.find("java.base/java.lang.Thread"), std::string::npos);
}

TEST(StackTraceParserTest, ParsesLoaderAndModulePrefixedFrames) {
    const char* fabricLine =
        "\tat knot//net.minecraft.class_1439.method_37908(class_1439.java:123)";
    EXPECT_TRUE(StackTraceParser::looksLikeFrame(fabricLine));
    const StackFrame fabric = StackTraceParser::parseFrame(fabricLine);
    ASSERT_TRUE(fabric.className.has_value());
    EXPECT_EQ(*fabric.className, "net.minecraft.class_1439");
    EXPECT_EQ(*fabric.methodName, "method_37908");
    EXPECT_NE(fabric.raw.find("knot//net.minecraft.class_1439"), std::string::npos);

    const char* customLine =
        "\tat launcher/example.module@1.0/com.example.coolutil.Widget.tick(Widget.java:42)";
    EXPECT_TRUE(StackTraceParser::looksLikeFrame(customLine));
    const StackFrame custom = StackTraceParser::parseFrame(customLine);
    ASSERT_TRUE(custom.className.has_value());
    EXPECT_EQ(*custom.className, "com.example.coolutil.Widget");
    EXPECT_EQ(*custom.methodName, "tick");
}

TEST(StackTraceParserTest, SignatureNormalizesModulePrefix) {
    StackFrame frame;
    frame.className = "custom.loader//com.example.coolutil.Widget";
    frame.methodName = "tick";
    EXPECT_EQ(frame.signature(), "com.example.coolutil.Widget.tick");
}

TEST(StackTraceParserTest, ParsesUnknownSourceFrame) {
    const StackFrame frame = StackTraceParser::parseFrame("at example.Mod.tick(Unknown Source)");
    ASSERT_TRUE(frame.sourceFile.has_value());
    EXPECT_EQ(*frame.sourceFile, "Unknown Source");
    EXPECT_FALSE(frame.lineNumber.has_value());
}

TEST(StackTraceParserTest, ParsesFrameWithServerMetadata) {
    const StackFrame frame = StackTraceParser::parseFrame(
        "\tat net.minecraft.server.v1_8_R3.ChunkRegionLoader.loadChunk(ChunkRegionLoader.java:70) "
        "~[minecraft_server.jar:git-Spigot-db6de12-18fbb24]");
    ASSERT_TRUE(frame.className.has_value());
    EXPECT_EQ(*frame.className, "net.minecraft.server.v1_8_R3.ChunkRegionLoader");
    ASSERT_TRUE(frame.methodName.has_value());
    EXPECT_EQ(*frame.methodName, "loadChunk");
    ASSERT_TRUE(frame.sourceFile.has_value());
    EXPECT_EQ(*frame.sourceFile, "ChunkRegionLoader.java");
    ASSERT_TRUE(frame.lineNumber.has_value());
    EXPECT_EQ(*frame.lineNumber, 70);
}

TEST(StackTraceParserTest, RejectsOrdinaryLogMessages) {
    const char* notHeaders[] = {
        "Missing dependency: jei",
        "Description: Mod loading failed",
        "Memory: 4096 MiB / 4096 MiB",
        "at com.foo.Bar.baz(Bar.java:1)",
        "-- System Details --",
        "... 12 more",
    };
    for (const char* text : notHeaders) {
        EXPECT_FALSE(StackTraceParser::looksLikeExceptionHeader(text)) << text;
    }
}

TEST(StackTraceParserTest, AcceptsExceptionHeaders) {
    const char* headers[] = {
        "java.lang.RuntimeException: boom",
        "java.lang.NullPointerException",
        "net.fabricmc.loader.impl.FormattedException: Could not find entrypoint",
        "MyCustomException: gone",
        "Caused by: java.io.IOException: nope",
        "Suppressed: java.lang.Error: nested",
    };
    for (const char* text : headers) {
        EXPECT_TRUE(StackTraceParser::looksLikeExceptionHeader(text)) << text;
    }
}

TEST(StackTraceParserTest, ParsesHeaderDetails) {
    TraceSegment segment;
    ASSERT_TRUE(StackTraceParser::parseExceptionHeader(
        "java.lang.RuntimeException: something broke", segment));
    EXPECT_EQ(segment.kind, TraceSegmentKind::Root);
    EXPECT_EQ(segment.exceptionClass, "java.lang.RuntimeException");
    EXPECT_EQ(segment.message, "something broke");
    EXPECT_EQ(segment.header(), "java.lang.RuntimeException: something broke");
}

TEST(StackTraceParserTest, ParsesCauseAndSuppressedKinds) {
    TraceSegment cause;
    ASSERT_TRUE(StackTraceParser::parseExceptionHeader(
        "Caused by: java.io.FileNotFoundException: missing", cause));
    EXPECT_EQ(cause.kind, TraceSegmentKind::Cause);
    EXPECT_EQ(cause.exceptionClass, "java.io.FileNotFoundException");
    EXPECT_EQ(cause.message, "missing");

    TraceSegment suppressed;
    ASSERT_TRUE(
        StackTraceParser::parseExceptionHeader("Suppressed: java.lang.Error: nested", suppressed));
    EXPECT_EQ(suppressed.kind, TraceSegmentKind::Suppressed);
}

TEST(StackTraceParserTest, ParsesExceptionInThreadPrefix) {
    TraceSegment segment;
    ASSERT_TRUE(StackTraceParser::parseExceptionHeader(
        "Exception in thread \"main\" java.lang.NullPointerException: oops", segment));
    EXPECT_EQ(segment.exceptionClass, "java.lang.NullPointerException");
    EXPECT_EQ(segment.message, "oops");
}

TEST(StackTraceParserTest, ParsesOmissionLines) {
    int frames = 0;
    EXPECT_TRUE(StackTraceParser::parseOmission("\t... 12 more", frames));
    EXPECT_EQ(frames, 12);
    EXPECT_FALSE(StackTraceParser::parseOmission("... more", frames));
    EXPECT_FALSE(StackTraceParser::parseOmission("...", frames));
    EXPECT_FALSE(StackTraceParser::parseOmission("just some text", frames));
}

TEST(StackTraceBuilderTest, SeedRootFramesAndCauses) {
    StackTraceBuilder builder;
    builder.seedRoot("java.lang.RuntimeException: boom", 10, false);
    ASSERT_TRUE(builder.active());
    EXPECT_EQ(builder.startLine(), 10u);

    EXPECT_TRUE(builder.accept("\tat com.foo.Bar.baz(Bar.java:1)", 11));
    EXPECT_FALSE(builder.accept("java.lang.IllegalStateException: second", 12));
    EXPECT_TRUE(builder.accept("Caused by: java.io.IOException: nope", 13));
    EXPECT_TRUE(builder.accept("\tat com.foo.IO.read(IO.java:2)", 14));
    EXPECT_TRUE(builder.accept("\t... 4 more", 15));
    EXPECT_FALSE(builder.accept("plain prose that is not a trace line", 16));

    const StackTrace trace = builder.finish();
    ASSERT_EQ(trace.segments.size(), 2u);
    EXPECT_EQ(trace.segments[0].exceptionClass, "java.lang.RuntimeException");
    EXPECT_EQ(trace.segments[0].frames.size(), 1u);
    EXPECT_EQ(trace.segments[1].kind, TraceSegmentKind::Cause);
    EXPECT_EQ(trace.segments[1].depth, 0);
    EXPECT_EQ(trace.segments[1].frames.size(), 1u);
    EXPECT_EQ(trace.segments[1].omittedFrames, 4);
    EXPECT_EQ(trace.startLine, 10u);
    EXPECT_EQ(trace.endLine, 15u);
    EXPECT_EQ(trace.rawText,
              "\tat com.foo.Bar.baz(Bar.java:1)\nCaused by: java.io.IOException: nope\n"
              "\tat com.foo.IO.read(IO.java:2)\n\t... 4 more");
    EXPECT_EQ(trace.rawText.find("boom"), std::string::npos);
    EXPECT_EQ(trace.topFrame(), "com.foo.Bar.baz");
    EXPECT_EQ(trace.rootMessage(), "boom");
    EXPECT_FALSE(builder.active());
}

TEST(StackTraceBuilderTest, AcceptsTraceStartingWithCause) {
    StackTraceBuilder builder;
    EXPECT_TRUE(builder.accept("Caused by: java.lang.Error: nested", 3));
    const StackTrace trace = builder.finish();
    ASSERT_EQ(trace.segments.size(), 1u);
    EXPECT_EQ(trace.segments[0].kind, TraceSegmentKind::Cause);
    EXPECT_NE(trace.rootSegment(), nullptr);
}
