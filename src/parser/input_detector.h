#pragma once

#include <filesystem>

#include "models/analysis_result.h"

namespace minesleuth {

InputFormat detectInputFormat(const std::filesystem::path& file_path);

}  // namespace minesleuth
