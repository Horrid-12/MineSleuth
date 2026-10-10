#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "models/stack_trace.h"

namespace minesleuth {

struct ModInfo {
    std::string id;
    std::string name;
    std::string version;
};

struct CrashReport {
    bool valid = false;
    std::string parseError;
    std::string title;
    std::string description;
    std::size_t descriptionLine = 0;
    std::string timestamp;
    std::vector<std::pair<std::string, std::string>> systemDetails;
    std::vector<ModInfo> mods;
    std::vector<StackTrace> exceptions;
    std::vector<std::string> rawLines;

    std::string systemDetail(const std::string& key) const {
        for (const auto& entry : systemDetails) {
            if (entry.first == key) {
                return entry.second;
            }
        }
        return {};
    }

    const ModInfo* findMod(const std::string& modId) const {
        for (const ModInfo& mod : mods) {
            if (mod.id == modId) {
                return &mod;
            }
        }
        return nullptr;
    }
};

}  // namespace minesleuth
