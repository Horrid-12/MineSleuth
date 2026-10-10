#include "diagnostics/rule_engine.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <unordered_set>

#include "analyzer/mod_attribution.h"
#include "diagnostics/default_rules.h"
#include "utils/strings.h"

namespace minesleuth {
namespace {

// True when a rule already names the mod or package in its own advice.
bool namesMod(const std::vector<std::string>& suggestions, const ModAttribution& mod) {
    const std::string needle =
        strings::toLower(mod.modId.empty() ? mod.ownerPackage : mod.modId);
    if (needle.empty()) {
        return false;
    }
    for (const std::string& suggestion : suggestions) {
        if (strings::toLower(suggestion).find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void attachModAttribution(std::vector<Diagnostic>& diagnostics,
                          const std::vector<LogEntry>& entries, const ModAttributor& attributor) {
    for (Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.mod.identified || diagnostic.entryIndex == kNoEntry ||
            diagnostic.entryIndex >= entries.size()) {
            continue;
        }
        ModAttribution attribution = attributor.attribute(entries[diagnostic.entryIndex]);
        if (!attribution.identified) {
            continue;
        }
        diagnostic.mod = std::move(attribution);
        if (!namesMod(diagnostic.suggestions, diagnostic.mod)) {
            diagnostic.suggestions.push_back(
                diagnostic.mod.actionStep(diagnostic.ruleId));
        }
    }
}

}  // namespace

RuleEngine::RuleEngine(std::vector<std::unique_ptr<DiagnosticRule>> rules)
    : rules_(std::move(rules)) {}

RuleEngine RuleEngine::withDefaultRules() {
    return RuleEngine(createDefaultRules());
}

std::vector<Diagnostic> RuleEngine::evaluate(const std::vector<LogEntry>& entries,
                                             const std::vector<ErrorGroup>& groups,
                                             const std::vector<ExceptionOccurrence>& exceptions,
                                             const CrashReport* crashReport,
                                             const ModRegistry* registry) const {
    RuleContext context{entries, groups, exceptions, crashReport, {}};
    std::vector<Diagnostic> diagnostics;
    std::vector<const DiagnosticRule*> fallbackRules;

    for (const std::unique_ptr<DiagnosticRule>& rule : rules_) {
        if (rule->isFallback()) {
            fallbackRules.push_back(rule.get());
            continue;
        }
        std::vector<Diagnostic> produced = rule->evaluate(context);
        if (rule->attributesCause()) {
            for (const Diagnostic& diagnostic : produced) {
                if (diagnostic.entryIndex != kNoEntry) {
                    context.matchedEntryIndices.insert(diagnostic.entryIndex);
                }
            }
        }
        for (Diagnostic& diagnostic : produced) {
            diagnostics.push_back(std::move(diagnostic));
        }
    }

    for (const DiagnosticRule* rule : fallbackRules) {
        std::vector<Diagnostic> produced = rule->evaluate(context);
        for (Diagnostic& diagnostic : produced) {
            diagnostics.push_back(std::move(diagnostic));
        }
    }

    std::stable_sort(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& a, const Diagnostic& b) {
                         if (severityRank(a.severity) != severityRank(b.severity)) {
                             return severityRank(a.severity) > severityRank(b.severity);
                         }
                         if (a.sourceLine != b.sourceLine) {
                             return a.sourceLine < b.sourceLine;
                         }
                         return a.ruleId < b.ruleId;
                     });

    std::unordered_set<std::string> seen;
    std::vector<Diagnostic> unique;
    unique.reserve(diagnostics.size());
    for (Diagnostic& diagnostic : diagnostics) {
        const std::string key = diagnostic.ruleId + '|' + std::to_string(diagnostic.sourceLine) +
                                '|' + diagnostic.title;
        if (!seen.insert(key).second) {
            continue;
        }
        unique.push_back(std::move(diagnostic));
    }

    const ModRegistry built =
        registry != nullptr ? ModRegistry{} : ModAttributor::buildRegistry(entries, crashReport);
    const ModAttributor attributor(registry != nullptr ? *registry : built);
    attachModAttribution(unique, entries, attributor);
    return unique;
}

}  // namespace minesleuth
