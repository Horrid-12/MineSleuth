#include "analyzer/mod_attribution.h"

#include <algorithm>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>

#include "utils/strings.h"

namespace minesleuth {
namespace {

// Mod ids written into loader or error messages.
const std::vector<std::regex>& mentionPatterns() {
    static const std::vector<std::regex> patterns = {
        std::regex(
            R"(mod id(?:\s+(?:detected|found|conflict(?:ed|ing)?))?\s*[:\-]\s*([a-z0-9][\w.-]*))",
            std::regex::icase),
        std::regex(R"(same id\s*[:\-]\s*([a-z0-9][\w.-]*))", std::regex::icase),
        std::regex(
            R"((?:missing|absent|unsupported|unknown|broken)\s+(?:or\s+\w+\s+)?(?:mandatory\s+|required\s+)?(?:mod\s+)?dependenc\w*\s*[:\-]\s*(?:mod\s+)?([a-z0-9][\w.-]*))",
            std::regex::icase),
        std::regex(R"((?:for|of)\s+(?:the\s+)?mod\s+([a-z0-9][\w.-]*))", std::regex::icase),
        std::regex(
            R"((?:depends?\s+on|requires?\s+the\s+mod\s+|required\s+(?:mod\s+)?|missing\s+mod\s+)([a-z0-9][\w.-]*))",
            std::regex::icase),
        std::regex(
            R"(mod\s+([a-z0-9][\w.-]*)\s+(?:requires?|is\s+incompatible|was\s+built|does\s+not|cannot|can't|failed))",
            std::regex::icase),
    };
    return patterns;
}

// Words that may follow "mod " but are never a mod id.
const std::unordered_set<std::string>& stopwords() {
    static const std::unordered_set<std::string> words = {
        "mod",      "mods",        "id",         "ids",        "the",
        "a",        "an",          "for",        "in",         "on",
        "of",       "and",         "or",         "to",         "from",
        "with",     "detected",    "found",      "failed",     "failure",
        "requires", "require",     "dependency", "dependencies", "mandatory",
        "required", "unsupported", "missing",    "unknown",    "loader",
        "file",     "files",       "pack",       "directory",  "path",
        "name",     "version",     "tab",        "menu",       "list",
        "entrypoint", "class",     "jar",        "zip",        "minecraft",
        "forge",    "fabric",      "neoforge",   "quilt",      "java",
        "when",     "while",       "but",        "not",        "this",
        "that",     "installed",   "loading",    "loaded",     "load",
        "init",     "common",      "client",     "server",     "api",
        "example",  "test",        "null",       "none",       "any",
        "all",      "each",        "both",       "into",       "is",
        "are",      "was",         "were",       "it",         "its",
    };
    return words;
}

std::string cleanToken(std::string token) {
    while (!token.empty() &&
           (token.back() == '.' || token.back() == '-' || token.back() == '_')) {
        token.pop_back();
    }
    return token;
}

bool plausibleModId(const std::string& token) {
    if (token.size() < 3) {
        return false;
    }
    const std::string lower = strings::toLower(token);
    if (isPlatformModId(lower)) {
        return false;
    }
    return stopwords().count(lower) == 0;
}

std::string findMention(const std::string& text) {
    for (const std::regex& pattern : mentionPatterns()) {
        std::smatch match;
        if (!std::regex_search(text, match, pattern)) {
            continue;
        }
        const std::string token = cleanToken(match[1].str());
        if (plausibleModId(token)) {
            return token;
        }
    }
    return {};
}

// Stack traces from Fabric/Quilt and JPMS can prefix a class with its module,
// e.g. "knot//net.minecraft.class_1439" or "java.base/java.lang.Thread".
std::string classNameWithoutModule(const std::string& className) {
    const std::size_t doubleSlash = className.find("//");
    if (doubleSlash != std::string::npos) {
        return className.substr(doubleSlash + 2);
    }
    const std::size_t slash = className.find('/');
    if (slash != std::string::npos) {
        const std::string rest = className.substr(slash + 1);
        if (rest.find('.') != std::string::npos) {
            return rest;
        }
    }
    return className;
}

// Minecraft, Java, loader, and bundled library packages never name the mod a
// user should update or remove.
bool isPlatformClass(const std::string& className) {
    const std::string normalized = classNameWithoutModule(className);
    static const char* const kPlatform[] = {
        "java.",      "javax.",          "jdk.",          "sun.",
        "com.sun.",   "net.minecraft.",  "com.mojang.",   "net.fabricmc.",
        "cpw.mods.",  "net.minecraftforge.", "net.neoforged.", "org.spongepowered.",
        "org.objectweb.asm.", "org.apache.", "io.netty.", "org.lwjgl.",
        "com.google.", "it.unimi.",      "org.slf4j.",    "org.jetbrains.",
        "kotlin.",    "org.openjdk.",    "com.typesafe.", "org.reactivestreams.",
    };
    for (const char* prefix : kPlatform) {
        if (strings::startsWith(normalized, prefix)) {
            return true;
        }
    }
    return false;
}

// The package that owns a class: com.acme.coolutil.init.Mod.onInit -> com.acme.coolutil.init
std::string packageOf(const std::string& className) {
    const std::size_t lastDot = className.rfind('.');
    if (lastDot == std::string::npos || lastDot == 0) {
        return {};
    }
    return className.substr(0, lastDot);
}

// Mods often ship code in their own top-level package (examplemod.widget.X), so
// that root is a usable candidate when no known mod matches the package.
std::string candidateIdFromPackage(const std::string& packagePath) {
    static const std::unordered_set<std::string> kVendorRoots = {
        "com",      "org",      "net",       "io",        "dev",
        "mc",       "api",      "lib",       "libs",      "util",
        "utils",    "internal", "impl",      "java",      "javax",
        "jdk",      "sun",      "minecraft", "forge",     "fabric",
        "neoforge", "quilt",    "mixin",     "mixins",    "kotlin",
        "scala",    "test",     "example",   "server",    "client",
    };
    const std::size_t firstDot = packagePath.find('.');
    if (firstDot == std::string::npos || firstDot == 0) {
        return {};
    }
    const std::string root = packagePath.substr(0, firstDot);
    if (root.size() < 4 || kVendorRoots.count(root) != 0) {
        return {};
    }
    // A module-prefixed frame such as "knot//net.minecraft..." must never become
    // a mod id; module names are not valid Maven/mod ids.
    if (root.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
        std::string::npos) {
        return {};
    }
    return root;
}

const StackFrame* firstModFrame(const StackTrace& trace) {
    for (const TraceSegment& segment : trace.segments) {
        for (const StackFrame& frame : segment.frames) {
            if (!frame.className.has_value() || frame.className->empty()) {
                continue;
            }
            if (isPlatformClass(*frame.className)) {
                continue;
            }
            return &frame;
        }
    }
    return nullptr;
}

// A class named inside a NoClassDefFoundError / ClassNotFoundException message.
std::string exceptionClassName(const std::string& text) {
    static const std::regex classPattern(
        R"((?:NoClassDefFoundError|ClassNotFoundException)[^\n]*?[:\s]"?((?:[\w$]+[./])+[\w$]+))",
        std::regex::icase);
    std::smatch match;
    if (!std::regex_search(text, match, classPattern)) {
        return {};
    }
    std::string className = match[1].str();
    std::replace(className.begin(), className.end(), '/', '.');
    return className;
}

std::string combinedText(const LogEntry& entry) {
    std::string text = entry.message;
    if (!entry.trace.has_value()) {
        return text;
    }
    const std::string header = entry.trace->rootHeader();
    if (!header.empty()) {
        text += '\n';
        text += header;
    }
    if (!entry.trace->rawText.empty()) {
        text += '\n';
        text += entry.trace->rawText;
    }
    return text;
}

void applyKnown(ModAttribution& attribution, const ModRef& known) {
    attribution.modId = known.id;
    attribution.modName = known.name;
    attribution.modVersion = known.version;
}

}  // namespace

