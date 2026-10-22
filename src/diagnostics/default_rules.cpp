#include "diagnostics/default_rules.h"

#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>

#include "analyzer/exception_extractor.h"
#include "models/diagnostic.h"
#include "utils/strings.h"

namespace minesleuth {
namespace {

constexpr std::size_t kRepeatThreshold = 5;

std::string evidenceOf(const std::string& text) {
    std::string collapsed = strings::collapseWhitespace(text);
    if (collapsed.size() > 160) {
        collapsed = collapsed.substr(0, 157) + "...";
    }
    return collapsed;
}

std::string truncate(const std::string& text, std::size_t limit) {
    std::string collapsed = strings::collapseWhitespace(text);
    if (collapsed.size() <= limit) {
        return collapsed;
    }
    std::size_t cutoff = collapsed.rfind(' ', limit > 3 ? limit - 3 : limit);
    if (cutoff == std::string::npos || cutoff < limit / 2) {
        cutoff = limit > 3 ? limit - 3 : limit;
    }
    while (cutoff > 0 && (collapsed[cutoff - 1] == ',' || collapsed[cutoff - 1] == ';' ||
                          collapsed[cutoff - 1] == ':' || collapsed[cutoff - 1] == '-' ||
                          collapsed[cutoff - 1] == '.' || collapsed[cutoff - 1] == ' ')) {
        --cutoff;
    }
    return collapsed.substr(0, cutoff) + "...";
}

std::string searchText(const LogEntry& entry) {
    std::string text = entry.message;
    if (entry.trace.has_value()) {
        text.push_back('\n');
        text += entry.trace->rawText;
    }
    return text;
}

std::string lineContaining(const std::string& text, std::size_t position) {
    const std::size_t start = text.rfind('\n', position);
    const std::size_t begin = start == std::string::npos ? 0 : start + 1;
    const std::size_t end = text.find('\n', position);
    return evidenceOf(text.substr(begin, (end == std::string::npos ? text.size() : end) - begin));
}

class PatternRule : public DiagnosticRule {
public:
    struct Spec {
        std::string id;
        std::string pattern;
        Severity severity = Severity::Error;
        Confidence confidence = Confidence::Likely;
        std::string title;
        std::string explanation;
        std::vector<std::string> suggestions;
        bool refinePathConfidence = false;
        bool scanInformational = false;
    };

    explicit PatternRule(Spec spec) : spec_(std::move(spec)) {
        regex_ = std::regex(spec_.pattern, std::regex::icase);
    }

    std::string id() const override { return spec_.id; }

    std::vector<Diagnostic> evaluate(const RuleContext& context) const override {
        std::vector<std::size_t> matched;
        std::unordered_map<std::size_t, std::string> evidenceByEntry;
        for (std::size_t i = 0; i < context.entries.size(); ++i) {
            const LogEntry& entry = context.entries[i];
            if (!spec_.scanInformational && !isProblemEntry(entry)) {
                continue;
            }
            const std::string text = searchText(entry);
            std::smatch match;
            if (!std::regex_search(text, match, regex_)) {
                continue;
            }
            matched.push_back(i);
            evidenceByEntry[i] = lineContaining(text, static_cast<std::size_t>(match.position(0)));
        }
        if (matched.empty()) {
            // Crash report descriptions are stored outside the log entries. Let rules
            // inspect that text too, so description-only failures (for example
            // "Ticking entity") still produce a specific diagnosis.
            if (context.crashReport == nullptr || context.crashReport->description.empty()) {
                return {};
            }
            const CrashReport& report = *context.crashReport;
            std::smatch descriptionMatch;
            if (!std::regex_search(report.description, descriptionMatch, regex_)) {
                return {};
            }
            Diagnostic diagnostic;
            diagnostic.ruleId = spec_.id;
            diagnostic.title = spec_.title;
            diagnostic.explanation = spec_.explanation;
            diagnostic.suggestions = spec_.suggestions;
            diagnostic.confidence = spec_.confidence;
            diagnostic.severity = spec_.severity;
            diagnostic.occurrences = 1;
            diagnostic.sourceLine = report.descriptionLine;
            diagnostic.entryIndex = kNoEntry;
            diagnostic.evidence = evidenceOf(report.description);
            diagnostic.sourceLabel = "crash report";
            return {std::move(diagnostic)};
        }

        std::unordered_map<std::size_t, std::size_t> entryToGroup;
        for (std::size_t g = 0; g < context.groups.size(); ++g) {
            for (std::size_t index : context.groups[g].entryIndices) {
                entryToGroup[index] = g;
            }
        }

        std::vector<std::string> bucketOrder;
        std::unordered_map<std::string, std::vector<std::size_t>> buckets;
        for (std::size_t index : matched) {
            const auto it = entryToGroup.find(index);
            const std::string key = it == entryToGroup.end()
                                        ? "e:" + std::to_string(index)
                                        : "g:" + std::to_string(it->second);
            if (buckets.find(key) == buckets.end()) {
                bucketOrder.push_back(key);
            }
            buckets[key].push_back(index);
        }

        static const std::regex pathish(
            R"([A-Za-z]:\\[^\s]+|/[^\s]+|[^\s]+\.(?:json|log|txt|cfg|toml|jar|png|dat|dll|so|zip|mcmeta|class|java|yml|yaml|properties))",
            std::regex::icase);

        std::vector<Diagnostic> diagnostics;
        for (const std::string& key : bucketOrder) {
            const std::vector<std::size_t>& members = buckets[key];
            Diagnostic diagnostic;
            diagnostic.ruleId = spec_.id;
            diagnostic.title = spec_.title;
            diagnostic.explanation = spec_.explanation;
            diagnostic.suggestions = spec_.suggestions;
            diagnostic.confidence = spec_.confidence;
            diagnostic.severity = spec_.severity;
            diagnostic.occurrences = members.size();
            diagnostic.sourceLine = context.entries[members.front()].line_number;
            diagnostic.entryIndex = members.front();
            diagnostic.evidence = evidenceByEntry[members.front()];
            diagnostic.sourceLabel = key.rfind("g:", 0) == 0 ? "grouped error" : "log entry";
            for (std::size_t index : members) {
                diagnostic.severity =
                    higherSeverity(diagnostic.severity, severityOf(context.entries[index].level));
            }
            if (spec_.refinePathConfidence) {
                diagnostic.confidence = std::regex_search(diagnostic.evidence, pathish)
                                            ? Confidence::Likely
                                            : Confidence::Unverified;
            }
            diagnostics.push_back(std::move(diagnostic));
        }
        return diagnostics;
    }

private:
    Spec spec_;
    std::regex regex_;
};

std::unique_ptr<DiagnosticRule> pattern(std::string id, std::string regexPattern,
                                        Severity severity, Confidence confidence, std::string title,
                                        std::string explanation,
                                        std::vector<std::string> suggestions,
                                        bool refinePathConfidence = false,
                                        bool scanInformational = false) {
    PatternRule::Spec spec{std::move(id),
                           std::move(regexPattern),
                           severity,
                           confidence,
                           std::move(title),
                           std::move(explanation),
                           std::move(suggestions),
                           refinePathConfidence,
                           scanInformational};
    return std::make_unique<PatternRule>(std::move(spec));
}

class RepeatedErrorRule : public DiagnosticRule {
public:
    std::string id() const override { return "repeated-error-cluster"; }
    bool attributesCause() const override { return false; }

