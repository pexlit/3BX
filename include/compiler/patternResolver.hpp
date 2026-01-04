#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <variant>
#include <optional>
#include "compiler/sectionAnalyzer.hpp"
#include "compiler/patternTree.hpp"

namespace tbx {

PatternType patternTypeFromPrefix(const std::string& prefix);
std::string patternTypeToString(PatternType type);

/**
 * Represents a resolved pattern definition
 */
struct ResolvedPattern {
    std::string pattern;           // The pattern string with $ for variable slots
    std::string originalText;     // The original text of the pattern
    std::vector<std::string> variables; // Names of the variables in the pattern
    CodeLine* sourceLine = nullptr; // The line that defines this pattern
    Section* body = nullptr;       // The body of the pattern (child section)
    PatternType type = PatternType::Effect;
    bool isPrivate = false;

    // Check if the pattern is a single word with no variables
    bool isSingleWord() const;

    // Calculate specificity (number of literal words)
    int specificity() const;

    // Print for debugging
    void print(int indent = 0) const;
};

/**
 * Represents a match of a code line against a pattern
 */
struct PatternMatch {
    ResolvedPattern* pattern = nullptr;
    
    struct ArgumentInfo {
        ResolvedValue value;
        int startCol;
        int length;
        bool isLiteral;

        ArgumentInfo() : value(""), startCol(0), length(0), isLiteral(false) {}
        ArgumentInfo(ResolvedValue v, int s = 0, int l = 0, bool lit = false)
            : value(v), startCol(s), length(l), isLiteral(lit) {}
    };

    std::map<std::string, ArgumentInfo> arguments;

    // Print for debugging
    void print(int indent = 0) const;
};

/**
 * SectionPatternResolver - Step 3 of the 3BX compiler pipeline
 *
 * Matches code lines against pattern definitions.
 */
class SectionPatternResolver {
public:
    SectionPatternResolver();

    /**
     * Resolve all pattern references in the section tree
     * @return true if all patterns were successfully resolved
     */
    bool resolve(Section* root);

    /**
     * Get any diagnostics (errors/warnings) found during resolution
     */
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    /**
     * Get all pattern definitions found in the codebase
     */
    const std::vector<std::unique_ptr<ResolvedPattern>>& patternDefinitions() const {
        return patternDefinitions_;
    }

    /**
     * Get all successful pattern matches
     */
    const std::vector<std::unique_ptr<PatternMatch>>& patternMatches() const {
        return patternMatches_;
    }

    /**
     * Print the results of pattern resolution for debugging
     */
    void printResults() const;

    /**
     * Tree-based matching (Step 3 optimized)
     */
    void buildPatternTrees();
    PatternMatch* matchWithTree(CodeLine* line);

private:
    /**
     * Phase 1: Collect all pattern definitions and code lines
     */
    void collectCodeLines(Section* section, std::vector<CodeLine*>& lines);
    void collectSections(Section* section, std::vector<Section*>& sections);
    std::unique_ptr<ResolvedPattern> extractPatternDefinition(CodeLine* line);

    /**
     * Phase 2: Variable identification and pattern string creation
     */
    std::vector<std::string> parsePatternWords(const std::string& text);
    std::vector<std::string> identifyVariables(const std::vector<std::string>& words);
    std::vector<std::string> identifyVariablesFromBody(const std::vector<std::string>& patternWords, Section* body);
    std::string createPatternString(const std::vector<std::string>& words, const std::vector<std::string>& variables);

    /**
     * Resolution algorithm phases
     */
    void resolveSingleWordPatterns();
    bool resolvePatternReferences();
    bool resolveSections();
    bool propagateVariablesFromCalls();

    /**
     * Matching logic
     */
    PatternMatch* tryMatchReference(CodeLine* line);
    bool tryMatchPattern(const ResolvedPattern* pattern, const std::string& referenceText, 
                        std::map<std::string, PatternMatch::ArgumentInfo>& arguments);
    bool tryMatchPatternSingle(const ResolvedPattern* pattern, const std::string& patternText, 
                              const std::string& referenceText, std::map<std::string, PatternMatch::ArgumentInfo>& arguments);

    /**
     * Helper to detect if a line is a special directive or intrinsic
     */
    bool isIntrinsicCall(const std::string& text) const;
    bool isPatternBodyDirective(const std::string& text) const;
    bool isSingleWordWithSection(const CodeLine* line) const;
    bool isInsidePatternsSection(CodeLine* line) const;

    /**
     * Tree-based matching helper structures and methods
     */
    struct ParsedLiteral {
        enum class Type { String, Number, Intrinsic, Group };
        Type type;
        std::string text;
        size_t startPos;
        size_t endPos;
        std::vector<std::string> intrinsicArgs;
    };

    std::vector<ParsedLiteral> detectLiterals(const std::string& input);
    size_t parseIntrinsicCall(const std::string& input, size_t startPos, std::string& name, std::vector<std::string>& args);
    std::unique_ptr<PatternMatch> treeMatchToPatternMatch(const TreePatternMatch& treeMatch, const ResolvedPattern* pattern);

    std::vector<std::unique_ptr<ResolvedPattern>> patternDefinitions_;
    std::vector<std::unique_ptr<PatternMatch>> patternMatches_;
    std::vector<Diagnostic> diagnostics_;

    // Working state during resolution
    std::vector<CodeLine*> allLines_;
    std::vector<Section*> allSections_;
    std::map<CodeLine*, ResolvedPattern*> lineToPattern_;
    std::map<CodeLine*, PatternMatch*> lineToMatch_;

    // Pattern Trees
    PatternTree effectTree_;
    PatternTree sectionTree_;
    PatternTree expressionTree_;
};

} // namespace tbx
