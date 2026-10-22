#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "analyzer/log_analyzer.h"
#include "models/analysis_result.h"
#include "models/diagnostic.h"
#include "parser/crash_report_parser.h"
#include "test_helpers.h"

using namespace minesleuth;
using minesleuth::test::fixturePath;

namespace {

LogEntry makeEntry(LogLevel level, const std::string& message, std::size_t line = 1) {
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.raw_line = message;
    entry.line_number = line;
    return entry;
}

std::vector<Diagnostic> analyze(std::vector<LogEntry> entries) {
    return LogAnalyzer().analyzeEntries(std::move(entries)).diagnostics;
}

const Diagnostic* findRule(const std::vector<Diagnostic>& diagnostics, const std::string& ruleId) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.ruleId == ruleId) {
            return &diagnostic;
        }
    }
    return nullptr;
}

struct RuleSample {
    std::string ruleId;
    std::string positive;
    std::string negative;
};

const std::vector<RuleSample>& ruleSamples() {
    static const std::vector<RuleSample> samples = {
        {"missing-mandatory-dependency", "Missing or unsupported mandatory dependency: jei",
         "Mod list generated successfully"},
        {"mixin-injection-failure", "Mixin apply failed for examplemod.mixins.json",
         "SpongePowered MIXIN subsystem initialized"},
        {"entrypoint-failure", "Could not find entrypoint for mod coolutil",
         "Registering entrypoints completed"},
        {"class-initializer-failure", "java.lang.ExceptionInInitializerError in class Setup",
         "Class loaded successfully"},
        {"class-not-found", "java.lang.ClassNotFoundException: com.example.Thing",
         "All classes resolved"},
        {"out-of-memory", "java.lang.OutOfMemoryError: Java heap space", "Memory usage nominal"},
        {"unsupported-class-version",
         "java.lang.UnsupportedClassVersionError: com/example/Foo was compiled by a newer Java",
         "Java 17.0.8 detected"},
        {"missing-file", "java.io.FileNotFoundException: config/example/config.json",
         "Config file written"},
        {"permission-denied", "Permission denied: /world/region", "Directory created"},
        {"disk-full", "No space left on device", "Disk usage 42 percent"},
        {"native-library-load", "java.lang.UnsatisfiedLinkError: no lwjgl in java.library.path",
         "Native library loaded"},
        {"glfw-window-error", "GLFW error 65542: WGL: driver does not support OpenGL",
         "Window focused"},
        {"opengl-render-error", "OpenGL error 1282: GL_INVALID_OPERATION",
         "Render distance set to 12"},
        {"network-connection-failure", "java.net.ConnectException: Connection refused",
         "Connected to server"},
        {"corrupted-archive", "java.util.zip.ZipException: invalid entry CRC", "Archive extracted"},
        {"duplicate-mod-id", "Duplicate mod id detected: examplemod", "Mods loaded: 12"},
        {"minecraft-version-mismatch", "Mod examplemod requires minecraft 1.20.4",
         "Minecraft 1.20.4 starting"},
        {"loader-mismatch", "This mod is designed for Forge but Fabric Loader is installed",
         "Fabric Loader 0.15.11"},
        {"file-locked",
         "java.nio.file.FileSystemException: mods/example.jar: The process cannot access the file "
         "because it is being used by another process",
         "File access check completed"},
        {"chunk-corruption", "Failed to load chunk at 12, -34 in world", "Chunk loading complete"},
        {"world-save-failure", "Failed to save the world: java.io.IOException: write error",
         "World saved successfully"},
        {"json-parse-error",
         "com.google.gson.JsonSyntaxException: Expected BEGIN_OBJECT but was STRING",
         "Loaded JSON config successfully"},
        {"config-parse-failure", "Could not parse config file: config/examplemod.toml",
         "Config loaded successfully"},
        {"data-pack-error", "Failed to load data pack example: pack.mcmeta missing",
         "Loaded 3 data packs"},
        {"resource-pack-error", "Failed to load resource pack file/example.zip",
         "Resource pack applied"},
        {"java-version-mismatch",
         "Mod examplemod requires Java 21 but the game is running Java 17", "Using Java 17.0.8"},
        {"jvm-fatal-crash", "# A fatal error has been detected by the Java Runtime Environment:",
         "JVM flags applied"},
        {"jvm-argument-error", "Error: Could not create the Java Virtual Machine.",
         "JVM arguments: -Xmx4G"},
        {"stack-overflow", "java.lang.StackOverflowError", "Stack size 1024"},
        {"concurrent-modification", "java.util.ConcurrentModificationException",
         "Concurrent chunk worker pool started"},
        {"port-in-use", "java.net.BindException: Address already in use: bind",
         "Server bound to port 25565"},
        {"authentication-failure",
         "Failed to authenticate: invalid session (try restarting your game)",
         "Authentication token refreshed"},
        {"mod-load-failure", "Failed to load mod examplemod", "Loaded 3 mods"},
        {"access-transformer-error",
         "Access transformer failed for META-INF/accesstransformer.cfg",
         "Access transformer applied"},
        {"remap-failure", "Failed to remap class net/example/Foo", "Remapping completed"},
        {"mixin-config-error", "Could not read mixin config examplemod.mixins.json",
         "Mixin config examplemod.mixins.json loaded"},
    };
    return samples;
}

}  // namespace

