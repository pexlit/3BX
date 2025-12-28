#pragma once

#include "ast/ast.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_set>

namespace tbx {

class PatternRegistry;

// Handles the multi-pass deduction of variables in patterns
class PatternResolver {
public:
    explicit PatternResolver(PatternRegistry& registry);

    // Run the deduction loop until convergence
    // Returns true if all patterns were successfully resolved
    bool resolveAll();

private:
    PatternRegistry& registry_;
    std::unordered_set<PatternDef*> resolvedDefs_;

    // Attempt to deduce variables for a single pattern
    // Returns true if specific variables were newly deduced
    bool resolvePattern(PatternDef* def);

    // Recursively scan AST for variable usage
    void scanNode(ASTNode* node, std::unordered_set<std::string>& deducedVars, const std::vector<std::string>& rawSyntax);

    // Identify if a raw syntax word is effectively a parameter/variable
    bool isVariable(const std::string& word, const std::unordered_set<std::string>& deducedVars) const;
};

} // namespace tbx
