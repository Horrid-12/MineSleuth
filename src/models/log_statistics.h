#pragma once

#include <cstddef>

namespace minesleuth {

struct LogStatistics {
    std::size_t total_entries = 0;
    std::size_t errors = 0;
    std::size_t warnings = 0;
    std::size_t infos = 0;
    std::size_t fatals = 0;
    std::size_t stack_traces = 0;
};

}  // namespace minesleuth