TEST(RulesTest, MatchesKnownIssuePatterns) {
    for (const RuleSample& sample : ruleSamples()) {
        SCOPED_TRACE(sample.ruleId);
        const std::vector<Diagnostic> diagnostics = analyze({makeEntry(LogLevel::ERROR, sample.positive)});
        ASSERT_NE(findRule(diagnostics, sample.ruleId), nullptr)
            << "expected a match for: " << sample.positive;
    }
}

TEST(RulesTest, IgnoresUnrelatedMessages) {
    for (const RuleSample& sample : ruleSamples()) {
        SCOPED_TRACE(sample.ruleId);
        const std::vector<Diagnostic> diagnostics = analyze({makeEntry(LogLevel::ERROR, sample.negative)});
        EXPECT_EQ(findRule(diagnostics, sample.ruleId), nullptr)
            << "unexpected match for: " << sample.negative;
    }
}

TEST(RulesTest, InformationalEntriesDoNotTriggerRules) {
    const std::vector<Diagnostic> diagnostics = analyze({makeEntry(LogLevel::INFO, "Missing dependency: jei")});
    EXPECT_EQ(findRule(diagnostics, "missing-mandatory-dependency"), nullptr);
}

TEST(RulesTest, OutOfMemoryIsCriticalAndConfirmed) {
    const std::vector<Diagnostic> diagnostics = analyze(
        {makeEntry(LogLevel::ERROR, "java.lang.OutOfMemoryError: Java heap space")});
    const Diagnostic* diagnostic = findRule(diagnostics, "out-of-memory");
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->severity, Severity::Critical);
    EXPECT_EQ(diagnostic->confidence, Confidence::Confirmed);
    EXPECT_FALSE(diagnostic->suggestions.empty());
}

TEST(RulesTest, MissingFileConfidenceDependsOnEvidence) {
    const Diagnostic* withPath = findRule(analyze({makeEntry(LogLevel::ERROR,
                                                    "java.io.FileNotFoundException: /world/region/r.0.0.mca")}),
                                          "missing-file");
    ASSERT_NE(withPath, nullptr);
    EXPECT_EQ(withPath->confidence, Confidence::Likely);

    const Diagnostic* withoutPath =
        findRule(analyze({makeEntry(LogLevel::ERROR, "java.io.FileNotFoundException: stream closed")}),
                 "missing-file");
    ASSERT_NE(withoutPath, nullptr);
    EXPECT_EQ(withoutPath->confidence, Confidence::Unverified);
}

TEST(RulesTest, RepeatedErrorRuleNeedsThreshold) {
    std::vector<LogEntry> four;
    for (std::size_t i = 0; i < 4; ++i) {
        four.push_back(makeEntry(LogLevel::ERROR, "Example mod exploded unexpectedly", i + 1));
    }
    EXPECT_EQ(findRule(analyze(four), "repeated-error-cluster"), nullptr);

    std::vector<LogEntry> five = four;
    five.push_back(makeEntry(LogLevel::ERROR, "Example mod exploded unexpectedly", 5));
    const Diagnostic* repeated = findRule(analyze(five), "repeated-error-cluster");
    ASSERT_NE(repeated, nullptr);
    EXPECT_EQ(repeated->occurrences, 5u);
    EXPECT_EQ(repeated->confidence, Confidence::Confirmed);
}

