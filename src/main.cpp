#include <iostream>
#include <filesystem>
#include <iomanip>
#include "parser/log_parser.h"

void printSummary(const std::filesystem::path& file_path, const minesleuth::LogStatistics& stats) {
    std::cout << "╔══════════════════════════════════╗\n";
    std::cout << "║          MINESLEUTH              ║\n";
    std::cout << "╠══════════════════════════════════╣\n";
    std::cout << "║ File: " << std::left << std::setw(27) << file_path.filename().string() << "║\n";
    std::cout << "║ Entries:              " << std::right << std::setw(10) << stats.total_entries << " ║\n";
    std::cout << "║ Errors:               " << std::right << std::setw(10) << stats.errors << " ║\n";
    std::cout << "║ Warnings:             " << std::right << std::setw(10) << stats.warnings << " ║\n";
    std::cout << "║ Fatal exceptions:     " << std::right << std::setw(10) << stats.fatals << " ║\n";
    std::cout << "╚══════════════════════════════════╝\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_minecraft_log>\n";
        return 1;
    }

    std::filesystem::path log_path(argv[1]);
    
    minesleuth::LogParser parser;
    if (parser.parseFile(log_path)) {
        printSummary(log_path, parser.getStatistics());
    } else {
        return 1;
    }

    return 0;
}
