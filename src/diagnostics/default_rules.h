#pragma once

#include <memory>
#include <vector>

#include "diagnostics/diagnostic_rule.h"

namespace minesleuth {

std::vector<std::unique_ptr<DiagnosticRule>> createDefaultRules();

}  // namespace minesleuth
