#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "models/crash_report.h"

namespace minesleuth {

class CrashReportParser {
public:
    CrashReport parseFile(const std::filesystem::path& file_path) const;
    CrashReport parseLines(const std::vector<std::string>& lines) const;
};

}  // namespace minesleuth