TEST(RulesTest, RepeatedErrorRuleDoesNotSuppressFallback) {
    std::vector<LogEntry> entries;
    for (std::size_t i = 0; i < 5; ++i) {
        entries.push_back(makeEntry(LogLevel::ERROR, "Example mod exploded unexpectedly", i + 1));
    }
    const std::vector<Diagnostic> diagnostics = analyze(entries);
    EXPECT_NE(findRule(diagnostics, "repeated-error-cluster"), nullptr);
    EXPECT_NE(findRule(diagnostics, "unrecognized-error"), nullptr);
}

TEST(RulesTest, FallbackFlagsUnmatchedErrors) {
    const std::vector<Diagnostic> diagnostics =
        analyze({makeEntry(LogLevel::ERROR, "Example mod exploded unexpectedly", 42)});
    const Diagnostic* fallback = findRule(diagnostics, "unrecognized-error");
    ASSERT_NE(fallback, nullptr);
    EXPECT_EQ(fallback->confidence, Confidence::Unverified);
    EXPECT_EQ(fallback->severity, Severity::Error);
    EXPECT_EQ(fallback->sourceLine, 42u);
    EXPECT_EQ(fallback->occurrences, 1u);
    EXPECT_EQ(fallback->suggestions.size(), 3u);
}

TEST(RulesTest, FallbackSkipsMatchedErrors) {
    const std::vector<Diagnostic> diagnostics =
        analyze({makeEntry(LogLevel::ERROR, "Missing dependency: jei")});
    EXPECT_NE(findRule(diagnostics, "missing-mandatory-dependency"), nullptr);
    EXPECT_EQ(findRule(diagnostics, "unrecognized-error"), nullptr);
}

TEST(RulesTest, FallbackSkipsBenignWarnings) {
    const std::vector<Diagnostic> diagnostics = analyze(
        {makeEntry(LogLevel::WARN, "Can't keep up! Did the system time change? Running behind")});
    EXPECT_EQ(findRule(diagnostics, "unrecognized-error"), nullptr);
}

TEST(RulesTest, AggregatesDuplicateMatchesIntoOneDiagnostic) {
    std::vector<LogEntry> entries;
    for (std::size_t i = 1; i <= 3; ++i) {
        entries.push_back(makeEntry(LogLevel::ERROR, "Missing dependency: jei", i));
    }
    const std::vector<Diagnostic> diagnostics = analyze(entries);
    const Diagnostic* matched = findRule(diagnostics, "missing-mandatory-dependency");
    ASSERT_NE(matched, nullptr);
    EXPECT_EQ(matched->occurrences, 3u);
    EXPECT_EQ(diagnostics.size(), 1u);
}

TEST(RulesTest, DiagnosticsAreSortedBySeverity) {
    const std::vector<Diagnostic> diagnostics = analyze(
        {makeEntry(LogLevel::ERROR, "Missing dependency: jei", 1),
         makeEntry(LogLevel::ERROR, "java.lang.OutOfMemoryError: Java heap space", 2)});
    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, Severity::Critical);
}