    std::vector<Diagnostic> evaluate(const RuleContext& context) const override {
        std::vector<Diagnostic> diagnostics;
        for (const ErrorGroup& group : context.groups) {
            if (group.count < kRepeatThreshold || group.entryIndices.empty()) {
                continue;
            }
            const std::size_t firstEntry = group.entryIndices.front();
            Diagnostic diagnostic;
            diagnostic.ruleId = id();
            diagnostic.title = "Repeated error (" + std::to_string(group.count) + "x)";
            diagnostic.explanation =
                "The same normalized error signature occurs " + std::to_string(group.count) +
                " times, starting at line " +
                std::to_string(context.entries[firstEntry].line_number) +
                ". Repetition usually means the failing operation runs every tick or touches many "
                "objects, so resolving the first occurrence normally clears the rest.";
            diagnostic.suggestions = {
                "Investigate the first occurrence; later ones are usually noise.",
                "If the timestamps are evenly spaced, the error repeats on a loop or per tick.",
                "Fix the underlying condition once instead of handling each occurrence.",
            };
            diagnostic.severity = severityOf(group.severity);
            diagnostic.confidence = Confidence::Confirmed;
            diagnostic.evidence = evidenceOf(group.displayMessage);
            diagnostic.sourceLine = context.entries[firstEntry].line_number;
            diagnostic.entryIndex = firstEntry;
            diagnostic.sourceLabel = "grouped error";
            diagnostic.occurrences = group.count;
            diagnostics.push_back(std::move(diagnostic));
        }
        return diagnostics;
    }
};

class CrashReportDescriptionRule : public DiagnosticRule {
public:
    std::string id() const override { return "crash-report-description"; }
    bool attributesCause() const override { return false; }

    std::vector<Diagnostic> evaluate(const RuleContext& context) const override {
        if (context.crashReport == nullptr || context.crashReport->description.empty()) {
            return {};
        }
        const CrashReport& report = *context.crashReport;

        std::string systemContext;
        const auto appendDetail = [&](const char* key) {
            const std::string value = report.systemDetail(key);
            if (value.empty()) {
                return;
            }
            if (!systemContext.empty()) {
                systemContext += ", ";
            }
            systemContext += std::string(key) + " " + value;
        };
        appendDetail("Minecraft Version");
        appendDetail("Operating System");
        appendDetail("Java Version");

        Diagnostic diagnostic;
        diagnostic.ruleId = id();

        const std::string cleanDesc = strings::collapseWhitespace(report.description);
        if (cleanDesc.size() <= 60 && cleanDesc.find(':') == std::string::npos) {
            diagnostic.title = "Crash: " + cleanDesc;
        } else {
            // If the description has a clean first sentence before a period, use that.
            std::size_t dotPos = cleanDesc.find(". ");
            if (dotPos != std::string::npos && dotPos <= 100) {
                diagnostic.title = "Crash: " + cleanDesc.substr(0, dotPos);
            } else {
                diagnostic.title = "Game-Reported Crash: " + truncate(cleanDesc, 60);
            }
        }

        diagnostic.explanation =
            "The game wrote its own description for this crash: \"" +
            cleanDesc + "\"." +
            (systemContext.empty() ? "" : " Reported environment: " + systemContext + ".");
        diagnostic.suggestions = {
            "Reproduce the crash with the same mod set to confirm it is stable.",
            "Search the description text verbatim for known issues with your mod list.",
            "Temporarily remove recently added mods to isolate the trigger.",
        };
        diagnostic.severity = Severity::Error;
        diagnostic.confidence = Confidence::Confirmed;
        diagnostic.evidence = evidenceOf(report.description);
        diagnostic.sourceLine = report.descriptionLine;
        diagnostic.entryIndex = kNoEntry;
        diagnostic.sourceLabel = "crash report";
        return {std::move(diagnostic)};
    }
};

class UnrecognizedErrorRule : public DiagnosticRule {
public:
    std::string id() const override { return "unrecognized-error"; }
    bool isFallback() const override { return true; }
    bool attributesCause() const override { return false; }

