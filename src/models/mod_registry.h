#pragma once

#include <cstddef>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "models/diagnostic.h"
#include "utils/strings.h"

namespace minesleuth {

// A mod this input let us identify. id/name/version come from a crash report
// mod list, or only the id when it was written inside a log message.
struct ModRef {
    std::string id;
    std::string name;
    std::string version;
    std::string source;
};

// Inventory of the mods known from one input. Lookups ignore case, and package
// matching ignores separators so "cool_util" is found in com.acme.coolutil.
class ModRegistry {
public:
    void add(ModRef mod) {
        if (strings::trim(mod.id).empty()) {
            return;
        }
        if (byId(mod.id) != nullptr) {
            return;  // keep the first entry, which is the richer crash report one
        }
        mods_.push_back(std::move(mod));
    }

    std::size_t size() const { return mods_.size(); }
    bool empty() const { return mods_.empty(); }
    const std::vector<ModRef>& mods() const { return mods_; }

    const ModRef* byId(std::string_view id) const {
        const std::string wanted = strings::toLower(strings::trim(id));
        if (wanted.empty()) {
            return nullptr;
        }
        for (const ModRef& mod : mods_) {
            if (strings::toLower(mod.id) == wanted) {
                return &mod;
            }
        }
        return nullptr;
    }

    static std::string normalize(std::string_view text) {
        std::string out;
        out.reserve(text.size());
        for (char c : text) {
            if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }
        return out;
    }

    // Longest mod id or display name that appears inside a class/package path.
    const ModRef* matchPackage(std::string_view packageOrClass) const {
        const std::string target = normalize(packageOrClass);
        if (target.empty()) {
            return nullptr;
        }
        const ModRef* best = nullptr;
        std::size_t bestKeySize = 0;
        for (const ModRef& mod : mods_) {
            if (isPlatformModId(mod.id)) {
                continue;  // Base game / loaders / java are never mod culprits to match
            }
            for (const std::string& rawKey : {mod.id, mod.name}) {
                const std::string key = normalize(rawKey);
                // Short ids such as "jei" would match too many packages.
                if (key.size() < 4 || key.size() <= bestKeySize) {
                    continue;
                }
                if (target.find(key) != std::string::npos) {
                    best = &mod;
                    bestKeySize = key.size();
                }
            }
        }
        return best;
    }

private:
    std::vector<ModRef> mods_;
};

}  // namespace minesleuth
