#include "parser/crash_report_parser.h"

#include <fstream>
#include <regex>

#include "parser/stack_trace_parser.h"
#include "utils/strings.h"

namespace minesleuth {
namespace {

bool isIndented(const std::string& line) {
    return !line.empty() && (line[0] == ' ' || line[0] == '\t');
}

std::string splitModEntry(const std::string& rest, std::string& version) {
    const std::string trimmed = strings::trim(rest);
    if (trimmed.empty()) {
        version.clear();
        return {};
    }
    if (std::isdigit(static_cast<unsigned char>(trimmed.front())) != 0) {
        version = trimmed;
        return {};
    }
    const std::size_t lastSpace = trimmed.find_last_of(" \t");
    if (lastSpace == std::string::npos) {
        version.clear();
        return trimmed;
    }
    const std::string tail = strings::trim(trimmed.substr(lastSpace + 1));
    if (!tail.empty() && std::isdigit(static_cast<unsigned char>(tail.front())) != 0) {
        version = tail;
        return strings::trim(trimmed.substr(0, lastSpace));
    }
    version.clear();
    return trimmed;
}

}  // namespace

CrashReport CrashReportParser::parseFile(const std::filesystem::path& file_path) const {
    CrashReport report;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        report.parseError = "Failed to open file: " + file_path.string();
        return report;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    if (lines.empty()) {
        report.parseError = "File is empty: " + file_path.string();
        return report;
    }
    return parseLines(lines);
}

CrashReport CrashReportParser::parseLines(const std::vector<std::string>& lines) const {
    CrashReport report;
    report.rawLines = lines;

    static const std::regex sectionRegex(R"(^\s*--\s*([A-Za-z].*?)\s*--\s*$)");
    static const std::regex modsHeaderRegex(R"(^\s*(?:Fabric|Forge|NeoForge|Quilt)?\s*Mods\s*:\s*$)",
                                            std::regex::icase);
    static const std::regex descriptionRegex(R"(^\s*Description\s*:\s*(.*)$)", std::regex::icase);
    static const std::regex timeRegex(R"(^\s*Time\s*:\s*(.*)$)", std::regex::icase);
    static const std::regex modEntryRegex(R"(^\s*([A-Za-z0-9_.\-]+)\s*:\s*(.+)$)");

    bool inMods = false;
    bool inSystemDetails = false;
    bool inDescription = false;
    StackTraceBuilder builder;

    auto collect = [&]() {
        if (builder.active()) {
            report.exceptions.push_back(builder.finish());
        }
        builder.reset();
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string line = lines[i];
        const std::size_t lineNumber = i + 1;
        if (line.empty()) {
            inDescription = false;
            continue;
        }

        const std::string trimmed = strings::trim(line);
        const std::string lowered = strings::toLower(trimmed);
        if (report.title.empty() && strings::startsWith(trimmed, "---") &&
            lowered.find("crash report") != std::string::npos) {
            report.title = trimmed;
            continue;
        }

        std::smatch match;
        if (std::regex_match(line, match, sectionRegex)) {
            collect();
            inDescription = false;
            inMods = false;
            const std::string name = strings::toLower(match[1].str());
            inSystemDetails = name.find("system details") != std::string::npos;
            if (name.find("mod") != std::string::npos) {
                inMods = true;
            }
            continue;
        }

        if (std::regex_match(line, match, descriptionRegex)) {
            collect();
            inDescription = true;
            inMods = false;
            report.description = strings::trim(match[1].str());
            report.descriptionLine = lineNumber;
            continue;
        }

        if (inDescription && isIndented(line) &&
            !StackTraceParser::looksLikeExceptionHeader(line) &&
            !StackTraceParser::looksLikeFrame(line)) {
            if (!report.description.empty()) {
                report.description.push_back(' ');
            }
            report.description += strings::trim(line);
            continue;
        }
        inDescription = false;

        if (std::regex_match(line, match, timeRegex)) {
            report.timestamp = strings::trim(match[1].str());
            continue;
        }

        if (std::regex_match(line, match, modsHeaderRegex)) {
            collect();
            inMods = true;
            inSystemDetails = true;
            continue;
        }

        if (inMods) {
            if (std::regex_match(line, match, modEntryRegex)) {
                ModInfo mod;
                mod.id = match[1];
                std::string version;
                const std::string name = splitModEntry(match[2], version);
                mod.name = name.empty() ? mod.id : name;
                mod.version = version;
                report.mods.push_back(std::move(mod));
                continue;
            }
            if (isIndented(line)) {
                continue;
            }
            inMods = false;
        }

        if (inSystemDetails && isIndented(line)) {
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos && colon > 0) {
                const std::string key = strings::trim(line.substr(0, colon));
                if (!key.empty()) {
                    report.systemDetails.emplace_back(key, strings::trim(line.substr(colon + 1)));
                    continue;
                }
            }
        }

        TraceSegment headerProbe;
        if (StackTraceParser::parseExceptionHeader(line, headerProbe) &&
            headerProbe.kind == TraceSegmentKind::Root) {
            collect();
            builder.seedRoot(line, lineNumber, false);
            continue;
        }
        if (builder.accept(line, lineNumber)) {
            continue;
        }
        collect();
    }
    collect();

    report.valid = !report.title.empty() || !report.description.empty() ||
                   !report.systemDetails.empty() || !report.exceptions.empty();
    if (!report.valid) {
        report.parseError = "Not a recognizable Minecraft crash report";
    }
    return report;
}

}  // namespace minesleuth
