#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "analyzer/log_analyzer.h"
#include "analyzer/mod_attribution.h"
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

LogEntry tracedEntry(LogLevel level, const std::string& message,
                     const std::vector<std::string>& classNames) {
    LogEntry entry = makeEntry(level, message);
    TraceSegment segment;
    segment.kind = TraceSegmentKind::Root;
    segment.exceptionClass = "java.lang.RuntimeException";
    for (const std::string& className : classNames) {
        StackFrame frame;
        frame.className = className;
        frame.methodName = "run";
        segment.frames.push_back(frame);
    }
    StackTrace trace;
    trace.segments.push_back(std::move(segment));
    entry.trace = std::move(trace);
    return entry;
}

ModRegistry registryWith(const std::string& id, const std::string& name = {},
                         const std::string& version = {}) {
    ModRegistry registry;
    ModRef ref;
    ref.id = id;
    ref.name = name.empty() ? id : name;
    ref.version = version;
    ref.source = "crash report";
    registry.add(std::move(ref));
    return registry;
}

const Diagnostic* findRule(const std::vector<Diagnostic>& diagnostics, const std::string& ruleId) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.ruleId == ruleId) {
            return &diagnostic;
        }
    }
    return nullptr;
}

bool anySuggestionNames(const Diagnostic& diagnostic, const std::string& needle) {
    return std::any_of(diagnostic.suggestions.begin(), diagnostic.suggestions.end(),
                       [&](const std::string& suggestion) {
                           return suggestion.find(needle) != std::string::npos;
                       });
}

}  // namespace

TEST(ModAttributorTest, BuildsRegistryFromCrashReport) {
    CrashReportParser parser;
    const CrashReport report = parser.parseFile(fixturePath("crash_report_fabric.txt"));
    ASSERT_TRUE(report.valid);
    ASSERT_EQ(report.mods.size(), 3u);

    const ModRegistry registry = ModAttributor::buildRegistry({}, &report);
    EXPECT_EQ(registry.size(), 3u);
    const ModRef* coolutil = registry.byId("coolutil");
    ASSERT_NE(coolutil, nullptr);
    EXPECT_EQ(coolutil->version, "4.5.6");
    EXPECT_EQ(coolutil->source, "crash report");
}

TEST(ModAttributorTest, CollectsModIdsMentionedInMessages) {
    const std::vector<LogEntry> entries = {
        makeEntry(LogLevel::ERROR, "Duplicate mod id: examplemod"),
        makeEntry(LogLevel::ERROR, "Missing dependency: coolutil"),
        makeEntry(LogLevel::INFO, "Loading Minecraft 1.20.1 with Fabric Loader 0.15.11"),
    };
    const ModRegistry registry = ModAttributor::buildRegistry(entries, nullptr);
    EXPECT_NE(registry.byId("examplemod"), nullptr);
    EXPECT_NE(registry.byId("coolutil"), nullptr);
    EXPECT_EQ(registry.byId("fabric"), nullptr);
}

TEST(ModAttributorTest, AttributesModIdNamedInMessage) {
    const ModAttributor attributor;
    const ModAttribution attribution =
        attributor.attribute(makeEntry(LogLevel::ERROR, "Duplicate mod id detected: examplemod"));
    EXPECT_TRUE(attribution.identified);
    EXPECT_EQ(attribution.modId, "examplemod");
    EXPECT_EQ(attribution.basis, ModBasis::MessageMention);
    EXPECT_EQ(attribution.confidence, Confidence::Confirmed);
    EXPECT_EQ(attribution.label(), "examplemod");
}

TEST(ModAttributorTest, AttributesEntrypointFailureToNamedMod) {
    const ModAttributor attributor;
    const ModAttribution attribution = attributor.attribute(
        makeEntry(LogLevel::ERROR, "Could not find entrypoint for mod coolutil"));
    ASSERT_TRUE(attribution.identified);
    EXPECT_EQ(attribution.modId, "coolutil");
}

