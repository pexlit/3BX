#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>

namespace tbx {

// Forward declarations
struct ResolvedPattern;
struct PatternTreeNode;

/**
 * Represents a matched value in a pattern
 * Can be a literal (number, string) or a nested expression match
 */
using MatchedValue = std::variant<
    int64_t,                                    // Integer literal
    double,                                     // Float literal
    std::string,                                // String literal or identifier
    std::shared_ptr<struct ExpressionMatch>     // Nested expression
>;

/**
 * Result of matching an expression (for expression substitution)
 */
struct ExpressionMatch {
    ResolvedPattern* pattern;                           // The matched expression pattern
    std::vector<MatchedValue> arguments;                // Arguments in order
    std::string matchedText;                            // The original text that was matched
};

/**
 * Result of a successful pattern match
 */
struct TreePatternMatch {
    ResolvedPattern* pattern;                           // The matched pattern
    std::vector<MatchedValue> arguments;                // Arguments in variable order
    size_t consumedLength;                              // How many characters were consumed
};

/**
 * Pattern element types for building patterns
 */
enum class PatternElementType {
    Literal,            // A literal string like "print " or " + "
    Variable,           // A $ variable slot (eager expression)
    ExpressionCapture,  // A {expression:name} lazy capture (greedy, caller's scope)
    WordCapture         // A {word:name} single identifier capture (non-greedy)
};

/**
 * A single element in a parsed pattern
 */
struct PatternElement {
    PatternElementType type;
    std::string text;           // For Literal: the text
    std::string captureName;    // For captures: the variable name (e.g., "member" in {word:member})
};

/**
 * PatternTreeNode - A node in the pattern matching trie
 *
 * Uses unordered_map for O(1) child lookup by literal string.
 * Separate pointers for different capture types.
 */
struct PatternTreeNode {
    // Child nodes keyed by literal strings (merged sequences)
    std::unordered_map<std::string, std::shared_ptr<PatternTreeNode>> children;

    // Child node for expression variable slots ($) - eager evaluation
    std::shared_ptr<PatternTreeNode> expressionChild;

    // Child node for {expression:name} - lazy capture (greedy, caller's scope)
    std::shared_ptr<PatternTreeNode> expressionCaptureChild;

    // Child node for {word:name} - single identifier capture (non-greedy)
    std::shared_ptr<PatternTreeNode> wordCaptureChild;

    // Patterns that end at this node
    std::vector<ResolvedPattern*> patternsEndedHere;

    PatternTreeNode() = default;
};

/**
 * PatternTree - Trie-like structure for efficient pattern matching
 *
 * Supports:
 * - Merged literal sequences for compact storage
 * - Expression substitution via recursive matching
 * - Alternatives [a|b] with branch-and-merge (handled during pattern insertion)
 * - Lazy captures {word} for deferred evaluation
 */
class PatternTree {
public:
    PatternTree();

    /**
     * Add a pattern to the tree
     * @param pattern The resolved pattern to add
     */
    void addPattern(ResolvedPattern* pattern);

    /**
     * Match input text against all patterns in the tree
     * @param input The input text to match
     * @param startPos Starting position in the input
     * @return Best match (most specific), or nullopt if no match
     */
    std::optional<TreePatternMatch> match(const std::string& input, size_t startPos = 0);

    /**
     * Match only expression patterns (for expression substitution)
     * @param input The input text to match
     * @param startPos Starting position in the input
     * @return Best expression match, or nullopt if no match
     */
    std::optional<TreePatternMatch> matchExpression(const std::string& input, size_t startPos = 0);

    /**
     * Clear all patterns from the tree
     */
    void clear();

    /**
     * Get the root node (for debugging)
     */
    const PatternTreeNode& root() const { return root_; }

private:
    /**
     * Parse a pattern into elements (merged literals + variable slots)
     */
    std::vector<PatternElement> parsePatternElements(const ResolvedPattern* pattern);

    /**
     * Parse a pattern string directly into elements
     */
    std::vector<PatternElement> parsePatternElementsFromString(const std::string& text);

    /**
     * Parse alternatives in a pattern text
     * Returns expanded patterns for [a|b] syntax
     */
    std::vector<std::string> expandAlternatives(const std::string& patternText);

    /**
     * Add a single expanded pattern path to the tree
     */
    void addPatternPath(const std::vector<PatternElement>& elements,
                          ResolvedPattern* pattern);

    /**
     * Internal matching with backtracking
     * @param node Current node in the tree
     * @param input Input text
     * @param pos Current position in input
     * @param arguments Collected arguments so far
     * @param matches Output: all successful matches found
     */
    void matchRecursive(PatternTreeNode* node,
                         const std::string& input,
                         size_t pos,
                         std::vector<MatchedValue>& arguments,
                         std::vector<TreePatternMatch>& matches);

    /**
     * Try to match an expression at the current position
     * Used when encountering an expression_child node
     */
    std::optional<ExpressionMatch> tryMatchExpressionAt(const std::string& input,
                                                            size_t pos);

    /**
     * Find possible expression end positions
     * Returns positions in reverse order (longest first) for greedy matching
     */
    std::vector<size_t> findExpressionBoundaries(const std::string& input, size_t start);

    /**
     * Check if a character is an expression boundary
     */
    bool isExpressionBoundary(char c) const;

    /**
     * Try to parse a literal value (number or string)
     */
    std::optional<MatchedValue> tryParseLiteral(const std::string& text);

    PatternTreeNode root_;

    // Separate storage for expression patterns (for recursive matching)
    std::vector<ResolvedPattern*> expressionPatterns_;
};

} // namespace tbx
