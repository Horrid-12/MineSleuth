#include "parser/input_detector.h"

#include <fstream>

#include "utils/strings.h"

namespace minesleuth {

InputFormat detectInputFormat(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return InputFormat::Unknown;
    }

    std::string line;
    std::size_t scanned = 0;
    while (scanned < 200 && std::getline(file, line)) {
        ++scanned;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string lowered = strings::toLower(strings::trim(line));
        if ((strings::startsWith(lowered, "---") && lowered.find("crash report") != std::string::npos) ||
            lowered.find("-- system details --") != std::string::npos) {
            return InputFormat::CrashReport;
        }
    }
    if (scanned == 0) {
        return InputFormat::Unknown;
    }
    return InputFormat::LogFile;
}

}  // namespace minesleuth