TEST(RulesTest, AnalyzesFabricLogFile) {
    const AnalysisResult result = LogAnalyzer().analyzeFile(fixturePath("fabric_modpack.log"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.format, InputFormat::LogFile);
    EXPECT_EQ(result.exceptions.size(), 1u);
    EXPECT_EQ(result.groups.size(), 4u);

    EXPECT_NE(findRule(result.diagnostics, "missing-mandatory-dependency"), nullptr);
    EXPECT_NE(findRule(result.diagnostics, "entrypoint-failure"), nullptr);
    EXPECT_NE(findRule(result.diagnostics, "class-not-found"), nullptr);
    EXPECT_NE(findRule(result.diagnostics, "duplicate-mod-id"), nullptr);
    EXPECT_NE(findRule(result.diagnostics, "missing-file"), nullptr);

    const Diagnostic* repeated = findRule(result.diagnostics, "repeated-error-cluster");
    ASSERT_NE(repeated, nullptr);
    EXPECT_EQ(repeated->occurrences, 5u);
    EXPECT_EQ(findRule(result.diagnostics, "unrecognized-error"), nullptr);
}

TEST(RulesTest, AnalyzesForgeLogWithFallbackOnly) {
    const AnalysisResult result = LogAnalyzer().analyzeFile(fixturePath("forge_unrecognized.log"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.format, InputFormat::LogFile);
    EXPECT_EQ(result.exceptions.size(), 2u);
    ASSERT_EQ(result.groups.size(), 2u);

    const Diagnostic* fallback = findRule(result.diagnostics, "unrecognized-error");
    ASSERT_NE(fallback, nullptr);
    EXPECT_EQ(fallback->confidence, Confidence::Unverified);
    EXPECT_EQ(fallback->occurrences, 2u);
    EXPECT_NE(fallback->title.find("java.lang.IllegalStateException"), std::string::npos);
    EXPECT_EQ(findRule(result.diagnostics, "missing-mandatory-dependency"), nullptr);
}

TEST(RulesTest, AnalyzesOutOfMemoryCrashReport) {
    const AnalysisResult result = LogAnalyzer().analyzeFile(fixturePath("crash_report_oom.txt"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.format, InputFormat::CrashReport);
    ASSERT_TRUE(result.crashReport.has_value());
    EXPECT_EQ(result.exceptions.size(), 1u);

    const Diagnostic* outOfMemory = findRule(result.diagnostics, "out-of-memory");
    ASSERT_NE(outOfMemory, nullptr);
    EXPECT_EQ(outOfMemory->severity, Severity::Critical);

    const Diagnostic* description = findRule(result.diagnostics, "crash-report-description");
    ASSERT_NE(description, nullptr);
    EXPECT_EQ(description->confidence, Confidence::Confirmed);
    EXPECT_NE(description->explanation.find("Ticking entity"), std::string::npos);
    EXPECT_NE(description->explanation.find("1.20.4"), std::string::npos);
}

TEST(RulesTest, AnalyzesFabricCrashReportWithMods) {
    const AnalysisResult result = LogAnalyzer().analyzeFile(fixturePath("crash_report_fabric.txt"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.format, InputFormat::CrashReport);
    ASSERT_TRUE(result.crashReport.has_value());
    EXPECT_EQ(result.crashReport->mods.size(), 3u);

    EXPECT_NE(findRule(result.diagnostics, "entrypoint-failure"), nullptr);
    EXPECT_NE(findRule(result.diagnostics, "class-not-found"), nullptr);
    EXPECT_NE(findRule(result.diagnostics, "crash-report-description"), nullptr);
}

TEST(RulesTest, ReportsMissingFiles) {
    const AnalysisResult result = LogAnalyzer().analyzeFile(fixturePath("definitely_missing.log"));
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
}

TEST(RulesTest, LongCrashReportDescriptionPreservesSentencesWithoutMidWordCutoff) {
    CrashReport report;
    report.valid = true;
    report.description = "This section contains information related to dynamic lighting, this may not be related to your crash. Mode: fastest Dynamic Light Sources: 0 Spatial Hash Occupancy: 0 / 1024";
    report.systemDetails.push_back({"Minecraft Version", "1.21.11"});

    const RuleEngine engine = RuleEngine::withDefaultRules();
    const std::vector<Diagnostic> diags = engine.evaluate({}, {}, {}, &report, nullptr);

    const Diagnostic* descDiag = nullptr;
    for (const auto& d : diags) {
        if (d.ruleId == "crash-report-description") {
            descDiag = &d;
            break;
        }
    }
    ASSERT_NE(descDiag, nullptr);

    // Title should cleanly preserve the first sentence without cutting off words like "cr..."
    EXPECT_EQ(descDiag->title.find("cr..."), std::string::npos);
    EXPECT_NE(descDiag->title.find("dynamic lighting"), std::string::npos);
    // Explanation should contain the full game description
    EXPECT_NE(descDiag->explanation.find("Spatial Hash Occupancy"), std::string::npos);
}