TEST(ModAttributorTest, ResolvesFramePackageToKnownMod) {
    const ModAttributor attributor(registryWith("examplemod", "Example Mod", "1.2.3"));
    const ModAttribution attribution = attributor.attribute(tracedEntry(
        LogLevel::ERROR, "Widget exploded",
        {"net.minecraft.client.Minecraft", "examplemod.widget.WidgetFactory"}));
    ASSERT_TRUE(attribution.identified);
    EXPECT_EQ(attribution.modId, "examplemod");
    EXPECT_EQ(attribution.modName, "Example Mod");
    EXPECT_EQ(attribution.modVersion, "1.2.3");
    EXPECT_EQ(attribution.basis, ModBasis::KnownModPackage);
    EXPECT_EQ(attribution.confidence, Confidence::Likely);
    EXPECT_EQ(attribution.ownerPackage, "examplemod.widget");
}

TEST(ModAttributorTest, FallsBackToFramePackageWithoutRegistry) {
    const ModAttributor attributor;
    const ModAttribution attribution = attributor.attribute(
        tracedEntry(LogLevel::ERROR, "Setup failed",
                    {"com.example.coolutil.init.ModInitializer"}));
    ASSERT_TRUE(attribution.identified);
    EXPECT_TRUE(attribution.modId.empty());
    EXPECT_EQ(attribution.ownerPackage, "com.example.coolutil.init");
    EXPECT_EQ(attribution.basis, ModBasis::FramePackage);
    EXPECT_EQ(attribution.confidence, Confidence::Unverified);
    EXPECT_NE(attribution.actionStep("some-rule").find("com.example.coolutil.init"),
              std::string::npos);
}

TEST(ModAttributorTest, DerivesCandidateIdFromPackageRoot) {
    const ModAttribution attribution = ModAttributor().attribute(
        tracedEntry(LogLevel::ERROR, "Widget exploded", {"examplemod.widget.WidgetFactory"}));
    ASSERT_TRUE(attribution.identified);
    EXPECT_EQ(attribution.modId, "examplemod");
    EXPECT_EQ(attribution.ownerPackage, "examplemod.widget");
    EXPECT_EQ(attribution.basis, ModBasis::FramePackage);
    EXPECT_EQ(attribution.confidence, Confidence::Unverified);
    EXPECT_EQ(attribution.label(), "examplemod");
}

TEST(ModAttributorTest, IgnoresPlatformFrames) {
    const ModAttributor attributor;
    const ModAttribution attribution = attributor.attribute(
        tracedEntry(LogLevel::ERROR, "Server stopped",
                    {"net.minecraft.server.MinecraftServer.run", "java.lang.Thread.run"}));
    EXPECT_FALSE(attribution.identified);
    EXPECT_TRUE(attribution.modId.empty());
}

TEST(ModAttributorTest, AttributesMissingClassFromExceptionMessage) {
    const std::string message = "java.lang.NoClassDefFoundError: com/coolutil/ModInitializer";

    const ModAttribution known =
        ModAttributor(registryWith("coolutil")).attribute(makeEntry(LogLevel::ERROR, message));
    ASSERT_TRUE(known.identified);
    EXPECT_EQ(known.modId, "coolutil");
    EXPECT_EQ(known.basis, ModBasis::ExceptionClass);
    EXPECT_EQ(known.confidence, Confidence::Likely);

    const ModAttribution unknown = ModAttributor().attribute(makeEntry(LogLevel::ERROR, message));
    ASSERT_TRUE(unknown.identified);
    EXPECT_TRUE(unknown.modId.empty());
    EXPECT_EQ(unknown.ownerPackage, "com.coolutil");
    EXPECT_EQ(unknown.confidence, Confidence::Unverified);
}

TEST(ModAttributionActionStepTest, KeepsAdviceCorrectPerRule) {
    ModAttribution attribution;
    attribution.identified = true;
    attribution.modId = "jei";

    EXPECT_NE(attribution.actionStep("missing-mandatory-dependency").find("Install the mod jei"),
              std::string::npos);
    EXPECT_NE(attribution.actionStep("duplicate-mod-id").find("jei"), std::string::npos);
    EXPECT_NE(attribution.actionStep("minecraft-version-mismatch").find("jei"), std::string::npos);
    EXPECT_NE(attribution.actionStep("entrypoint-failure").find("jei"), std::string::npos);
    EXPECT_NE(attribution.actionStep("some-other-rule").find("Update or remove jei"),
              std::string::npos);

    ModAttribution packageOnly;
    packageOnly.identified = true;
    packageOnly.ownerPackage = "com.example.coolutil";
    EXPECT_NE(packageOnly.actionStep("some-rule").find("com.example.coolutil"), std::string::npos);
}

