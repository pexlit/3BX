#pragma once

#include "ast/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace tbx {

struct Pattern;

// Manages precedence relationships between patterns and groups
class PrecedenceRegistry {
public:
    // Add a named group (e.g., "Multiplicative")
    void registerGroup(const std::string& groupName);

    // Register a priority rule: subject "is tighter than" object
    void addRelation(const std::string& subject, const std::string& object);

    // Register a pattern belonging to a group
    void assignPatternToGroup(const std::string& patternSignature, const std::string& groupName);

    // Register a pattern relative to a syntax string (raw or canonical signature)
    void addPatternRelation(const std::string& patternSignature, Relation rel, const std::string& targetSyntax);

    // Calculate integer priorities for all registered items
    // Returns true on success, false on cycle detection
    bool calculatePriorities();

    // Get the calculated priority for a pattern (higher number = tighter binding)
    // Returns std::nullopt if pattern has no assigned priority
    std::optional<int> getPriority(const std::string& patternSignature) const;

    // Helper: Convert pattern syntax to canonical signature (e.g., "a + b" -> "$ + $")
    static std::string canonicalize(const std::vector<PatternElement>& elements);
    static std::string canonicalize(const std::vector<std::string>& rawSyntax);

private:
    // Graph nodes: can be Group Names or Pattern Signatures
    std::unordered_set<std::string> nodes_;
    
    // Adjacency list: key -> [values that are TIGHTER than key]
    // Usage: if A > B (A tighter than B), then B -> A edge exists? 
    // Standard ToPo Sort: Node with 0 incoming edges has lowest priority.
    // If A binds tighter than B, A should be evaluated first (bottom of tree) or...
    // Precedence: * (20) > + (10). 
    // If we want generated int priorities where Higher is Tighter:
    // A > B means A depends on B implies... 
    // Let's use: Edge A -> B means A is TIGHTER than B (A > B).
    // Sorting: Visit logic...
    // Actually simpler: Edge From -> To means "From must have LOWER priority than To".
    // So if A TighterThan B, then B -> A. (B processes first/lower... wait).
    // Standard Precedence: * binds tighter. 1 + 2 * 3. 
    // Parse: 1 + (2 * 3). * is deeper in AST, evaluated first.
    // Pratt Parsing: * has Higher Binding Power.
    // So if A TighterThan B, Priority(A) > Priority(B).
    // Edge B -> A (B is looser than A).
    // Topological sort yields B, then A. 
    // Priority(B) = 0, Priority(A) = 1. Correct.
    std::unordered_map<std::string, std::vector<std::string>> adj_;
    
    // Mapping from pattern signature to its assigned group (if any)
    std::unordered_map<std::string, std::string> patternToGroup_;

    // Result: signature/group -> integer priority
    std::unordered_map<std::string, int> priorities_;
};

} // namespace tbx
