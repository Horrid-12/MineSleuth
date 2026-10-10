#pragma once

#include <string>
#include <vector>

#include "models/crash_report.h"
#include "models/diagnostic.h"
#include "models/error_group.h"
#include "models/log_entry.h"
#include "models/mod_registry.h"

namespace minesleuth {

// Names the mod behind an error: first from a mod id written in the message,
// otherwise from the first stack frame that is not Minecraft/Java/loader code,
// otherwise from a class named inside the exception message. The registry
// supplies names and versions when the input listed its mods.
class ModAttributor {
public:
    ModAttributor() = default;
    explicit ModAttributor(ModRegistry registry) : registry_(std::move(registry)) {}

    // Crash report mod list first, then every mod id mentioned in a message.
    static ModRegistry buildRegistry(const std::vector<LogEntry>& entries,
                                     const CrashReport* crashReport);

    ModAttribution attribute(const LogEntry& entry) const;
    ModAttribution attribute(const ErrorGroup& group) const;

    const ModRegistry& registry() const { return registry_; }

private:
    ModRegistry registry_;
};

}  // namespace minesleuth
