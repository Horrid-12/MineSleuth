#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef MINESLEUTH_FIXTURE_DIR
#define MINESLEUTH_FIXTURE_DIR "fixtures"
#endif

namespace minesleuth::test {

inline std::filesystem::path fixturePath(const std::string& name) {
    return std::filesystem::path(MINESLEUTH_FIXTURE_DIR) / name;
}

inline std::vector<std::string> readLines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

}  // namespace minesleuth::test
