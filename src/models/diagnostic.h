#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "models/log_entry.h"
#include "utils/strings.h"

namespace minesleuth {

enum class Severity {
    Info,
    Warning,
    Error,
    Critical
};

enum class Confidence {
    Unverified,
    Likely,
    Confirmed
};

inline int severityRank(Severity severity) {
    switch (severity) {
        case Severity::Info: return 0;
        case Severity::Warning: return 1;
        case Severity::Error: return 2;
        case Severity::Critical: return 3;
    }
    return 0;
}

inline const char* toString(Severity severity) {
    switch (severity) {
        case Severity::Info: return "INFO";
        case Severity::Warning: return "WARNING";
        case Severity::Error: return "ERROR";
        case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

inline const char* toString(Confidence confidence) {
    switch (confidence) {
        case Confidence::Unverified: return "UNVERIFIED";
        case Confidence::Likely: return "LIKELY";
        case Confidence::Confirmed: return "CONFIRMED";
    }
    return "UNKNOWN";
}

inline Severity severityOf(LogLevel level) {
    switch (level) {
        case LogLevel::FATAL: return Severity::Critical;
        case LogLevel::ERROR: return Severity::Error;
        case LogLevel::WARN: return Severity::Warning;
        default: return Severity::Info;
    }
}

inline Severity higherSeverity(Severity a, Severity b) {
    return severityRank(a) >= severityRank(b) ? a : b;
}

constexpr std::size_t kNoEntry = static_cast<std::size_t>(-1);

enum class ModBasis {
    None,
    MessageMention,   // the error message names the mod
    KnownModPackage,  // a stack frame package matches a known mod id
    FramePackage,     // the stack frame package is the only lead
    ExceptionClass    // a class named inside the exception message
};

inline bool isPlatformModId(std::string_view id) {
    const std::string lower = strings::toLower(strings::trim(id));
    return lower == "minecraft" || lower == "fabricloader" || lower == "fabric-loader" ||
           lower == "fabric" || lower == "forge" || lower == "minecraftforge" ||
           lower == "neoforge" || lower == "quilt" || lower == "quilt_loader" ||
           lower == "quilt-loader" || lower == "java" || lower == "openjdk" ||
           lower == "oracle" || lower == "mixin" || lower == "sponge" ||
           lower == "spongepowered";
}

// Packages shipped by the game, its loaders, and the JVM are platform code, not
// user-installed mods. Class-loader and module prefixes are removed before this
// check, so the same rule covers Fabric, Quilt, Forge, NeoForge, Paper, JPMS,
// and custom launch wrappers.
inline bool isPlatformPackage(std::string_view packageOrClass) {
    static constexpr std::string_view kPlatformPrefixes[] = {
        "java.",                "javax.",             "jdk.",
        "sun.",                 "com.sun.",           "net.minecraft.",
        "com.mojang.",          "net.fabricmc.",      "org.quiltmc.",
        "cpw.mods.",            "net.minecraftforge.", "net.neoforged.",
        "org.spongepowered.",   "org.bukkit.",        "org.spigotmc.",
        "io.papermc.",          "com.destroystokyo.", "net.md_5.bungee.",
        "com.velocitypowered.", "org.objectweb.asm.", "org.apache.",
        "io.netty.",            "org.lwjgl.",         "com.google.",
        "it.unimi.",            "org.slf4j.",         "org.jetbrains.",
        "kotlin.",              "org.openjdk.",       "com.typesafe.",
        "org.reactivestreams.",
    };
    for (std::string_view prefix : kPlatformPrefixes) {
        if (strings::startsWith(packageOrClass, prefix)) {
            return true;
        }
    }
    return false;
}

// Which mod an error points at. modId is filled when the input names the mod
// (crash report mod list or a mod id written in the message); ownerPackage and
// frame are the fallback lead taken from the stack trace or exception text.
// identified means we can point at a mod id or a concrete package, not that the
// mod is proven guilty - confidence says how firm the claim is.
struct ModAttribution {
    bool identified = false;
    std::string modId;
    std::string modName;
    std::string modVersion;
    std::string ownerPackage;
    std::string frame;
    ModBasis basis = ModBasis::None;
    Confidence confidence = Confidence::Unverified;

    bool isPlatform() const {
        return isPlatformModId(modId) ||
               (!ownerPackage.empty() && isPlatformPackage(ownerPackage));
    }

    std::string label() const {
        if (!modId.empty()) {
            std::string out = modId;
            if (!modVersion.empty()) {
                out += ' ' + modVersion;
            }
            if (!modName.empty() && modName != modId) {
                out += " (" + modName + ")";
            }
            return out;
        }
        return ownerPackage.empty() ? std::string{} : "package " + ownerPackage;
    }

    const char* basisText() const {
        switch (basis) {
            case ModBasis::MessageMention: return "named in the error message";
            case ModBasis::KnownModPackage: return "stack frame matches a known mod";
            case ModBasis::FramePackage: return "stack frame package";
            case ModBasis::ExceptionClass: return "class named in the exception message";
            case ModBasis::None: return "";
        }
        return "";
    }

    // Concrete fix step. The rule id keeps the advice correct for rules where
    // the named mod is a dependency to install rather than a culprit to remove.
    std::string actionStep(const std::string& ruleId) const {
        if (!identified) {
            return {};
        }

        // Platform components (Minecraft, loaders, Java) can never be removed as user mods.
        if (isPlatform()) {
            if (ruleId == "class-not-found") {
                return "A mod is trying to call an incompatible Minecraft method or class ('" +
                       (frame.empty() ? label() : frame) + "'). Check for mods built for a different Minecraft version, then update or remove them.";
            }
            if (ruleId == "minecraft-version-mismatch") {
                return "Verify your game version in your launcher profile matches what your installed mods require.";
            }
            return "This issue involves the " + label() + " base platform. Check for incompatible mods or verify your game installation.";
        }

        if (modId.empty()) {
            return "Update or remove the mod that ships package " + ownerPackage +
                   (frame.empty() ? "" : " (first non-Minecraft frame " + frame + ")") +
                   ", then re-test.";
        }
        const std::string target = label();
        if (ruleId == "missing-mandatory-dependency") {
            return "Install the mod " + target +
                   " (the missing dependency), or remove the mod that requires it, then re-test.";
        }
        if (ruleId == "duplicate-mod-id") {
            return "Remove or update the mod declaring the id " + modId +
                   "; check your mods folder for a second copy of it, then re-test.";
        }
        if (ruleId == "minecraft-version-mismatch") {
            return "Install a build of " + target + " made for the Minecraft version you run, "
                   "or change the game version, then re-test.";
        }
        if (ruleId == "loader-mismatch") {
            return "Install a build of " + target +
                   " made for your mod loader, or remove it, then re-test.";
        }
        if (ruleId == "entrypoint-failure") {
            return "Reinstall or update " + target +
                   "; if the entrypoint still fails, remove it to confirm it is the cause.";
        }
        if (ruleId == "class-not-found" && basis == ModBasis::ExceptionClass) {
            return "The class above is missing from " + target +
                   ": reinstall or update that mod and check its dependencies.";
        }
        return "Update or remove " + target + ", then re-test to confirm this error stops.";
    }
};

struct Diagnostic {
    std::string ruleId;
    std::string title;
    std::string explanation;
    std::vector<std::string> suggestions;
    Severity severity = Severity::Error;
    Confidence confidence = Confidence::Likely;
    std::string evidence;
    std::string sourceLabel;
    std::size_t sourceLine = 0;
    std::size_t entryIndex = kNoEntry;
    std::size_t occurrences = 1;
    ModAttribution mod;
};

}  // namespace minesleuth