    std::vector<Diagnostic> evaluate(const RuleContext& context) const override {
        std::vector<Diagnostic> diagnostics;
        for (const ErrorGroup& group : context.groups) {
            if (group.entryIndices.empty()) {
                continue;
            }
            Severity severity = severityOf(group.severity);
            const bool hasTrace = group.sampleTrace.has_value();
            if (hasTrace) {
                severity = higherSeverity(severity, Severity::Error);
            }
            const bool significant =
                severityRank(severity) >= severityRank(Severity::Error) || hasTrace;
            if (!significant) {
                continue;
            }

            bool matched = false;
            for (std::size_t index : group.entryIndices) {
                if (context.matchedEntryIndices.count(index) > 0) {
                    matched = true;
                    break;
                }
            }
            if (matched) {
                continue;
            }

            const std::size_t firstEntry = group.entryIndices.front();
            const std::string message = truncate(group.displayMessage, 100);
            Diagnostic diagnostic;
            diagnostic.ruleId = id();
            diagnostic.title = group.exceptionClass.empty()
                                   ? "Unrecognized error: " + message
                                   : "Unhandled " + group.exceptionClass +
                                         (message.empty() ? "" : ": " + message);
            diagnostic.explanation =
                "No known-issue rule matched this error with the available evidence, so no "
                "specific root cause can be claimed." +
                (group.topFrame.has_value()
                     ? " The stack trace points at " + *group.topFrame + "."
                     : " The log does not include a usable stack trace for it.");
            diagnostic.suggestions = {
                "Read the full stack trace and find the deepest frame you recognise (your own "
                "mod, a config file, or a library).",
                "Check mods or configs that were recently added, updated, or removed.",
                "Search the exact exception message for known issues with this mod set.",
            };
            if (group.topFrame.has_value()) {
                diagnostic.suggestions.push_back("Top frame: " + *group.topFrame +
                                                 " - check whether that class belongs to a mod "
                                                 "you can update or remove.");
            }
            diagnostic.severity = severity;
            diagnostic.confidence = Confidence::Unverified;
            diagnostic.evidence = evidenceOf(group.displayMessage);
            diagnostic.sourceLine = context.entries[firstEntry].line_number;
            diagnostic.entryIndex = firstEntry;
            diagnostic.sourceLabel = "grouped error";
            diagnostic.occurrences = group.count;
            diagnostics.push_back(std::move(diagnostic));
        }
        return diagnostics;
    }
};

}  // namespace

std::vector<std::unique_ptr<DiagnosticRule>> createDefaultRules() {
    std::vector<std::unique_ptr<DiagnosticRule>> rules;

    rules.push_back(pattern(
        "missing-mandatory-dependency",
        R"(missing (?:or unsupported )?(?:mandatory |required )?(?:mod )?dependenc\w*|missing (?:required|mandatory) mod|depends? on (?:mod |the mod)|could not find (?:the )?(?:required|mandatory) (?:mod|dependenc)\w*|could not find required mod|requires? (?:the )?mod\b)",
        Severity::Error, Confidence::Confirmed, "Missing mod dependency",
        "The log explicitly reports a mod dependency that is absent or unsupported, so the "
        "dependent mod cannot be resolved.",
        {"Install the dependency named in the evidence line.",
         "Compare the required dependency list of the failing mod with the mods you installed.",
         "Remove the dependent mod if its dependency is unavailable for your Minecraft version."}));

    rules.push_back(pattern(
        "mixin-injection-failure",
        R"(invalidinjectionexception|injectionerror|injector.{0,40}(?:failed|error|invalid)|failed to apply mixin|mixin.{0,80}(?:failed|failure|could not|cannot)|@inject annotation on.{0,160}(?:could not|failed)|softimplement.{0,40}(?:failed|error))",
        Severity::Error, Confidence::Confirmed, "Mixin injection failure",
        "A Mixin transformation could not be applied, which means a mod targeted code that is "
        "not present in this Minecraft or mod version.",
        {"Update or remove the mod that owns the failing mixin.",
         "Check that every mod matches your exact Minecraft version.",
         "If the report names a target class, look for another mod already changing that class."}));

    rules.push_back(pattern(
        "entrypoint-failure",
        R"(entrypoint.{0,80}(?:fail|failed|error|exception|could not|unable)|(?:error|failed|unable|could not) (?:to )?(?:load|find|invoke|resolve).{0,30}entrypoint)",
        Severity::Error, Confidence::Confirmed, "Mod entrypoint failed to load",
        "The mod loader could not construct or run a mod entrypoint, so that mod never "
        "registered its content.",
        {"Check the stack trace for the mod that owns the entrypoint.",
         "Look for a missing or incompatible dependency of that mod.",
         "Remove the mod temporarily to confirm it is the source of the crash."}));

    rules.push_back(pattern(
        "class-initializer-failure",
        R"(exceptionininitializererror)",
        Severity::Error, Confidence::Likely, "Class failed to initialize",
        "A static initializer threw an exception while a class was being loaded, which usually "
        "means bad configuration or a missing resource read during class setup.",
        {"Inspect the cause chain below the ExceptionInInitializerError.",
         "Check config files the mod reads during startup.",
         "Delete or reset the mod's config file if it may be corrupt."}));

    rules.push_back(pattern(
        "class-not-found",
        R"(noclassdeffounderror|classnotfoundexception|nosuchmethoderror|nosuchfielderror)",
        Severity::Error, Confidence::Likely, "Class or member not found",
        "Code referenced a class, method, or field that is not on the classpath, which usually "
        "means a missing dependency or a version mismatch between mods.",
        {"Check whether the class name in the evidence belongs to a dependency you have not "
         "installed.",
         "Verify that dependent mods are all built for the same Minecraft version.",
         "Update both the failing mod and its library dependencies together."}));

    rules.push_back(pattern(
        "out-of-memory",
        R"(outofmemoryerror|java heap space|gc overhead limit exceeded)",
        Severity::Critical, Confidence::Confirmed, "Out of memory",
        "The JVM ran out of heap space, so the game aborted the current operation or crashed.",
        {"Increase the memory allocation (for example -Xmx6G) in your launcher profile.",
         "Close other games or launchers that are holding memory.",
         "Reduce memory-heavy settings such as render distance, shaders, or large modpacks."},
        false));

    rules.push_back(pattern(
        "unsupported-class-version",
        R"(unsupportedclassversionerror|compiled by a newer (?:java|compiler)|invalid target release|release version \d+ (?:is )?not supported)",
        Severity::Error, Confidence::Confirmed, "Unsupported Java class version",
        "A mod or library was compiled for a newer Java version than the one running the game.",
        {"Update the Java runtime used by the launcher to the version the mod requires.",
         "Or install a build of the mod compiled for your current Java version.",
         "Check the mod page for its required Java version."}));

    rules.push_back(pattern(
        "missing-file",
        R"(filenotfoundexception|no such file or directory|cannot find (?:the )?file|could not (?:read|open|load|find) (?:the )?(?:file|path|directory)|failed to (?:load|read|open) (?:the )?(?:resource|file|path))",
        Severity::Warning, Confidence::Unverified, "Required file is missing",
        "The log reports that a file or directory could not be found, so a resource, config, or "
        "library the game expected is absent.",
        {"Confirm the path shown in the evidence still exists.",
         "Restore the file from a backup or let the mod regenerate it.",
         "If a mod was updated, remove stale leftover files from the previous version."},
        true));

    rules.push_back(pattern(
        "permission-denied",
        R"(permission denied|access denied|access is denied|accessdeniedexception)",
        Severity::Error, Confidence::Likely, "Access denied",
        "The operating system refused access to a file or directory the game tried to use.",
        {"Check that the game has read and write permission on the affected folder.",
         "Move the installation out of a protected directory such as Program Files.",
         "Run the launcher as the same user that owns the game files."}));

    rules.push_back(pattern(
        "disk-full",
        R"(no space left on device|disk (?:is )?(?:full|out of space)|not enough (?:disk |free )?space|there is not enough space)",
        Severity::Critical, Confidence::Confirmed, "Disk is full",
        "The disk holding the game or its data has no free space left.",
        {"Free disk space on the drive that holds the game and its saves.",
         "Clear old log files, crash reports, and unused mod archives.",
         "Re-run the operation after freeing space to confirm the error disappears."}));

    rules.push_back(pattern(
        "native-library-load",
        R"(unsatisfiedlinkerror|no lwjgl in java\.library\.path|can't find dependent libraries|cannot load (?:the )?(?:native|library|shared library)|no .{0,40} in java\.library\.path)",
        Severity::Error, Confidence::Likely, "Native library failed to load",
        "A native library (LWJGL, audio, or input) could not be loaded, which usually points at "
        "a broken or mismatched Java installation.",
        {"Run the launcher's Java repair or reinstall the bundled JRE.",
         "Make sure no other Java installation is shadowing the one the launcher uses.",
         "Verify the game files to restore missing native binaries."}));

    rules.push_back(pattern(
        "glfw-window-error",
        R"(glfw error|glfwinit failed|failed to create (?:the )?(?:glfw )?window|pixel format not accelerated|glfw not initialized)",
        Severity::Error, Confidence::Likely, "Window creation failed",
        "GLFW could not create the game window, which usually means outdated or missing GPU "
        "drivers rather than a problem with the game itself.",
        {"Update your graphics card drivers.",
         "Close overlays or other applications that hook into OpenGL.",
         "If you run multiple GPUs, force the dedicated GPU for the launcher."}));

    rules.push_back(pattern(
        "opengl-render-error",
        R"(gl_invalid_\w+|opengl error|gl_framebuffer_\w+|failed to compile (?:the )?(?:vertex |fragment )?shader|shader (?:compile|link) failed|error compiling shader)",
        Severity::Error, Confidence::Likely, "Graphics driver rejected a render operation",
        "The OpenGL driver reported an error or a shader failed to compile, which usually "
        "indicates outdated GPU drivers or a shader pack problem.",
        {"Update your graphics card drivers.",
         "Disable shaders or resource packs to confirm they are not the cause.",
         "Reduce video settings such as render distance or mipmap levels."}));

    rules.push_back(pattern(
        "network-connection-failure",
        R"(unknownhostexception|connection refused|connect(?:ion)? timed out|no route to host|connection reset by peer|failed to connect to|network is unreachable|unable to resolve (?:host|address))",
        Severity::Warning, Confidence::Likely, "Network connection failed",
        "The game could not reach a remote host, so multiplayer, Realms, or a server resource "
        "pack download failed.",
        {"Check the server address and that it is online.",
         "Verify your own internet connection, VPN, or firewall rules.",
         "Try again later if the host is rate-limiting or restarting."}));

    rules.push_back(pattern(
        "corrupted-archive",
        R"(zipexception|invalid (?:zip|jar|compressed data|entry)|corrupt(?:ed)? (?:jar|zip|file|archive)|end of zlib data stream|cen headers? (?:are )?missing)",
        Severity::Error, Confidence::Likely, "Corrupt archive or mod file",
        "A jar or zip file could not be read, which means the file is truncated or was damaged "
        "during download.",
        {"Delete the affected file and download it again.",
         "Verify the integrity of the mod files in your launcher.",
         "Check for disk errors if the corruption keeps recurring."}));

    rules.push_back(pattern(
        "duplicate-mod-id",
        R"(duplicate mod|two mods with the same id|duplicate mod id|found duplicate mod id)",
        Severity::Error, Confidence::Confirmed, "Duplicate mod id",
        "Two installed mods declare the same mod id, so the loader cannot decide which one to "
        "load.",
        {"Remove one of the two mods that claim the id named in the evidence.",
         "Check whether a mod was installed twice in different folders.",
         "Look for an updated build that renamed the conflicting id."}));

    rules.push_back(pattern(
        "minecraft-version-mismatch",
        R"(requires? minecraft [0-9]|built for minecraft|unsupported minecraft version|minecraft [0-9][0-9a-z.]* (?:or later|is required|required))",
        Severity::Error, Confidence::Confirmed, "Minecraft version mismatch",
        "A mod declares a Minecraft version other than the one running, so it was not loaded.",
        {"Install the mod build made for your Minecraft version.",
         "Or change the game version in the launcher to the one the mod requires.",
         "Check every mod in the pack for the same mismatch."}));

    rules.push_back(pattern(
        "loader-mismatch",
        R"(designed for (?:forge|fabric|neoforge|quilt)|requires? (?:the )?(?:forge|fabric|neoforge|quilt)(?: mod)? loader|wrong mod loader|installed (?:for|with) (?:forge|fabric|neoforge|quilt) but)",
        Severity::Error, Confidence::Confirmed, "Wrong mod loader",
        "A mod built for a different mod loader was installed into this instance.",
        {"Install the loader the mod was built for, or find a build for your loader.",
         "Confirm the instance's loader in the launcher before adding mods.",
         "Remove mods that belong to a different loader."}));

    rules.push_back(pattern(
        "file-locked",
        R"(the process cannot access the file because it is being used by another process|sharing violation|filesystemexception.{0,120}(?:in use|being used|locked)|(?:file|folder|directory) .{0,80}(?:is|are) (?:already )?(?:in use|locked|open in another process))",
        Severity::Error, Confidence::Likely, "File is locked by another process",
        "Another process (often a second game instance, a launcher, or antivirus) is holding a "
        "file open, so the game cannot read or write it.",
        {"Close other Minecraft instances, launchers, and editors that may have the file open.",
         "Wait a few seconds and retry, since antivirus scans can lock files briefly.",
         "Add the game folder to your antivirus exclusions if the lock keeps recurring."}));

    rules.push_back(pattern(
        "chunk-corruption",
        R"((?:corrupt|corrupted|bad|invalid) (?:chunk|region)(?: file)?|chunk (?:file )?(?:at|in) [^\s]+ (?:is )?(?:corrupt|missing|invalid)|(?:failed|error|unable) to (?:load|read|parse) (?:the )?chunk|couldn't load chunk|chunkregionloader\.load(?:chunk|entities)|exception generating new chunk|region file .{0,80}(?:corrupt|invalid|truncated))",
        Severity::Error, Confidence::Likely, "Chunk or region data is damaged",
        "The game could not read a chunk or region file, which usually means the save was "
        "interrupted or the file was damaged by a crash or disk problem.",
        {"Restore the affected region file from a backup before loading the world again.",
         "Run a world repair tool or let the game regenerate the affected chunk if losing it is "
         "acceptable.",
         "Check the disk for errors if corruption affects more than one region file."}));

    rules.push_back(pattern(
        "world-save-failure",
        R"((?:failed|could not|unable|error) (?:to )?(?:save|write) (?:the )?(?:world|level|chunk|player data)|(?:saving|world save) (?:failed|error)|(?:world|level) (?:could not|cannot|failed to) be saved|exception (?:while|during) saving)",
        Severity::Error, Confidence::Likely, "World could not be saved",
        "The game failed while writing world or player data, so recent progress may not have "
        "been persisted.",
        {"Free disk space and check that the save folder is writable before playing again.",
         "Close other programs that may have the world files open.",
         "Back up the world folder and check the log for the underlying I/O exception."}));

    rules.push_back(pattern(
        "json-parse-error",
        R"(jsonsyntaxexception|jsonparseexception|malformedjson(?:exception)?|json (?:parse|syntax|decoding) (?:error|exception|failed)|(?:parse|parsing) (?:error|exception).{0,40}json|unexpected (?:token|character|end of input).{0,60}(?:json|line \d+)|invalid json)",
        Severity::Error, Confidence::Likely, "Malformed JSON data",
        "A JSON file or response could not be parsed, so the mod or game component that reads "
        "it may skip its configuration or fail to start.",
        {"Find the JSON file named in the evidence and validate it with a JSON checker.",
         "Restore the file from a backup or delete it so the mod regenerates a clean default.",
         "Reinstall the mod if the malformed JSON ships inside its jar."}));

    rules.push_back(pattern(
        "config-parse-failure",
        R"((?:could not|failed|unable) (?:to )?(?:parse|read|load) (?:the )?config(?:uration)?(?: file)?|(?:invalid|malformed|corrupt(?:ed)?) config(?:uration)?(?: file)?|config(?:uration)?(?: file)? (?:is )?(?:invalid|malformed|corrupt(?:ed)?)|toml.{0,40}(?:parse|syntax) (?:error|exception))",
        Severity::Warning, Confidence::Likely, "Config file could not be parsed",
        "A mod's configuration file is invalid or unreadable, so the mod may fall back to "
        "defaults or skip loading.",
        {"Open the config file named in the evidence and check for syntax errors near the "
         "reported line.",
         "Restore the file from a backup or delete it to let the mod regenerate defaults.",
         "Avoid editing configs while the game is running, since the file may be rewritten on "
         "exit."}));

    rules.push_back(pattern(
        "data-pack-error",
        R"((?:failed|could not|unable|error) (?:to )?(?:load|read|parse|apply) (?:the )?data ?pack|data ?pack .{0,80}(?:failed|invalid|corrupt|could not (?:be )?(?:loaded|read|parsed))|invalid data ?pack|(?:error|exception) (?:while|during) (?:loading|reading) (?:the )?data ?pack)",
        Severity::Error, Confidence::Likely, "Data pack failed to load",
        "A data pack could not be loaded or parsed, so its recipes, loot tables, or world data "
        "are missing from the game.",
        {"Check that the data pack's pack.mcmeta is valid and matches the pack format for your "
         "game version.",
         "Remove recently added data packs one at a time to identify the broken one.",
         "Re-download the pack if its files may have been corrupted."}));

    rules.push_back(pattern(
        "resource-pack-error",
        R"((?:failed|could not|unable|error) (?:to )?(?:load|read|parse|apply) (?:the )?resource ?pack|resource ?pack .{0,80}(?:failed|invalid|corrupt|could not (?:be )?(?:loaded|read|parsed))|invalid resource ?pack|(?:error|exception) (?:while|during) (?:loading|applying) (?:the )?resource ?pack)",
        Severity::Warning, Confidence::Likely, "Resource pack failed to load",
        "A resource pack could not be loaded or parsed, so textures, models, or sounds from it "
        "will be missing or fall back to vanilla.",
        {"Check that the pack is built for your Minecraft version and has a valid pack.mcmeta.",
         "Remove recently added resource packs one at a time to identify the broken one.",
         "Re-download the pack if its files may have been corrupted."}));

    rules.push_back(pattern(
        "java-version-mismatch",
        R"((?:requires?|needs?) (?:java|jdk) (?:version )?\d+|(?:java|jdk) (?:version )?\d+ (?:or (?:newer|later|higher)|is required)|(?:unsupported|incompatible|wrong) java version|you are (?:running|using) (?:an? )?(?:unsupported|incompatible|outdated) (?:java|jdk))",
        Severity::Error, Confidence::Likely, "Wrong Java version",
        "A mod or the game requires a different Java version than the one the launcher started, "
        "so it cannot run on this runtime.",
        {"Set the launcher to use the Java version named in the evidence (17 or 21 are common "
         "for modern versions).",
         "Remove old Java installations from the launcher's Java list to avoid picking the "
         "wrong one.",
         "If no matching runtime is installed, install a current JDK and point the launcher at "
         "it."}));

    rules.push_back(pattern(
        "jvm-fatal-crash",
        R"(a fatal error has been detected by the java runtime environment|exception_access_violation|sigsegv|hs_err_pid|fatal error (?:in|on) (?:the )?(?:jvm|java runtime)|jvm (?:has )?crashed|java runtime environment has (?:crashed|encountered a fatal error))",
        Severity::Critical, Confidence::Confirmed, "Java runtime crashed",
        "The JVM itself terminated abnormally (a native crash), which points at a driver, "
        "native library, or hardware problem rather than a normal Java exception.",
        {"Open the hs_err_pid log named in the evidence and read the problematic frame.",
         "Update your graphics drivers and remove mods that ship native libraries.",
         "Test with a smaller mod set and default JVM arguments to rule out memory or driver "
         "pressure."}));

    rules.push_back(pattern(
        "jvm-argument-error",
        R"(unrecognized (?:vm|jvm) option|error: could not create the java virtual machine|error occurred during initialization of vm|could not reserve enough space for object heap|invalid (?:jvm|vm) (?:option|argument)|unable to create (?:the )?java virtual machine|missing (?:vm|jvm) option)",
        Severity::Critical, Confidence::Confirmed, "Invalid JVM arguments",
        "The launcher passed an option the JVM rejected or could not satisfy, so the game "
        "process never started.",
        {"Remove custom JVM arguments and let the launcher use its defaults.",
         "Check for a typo or an unsupported flag such as a GC option your Java version does "
         "not provide.",
         "Lower -Xmx if the requested heap is larger than the memory your system can reserve."}));

    rules.push_back(pattern(
        "stack-overflow",
        R"(stackoverflowerror|stack overflow)",
        Severity::Error, Confidence::Confirmed, "Stack overflow",
        "A call chain recursed until the thread ran out of stack space, which usually means a "
        "mod has an infinite loop or recursive event handler.",
        {"Find the repeating frame in the stack trace; the class it belongs to owns the loop.",
         "Update or remove the mod responsible for that frame.",
         "If the trace is dominated by a scripting or config library, test with it removed."}));

    rules.push_back(pattern(
        "concurrent-modification",
        R"(concurrentmodificationexception|concurrent modification(?: of| exception)?)",
        Severity::Error, Confidence::Likely, "Collection modified while in use",
        "Code changed a collection while another part of the game was iterating it, which is a "
        "thread-safety bug in the mod that owns the code.",
        {"Identify the mod frame closest to the ConcurrentModificationException.",
         "Update that mod; this class of bug is usually fixed in later builds.",
         "If no update exists, remove the mod or report the trace to its author."}));

    rules.push_back(pattern(
        "port-in-use",
        R"(address already in use|bindexception|failed to bind (?:to )?(?:port|address|server)|could not bind (?:to )?(?:port|address|server)|port \d+ (?:is )?(?:already )?in use)",
        Severity::Error, Confidence::Likely, "Server port is already in use",
        "The server could not claim its port because another process is already listening on it, "
        "so clients cannot connect.",
        {"Close any other server or game instance using the same port.",
         "Change server-port in server.properties if you need both instances to run.",
         "Check for leftover Java processes in Task Manager or Activity Monitor."}));

    rules.push_back(pattern(
        "authentication-failure",
        R"(authenticationexception|authentication servers? (?:are|is) (?:down|unreachable)|failed to authenticate|invalid session|session (?:has )?expired|invalid credentials|unable to (?:authenticate|verify) (?:the )?(?:user|session|account)|failed to read from https?://api\.minecraftservices\.com/publickeys)",
        Severity::Warning, Confidence::Likely, "Authentication failed",
        "The game could not verify the player's session with Mojang or Microsoft, so online "
        "features and multiplayer login were refused.",
        {"Restart the launcher and sign in again to refresh the session token.",
         "Check that the system clock is correct, since expired tokens often follow clock drift.",
         "Retry later if the authentication servers are reported as down."}));

    rules.push_back(pattern(
        "mod-load-failure",
        R"((?:failed|could not|unable) (?:to )?(?:load|initialize|initialise|construct) (?:the )?mod\b|mod .{0,60} (?:failed|could not) (?:to )?(?:load|initialize|initialise)|(?:error|exception) (?:while|during) (?:loading|initializing|initialising) (?:the )?mod\b|mod loading (?:failed|error(?:ed)?)|(?:has )?failed to load correctly|caught exception from [^\n]+|encountered an error during [^\n]{0,40}(?:loading|initialization))",
        Severity::Error, Confidence::Likely, "Mod failed to load",
        "The loader reported that a mod could not be loaded or initialized, so that mod's "
        "content is absent from the game.",
        {"Read the surrounding stack trace to find the mod and the failing class.",
         "Check that the mod's dependencies are installed and built for your game version.",
         "Reinstall the mod, then remove it temporarily to confirm it is the trigger."}));

    rules.push_back(pattern(
        "access-transformer-error",
        R"(access ?transformer.{0,60}(?:error|failed|invalid|exception|could not)|(?:failed|could not|unable) (?:to )?(?:apply|parse|read) (?:the )?access ?transformer|invalid access ?transformer)",
        Severity::Error, Confidence::Likely, "Access transformer failed",
        "A mod's access transformer could not be applied or parsed, so its widened access rules "
        "are missing and dependent code may fail later.",
        {"Check the accesstransformer.cfg named in the evidence for invalid or outdated entries.",
         "Update the mod that ships the transformer to a build for your game version.",
         "Remove the mod if the transformer error persists after reinstalling it."}));

    rules.push_back(pattern(
        "remap-failure",
        R"(failed to remap|remap(?:ping)? (?:failed|error)|remap(?:ping)? exception|(?:error|exception) (?:while|during) remap(?:ping)?|invalid (?:mapping|remap))",
        Severity::Error, Confidence::Likely, "Class remapping failed",
        "The loader could not remap classes or members between mappings, which usually means "
        "mismatched mod, loader, or Minecraft versions.",
        {"Verify that every mod and the loader target the same Minecraft version.",
         "Update the failing mod and the loader together.",
         "Remove mods that were built against a different mapping set."}));

    rules.push_back(pattern(
        "mixin-config-error",
        R"((?:mixins?\.json|mixin config).{0,80}(?:invalid|failed|failure|could not|not found|missing|corrupt|malformed)|(?:invalid|could not (?:read|parse|find|load)|failed to (?:read|parse|load)) (?:the )?(?:mixin config|mixins?\.json))",
        Severity::Error, Confidence::Likely, "Mixin configuration is invalid",
        "A mod's mixin configuration could not be read or validated, so its patches were never "
        "applied.",
        {"Check the mixin JSON named in the evidence for syntax errors or missing targets.",
         "Reinstall the mod to restore its bundled mixin configuration.",
         "Update the mod if its mixin config targets a Minecraft version you are not running."}));

    // The message formats below were cross-checked against the MIT-licensed
    // Aternos codex-minecraft analyser (https://github.com/aternosorg/codex-minecraft),
    // which powers the mclo.gs insights.

    rules.push_back(pattern(
        "mod-conflict",
        R"(incompatible mod set|(?:is incompatible|conflicts) with [^\n]{0,160}of (?:mod )?'[^']+'|mod [^\n]{0,80} (?:is )?incompatible with (?:mod )?[^\n]{0,80})",
        Severity::Error, Confidence::Confirmed, "Mods are incompatible",
        "The log explicitly reports two mods that cannot run together, usually because one "
        "targets a version or API the other does not support.",
        {"Remove one of the conflicting mods named in the evidence.",
         "Update both mods to versions that declare compatibility with each other.",
         "Check the mod pages for a known incompatibility and a recommended alternative."}));

    rules.push_back(pattern(
        "duplicate-mod-version",
        R"(duplicate versions? for mod(?: id)? '[^']+'|found duplicate mod [^\n]{0,120} at \[[^\]]+\])",
        Severity::Error, Confidence::Confirmed, "Two copies of the same mod",
        "The loader found more than one build of the same mod id, so it cannot decide which "
        "version to load.",
        {"Delete the older duplicate jar named in the evidence.",
         "Check the mods folder for copies with different filenames but the same mod id.",
         "Keep only the build that matches your Minecraft version and loader."}));

    rules.push_back(pattern(
        "fabric-entrypoint-stage",
        R"(could not execute entrypoint stage '[^']+' due to errors, provided by '[^']+')",
        Severity::Error, Confidence::Confirmed, "Fabric entrypoint threw an error",
        "A Fabric mod's entrypoint ran but threw an exception, so the mod did not finish "
        "registering its content.",
        {"Read the exception below the entrypoint message to find the failing class.",
         "Reinstall or update the mod that provides the entrypoint.",
         "Remove the mod temporarily to confirm it is the source of the crash."}));

    rules.push_back(pattern(
        "mixin-transformer-error",
        R"(mixintransformererror|mixin transformer (?:error|failed|crashed)|an unexpected critical error was encountered[^\n]{0,80}mixin)",
        Severity::Error, Confidence::Confirmed, "Mixin transformer crashed",
        "The Mixin transformer hit a critical error while applying patches, which usually means "
        "a mod is incompatible with this Minecraft or loader version.",
        {"Find the mod named in the Caused by section of the mixin stack trace.",
         "Update or remove the mod that owns the failing mixin.",
         "Check whether another mod already modifies the same target class."}));

    rules.push_back(pattern(
        "mod-wrong-minecraft-version",
        R"(the mod \S+ does not wish to run in minecraft version minecraft [0-9][0-9a-z.\-]*|this mod (?:is|was) (?:built|made) for minecraft [^,\n]{0,40}(?:but|and not))",
        Severity::Error, Confidence::Confirmed, "Mod refuses this Minecraft version",
        "A mod explicitly rejected the running Minecraft version, so it was not loaded.",
        {"Install the build of that mod made for your exact Minecraft version.",
         "Or switch the instance to the Minecraft version the mod declares.",
         "Remove the mod if no matching build exists."}));

    rules.push_back(pattern(
        "world-missing-mod",
        R"(this world was saved with mod (?!forge\b|minecraft\b)[^\s]+ which appears to be missing)",
        Severity::Warning, Confidence::Confirmed, "World references a missing mod",
        "The save was created with a mod that is no longer installed, so blocks, entities, or "
        "data from that mod may be lost or cause errors.",
        {"Reinstall the missing mod before loading the world again.",
         "Restore a backup of the world if the mod cannot be reinstalled.",
         "If you accept the loss, make a backup and let the game remove unknown content."}));

    rules.push_back(pattern(
        "world-mod-version-change",
        R"(this world was saved with mod [^\s]+ version \S+ and it is now at version [^,\n]+)",
        Severity::Warning, Confidence::Confirmed, "World was saved with a different mod version",
        "The save was created with another version of an installed mod, which can leave "
        "incompatible blocks, entities, or data behind.",
        {"Back up the world before loading it with the new mod version.",
         "Reinstall the original mod version if the world fails to load correctly.",
         "Check the mod's changelog for world-data migration steps."},
        false, true));

    rules.push_back(pattern(
        "fml-confirm-required",
        R"(forge mod loader detected that the backup level\.dat is being used|run the command /fml confirm|/fml confirm (?:or|and) /fml cancel)",
        Severity::Warning, Confidence::Confirmed, "Forge is waiting for confirmation",
        "The server stopped during startup to ask whether the world should be loaded after a "
        "mod change, so it will not finish starting until it gets an answer.",
        {"Run /fml confirm in the server console if the mod change is intentional.",
         "Run /fml cancel instead if you want to stop the load.",
         "For unattended servers, set -Dfml.queryResult=confirm in the startup arguments."}));

    rules.push_back(pattern(
        "missing-datapack-mod",
        R"(missing data ?pack mod ?: ?[a-z0-9_.\-]+)",
        Severity::Error, Confidence::Confirmed, "Data pack depends on a missing mod",
        "A data pack references content from a mod that is not installed, so its recipes or "
        "world data cannot be loaded.",
        {"Install the mod named after the colon in the evidence.",
         "Remove the data pack if you no longer use that mod.",
         "Check that the data pack and mod target the same Minecraft version."}));

    rules.push_back(pattern(
        "mod-module-conflict",
        R"(module\.resolutionexception: modules [^ ]+ and [^ ]+ export package|module\.resolutionexception: module [^ ]+ contains package [^ ]+, module [^ ]+ exports package)",
        Severity::Error, Confidence::Confirmed, "Mods export the same Java package",
        "Two mods ship the same Java package in their module path, so the JVM refused to start "
        "with both of them.",
        {"Remove one of the two mods named in the evidence.",
         "Update both mods to builds that no longer bundle the conflicting package.",
         "Check for a mod that bundles a library already provided by another mod."}));

    rules.push_back(pattern(
        "malformed-unicode-encoding",
        R"(malformed \\+uxxxx encoding|malformed \\+u[0-9a-f]{4} encoding)",
        Severity::Error, Confidence::Confirmed, "Malformed unicode escape in a config",
        "A properties or config file contains an invalid \\uXXXX escape, so the game failed "
        "while loading it.",
        {"Open the file named in the stack trace and fix or remove the invalid \\uXXXX escape.",
         "Restore the file from a backup or delete it to regenerate defaults.",
         "Save the file as UTF-8 if it contains non-ASCII text."}));

    rules.push_back(pattern(
        "ticking-block-entity",
        R"((?:description: )?ticking block entity|-- block entity being ticked --)",
        Severity::Critical, Confidence::Confirmed, "Block entity crashed the game",
        "A block entity threw an exception while ticking, and the crash report identifies the "
        "block type and location responsible.",
        {"Find the block location in the crash report's 'Block entity being ticked' section.",
         "Back up the world, then remove that block with a world editor or by loading a backup.",
         "Update or remove the mod that owns the block entity."}));

    rules.push_back(pattern(
        "ticking-entity",
        R"((?:description: )?ticking entity|-- entity being ticked --)",
        Severity::Critical, Confidence::Confirmed, "Entity crashed the game",
        "An entity threw an exception while ticking, and the crash report identifies its type "
        "and exact location.",
        {"Find the entity type and coordinates in the crash report's 'Entity being ticked' section.",
         "Back up the world, then remove that entity with a world editor or command.",
         "Update or remove the mod that owns the entity."}));

    rules.push_back(pattern(
        "plugin-runtime-error",
        R"(could not pass event \w+ to \w+|task \S+ for \w+ \S+ generated an exception)",
        Severity::Error, Confidence::Likely, "Server plugin threw an error",
        "A server plugin failed while handling an event or a scheduled task, so that action was "
        "aborted.",
        {"Read the stack trace to find the plugin class that threw the exception.",
         "Update the named plugin; event and scheduler bugs are usually fixed in later builds.",
         "Remove the plugin temporarily to confirm it is the cause."}));

    rules.push_back(pattern(
        "world-duplicate",
        R"(world [^\n]{0,80} is a duplicate of another world and has been prevented from loading|delete the uid\.dat file from [^\n]{0,80} world directory)",
        Severity::Error, Confidence::Confirmed, "Two worlds share the same identity",
        "The server found two world folders with the same uid.dat identity and refused to load "
        "the duplicate.",
        {"Delete the uid.dat file in the duplicate world folder named in the evidence.",
         "Or move the duplicate world to a separate server instance.",
         "Back up both worlds before changing their identity files."}));

    rules.push_back(pattern(
        "unsupported-api-version",
        R"(unsupported api version \d+\.\d+|invalidpluginexception: unsupported api version)",
        Severity::Error, Confidence::Confirmed, "Plugin targets an unsupported API version",
        "A server plugin was built for a Bukkit API version the server no longer accepts, so it "
        "was not loaded.",
        {"Update the plugin to a build for your server version.",
         "Or run the server version the plugin declares support for.",
         "Remove the plugin if no compatible build is available."}));

    rules.push_back(pattern(
        "paper-chunk-version-downgrade",
        R"(server attempted to load chunk saved with newer version of minecraft! \d+ > \d+)",
        Severity::Error, Confidence::Confirmed, "World was saved by a newer Minecraft version",
        "The server tried to load a chunk written by a newer Minecraft version, which cannot be "
        "downgraded safely.",
        {"Run the server with the Minecraft version that last saved the world.",
         "Restore a world backup made before the upgrade.",
         "Do not downgrade a world in place; copy it and test the downgrade separately."}));

    rules.push_back(std::make_unique<RepeatedErrorRule>());
    rules.push_back(std::make_unique<CrashReportDescriptionRule>());
    rules.push_back(std::make_unique<UnrecognizedErrorRule>());
    return rules;
}

}  // namespace minesleuth