TEST(RulesNameModsTest, EngineAppendsModSpecificFix) {
    const std::vector<Diagnostic> diagnostics =
        LogAnalyzer().analyzeEntries({makeEntry(LogLevel::ERROR, "Could not find entrypoint for mod coolutil")})
            .diagnostics;
    const Diagnostic* entrypoint = findRule(diagnostics, "entrypoint-failure");
    ASSERT_NE(entrypoint, nullptr);
    EXPECT_EQ(entrypoint->mod.modId, "coolutil");
    EXPECT_TRUE(anySuggestionNames(*entrypoint, "coolutil"));
}

TEST(RulesNameModsTest, MissingDependencyNamesTheDependency) {
    const std::vector<Diagnostic> diagnostics =
        LogAnalyzer().analyzeEntries({makeEntry(LogLevel::ERROR, "Missing dependency: jei")})
            .diagnostics;
    const Diagnostic* dependency = findRule(diagnostics, "missing-mandatory-dependency");
    ASSERT_NE(dependency, nullptr);
    EXPECT_EQ(dependency->mod.modId, "jei");
    EXPECT_TRUE(anySuggestionNames(*dependency, "jei"));
}

TEST(RulesNameModsTest, FabricLogFileNamesMods) {
    const AnalysisResult result = LogAnalyzer().analyzeFile(fixturePath("fabric_modpack.log"));
    ASSERT_TRUE(result.ok) << result.error;

    const Diagnostic* entrypoint = findRule(result.diagnostics, "entrypoint-failure");
    ASSERT_NE(entrypoint, nullptr);
    EXPECT_EQ(entrypoint->mod.modId, "coolutil");
    EXPECT_TRUE(anySuggestionNames(*entrypoint, "coolutil"));

    const Diagnostic* duplicate = findRule(result.diagnostics, "duplicate-mod-id");
    ASSERT_NE(duplicate, nullptr);
    EXPECT_EQ(duplicate->mod.modId, "examplemod");
    EXPECT_TRUE(anySuggestionNames(*duplicate, "examplemod"));
}

TEST(RulesNameModsTest, CrashReportAttributionIncludesVersion) {
    const AnalysisResult result =
        LogAnalyzer().analyzeFile(fixturePath("crash_report_fabric.txt"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.modRegistry.size(), 3u);

    const Diagnostic* entrypoint = findRule(result.diagnostics, "entrypoint-failure");
    ASSERT_NE(entrypoint, nullptr);
    EXPECT_EQ(entrypoint->mod.modId, "coolutil");
    EXPECT_EQ(entrypoint->mod.modVersion, "4.5.6");
    EXPECT_NE(entrypoint->mod.label().find("4.5.6"), std::string::npos);
    EXPECT_TRUE(anySuggestionNames(*entrypoint, "coolutil"));
}

TEST(ModAttributionTest, NeverSuggestsUpdatingOrRemovingMinecraft) {
    ModAttribution attribution;
    attribution.identified = true;
    attribution.modId = "minecraft";
    attribution.modName = "Minecraft";
    attribution.modVersion = "1.21.11";
    attribution.frame = "net.minecraft.class_1439.method_37908()";

    EXPECT_TRUE(attribution.isPlatform());
    const std::string fix = attribution.actionStep("class-not-found");
    EXPECT_EQ(fix.find("Update or remove minecraft"), std::string::npos);
    EXPECT_EQ(fix.find("Update or remove Minecraft"), std::string::npos);
    EXPECT_NE(fix.find("different Minecraft version"), std::string::npos);
}

TEST(ModAttributionTest, MatchPackageIgnoresMinecraftAndLoaders) {
    ModRegistry registry;
    registry.add({"minecraft", "Minecraft", "1.21.11", "crash report"});
    registry.add({"fabricloader", "Fabric Loader", "0.16.0", "crash report"});
    registry.add({"sodium", "Sodium", "0.5.8", "crash report"});

    // Platform packages should not match platform mods
    EXPECT_EQ(registry.matchPackage("net.minecraft.class_1439"), nullptr);
    EXPECT_EQ(registry.matchPackage("net.fabricmc.loader.impl.launch.knot"), nullptr);

    // Actual third-party mods match
    const ModRef* matched = registry.matchPackage("me.jellysquid.mods.sodium.client");
    ASSERT_NE(matched, nullptr);
    EXPECT_EQ(matched->id, "sodium");
}