ModRegistry ModAttributor::buildRegistry(const std::vector<LogEntry>& entries,
                                         const CrashReport* crashReport) {
    ModRegistry registry;
    if (crashReport != nullptr) {
        for (const ModInfo& mod : crashReport->mods) {
            ModRef ref;
            ref.id = mod.id;
            ref.name = mod.name;
            ref.version = mod.version;
            ref.source = "crash report";
            registry.add(std::move(ref));
        }
    }
    for (const LogEntry& entry : entries) {
        const std::string mention = findMention(combinedText(entry));
        if (mention.empty()) {
            continue;
        }
        ModRef ref;
        ref.id = mention;
        ref.name = mention;
        ref.source = "log message";
        registry.add(std::move(ref));
    }
    return registry;
}

ModAttribution ModAttributor::attribute(const LogEntry& entry) const {
    const std::string text = combinedText(entry);

    // 1) The message names a mod id.
    const std::string mention = findMention(text);
    if (!mention.empty()) {
        ModAttribution attribution;
        attribution.identified = true;
        attribution.modId = mention;
        attribution.basis = ModBasis::MessageMention;
        attribution.confidence = Confidence::Confirmed;
        if (const ModRef* known = registry_.byId(mention)) {
            applyKnown(attribution, *known);
        }
        return attribution;
    }

    // 2) The first non-platform stack frame points at the owning package.
    if (entry.trace.has_value()) {
        if (const StackFrame* frame = firstModFrame(*entry.trace)) {
            ModAttribution attribution;
            attribution.identified = true;
            attribution.frame = frame->signature();
            const std::string className = classNameWithoutModule(*frame->className);
            attribution.ownerPackage = packageOf(className);
            if (const ModRef* known = registry_.matchPackage(className)) {
                applyKnown(attribution, *known);
                attribution.basis = ModBasis::KnownModPackage;
                attribution.confidence = Confidence::Likely;
            } else {
                attribution.basis = ModBasis::FramePackage;
                attribution.confidence = Confidence::Unverified;
                attribution.modId = candidateIdFromPackage(attribution.ownerPackage);
            }
            return attribution;
        }
    }

    // 3) An exception message names the class that could not be loaded.
    const std::string className = exceptionClassName(text);
    if (!className.empty()) {
        ModAttribution attribution;
        attribution.frame = className;
        attribution.ownerPackage = packageOf(className);
        attribution.basis = ModBasis::ExceptionClass;
        if (const ModRef* known = registry_.matchPackage(className)) {
            applyKnown(attribution, *known);
            attribution.confidence = Confidence::Likely;
        } else {
            attribution.confidence = Confidence::Unverified;
            attribution.modId = candidateIdFromPackage(attribution.ownerPackage);
        }
        attribution.identified = !attribution.ownerPackage.empty() ||
                                 !attribution.modId.empty();
        return attribution;
    }

    return {};
}

ModAttribution ModAttributor::attribute(const ErrorGroup& group) const {
    LogEntry entry;
    entry.message = group.displayMessage;
    if (group.sampleTrace.has_value()) {
        entry.trace = *group.sampleTrace;
    }
    return attribute(entry);
}

}  // namespace minesleuth
