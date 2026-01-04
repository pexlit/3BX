#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include "compiler/diagnostic.hpp"

namespace tbx {

/**
 * Pattern type enumeration
 * Corresponds to the prefixes: "effect ", "expression ", "section "
 * Note: "condition" is treated as "expression" (booleans are just expressions)
 */
enum class PatternType {
    Effect,      // effect print msg:
    Expression,  // expression left + right: (includes conditions)
    Section      // section loop:
};

// Forward declaration
struct Section;

/**
 * Represents a resolved variable value
 * Can be a literal (number, string) or a reference to another pattern
 */
using ResolvedValue = std::variant<
    int64_t,                    // Integer literal
    double,                     // Float literal
    std::string,                // String literal or identifier
    std::shared_ptr<Section>    // Nested section/expression
>;

/**
 * CodeLine - Represents a single line of code within a section
 *
 * Each code line can be:
 * - A pattern definition (starts with "section ", "effect ", or "expression ")
 * - A pattern reference (any other code)
 *
 * If the line ends with ":", it has a child section.
 */
struct CodeLine {
    std::string text;                       // The raw line text (trimmed)
    bool isPatternDefinition = false;       // Starts with "section ", "effect ", or "expression "
    bool isPrivate = false;                 // Only accessible in the defining file
    PatternType type = PatternType::Effect; // The type of pattern (if isPatternDefinition)
    bool isResolved = false;                // Has this line been resolved?
    std::unique_ptr<Section> childSection;  // If line ends with ":"
    int lineNumber = 0;                     // Original line number in source
    std::string filePath;                   // Original source file path
    int startColumn = 0;                    // 0-based start column of actual code
    int endColumn = 0;                      // 0-based end column of actual code

    // Default constructor
    CodeLine() = default;

    // Constructor with text
    explicit CodeLine(const std::string& lineText, int lineNum = 0, std::string file = "", int startCol = 0, int endCol = 0);

    // Move constructor and assignment
    CodeLine(CodeLine&& other) noexcept = default;
    CodeLine& operator=(CodeLine&& other) noexcept = default;

    // No copy (due to unique_ptr)
    CodeLine(const CodeLine&) = delete;
    CodeLine& operator=(const CodeLine&) = delete;

    /**
     * Check if this line has a child section (ends with ":")
     */
    bool hasChildSection() const { return childSection != nullptr; }

    /**
     * Get the pattern text (without the trailing ":" if present)
     */
    std::string getPatternText() const;
};

/**
 * Section - A block of code at a particular indentation level
 *
 * Sections contain code lines, and each code line can have a child section.
 * This creates a tree structure based on indentation.
 */
struct Section {
    std::vector<CodeLine> lines;
    bool isResolved = false;
    std::map<std::string, ResolvedValue> resolvedVariables;
    Section* parent = nullptr;
    int indentLevel = 0;

    // Default constructor
    Section() = default;

    // Move constructor and assignment
    Section(Section&& other) noexcept = default;
    Section& operator=(Section&& other) noexcept = default;

    // No copy (due to unique_ptr in CodeLine)
    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;

    /**
     * Add a code line to this section
     */
    void addLine(CodeLine line);

    /**
     * Check if all lines in this section are resolved
     */
    bool allLinesResolved() const;

    /**
     * Print the section tree for debugging
     */
    void print(int depth = 0) const;
};

/**
 * SectionAnalyzer - Step 2 of the 3BX compiler pipeline
 *
 * Analyzes merged source code and creates a section tree based on indentation.
 *
 * Key principles:
 * - NO hardcoded keywords
 * - Indentation determines structure
 * - Lines ending with ":" have child sections
 * - Lines starting with "section ", "effect ", or "expression " are pattern definitions
 */
class SectionAnalyzer {
public:
    /**
     * Analyze source code and create section tree
     * @param source The merged source code
     * @param sourceMap Optional map mapping merged line numbers to original file locations
     *                   (mergedLine -> {filePath, originalLine})
     * @return Root section containing the entire program
     */
    struct SourceLocation {
        std::string filePath;
        int lineNumber;
    };
    
    std::unique_ptr<Section> analyze(const std::string& source, 
                                    const std::map<int, SourceLocation>& sourceMap = {});

    /**
     * Get any errors that occurred during analysis
     */
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

private:
    /**
     * Split source into lines and calculate indent levels
     */
    struct SourceLine {
        std::string text;       // Original text with whitespace trimmed
        int indentLevel;        // Number of leading spaces/tabs (normalized)
        int lineNumber;         // 1-based line number (in merged file)
        std::string filePath;   // Original file path (if available)
        int originalLine;       // Original line number (if available)
        int startColumn;        // Start column
        int endColumn;          // End column
        bool isEmpty;           // Is this line empty or comment-only?
    };

    std::vector<SourceLine> splitLines(const std::string& source, 
                                       const std::map<int, SourceLocation>& sourceMap);

    /**
     * Calculate indent level from leading whitespace
     * Tabs are treated as equivalent to 4 spaces
     */
    int calculateIndent(const std::string& line, int& startCol);

    /**
     * Trim leading and trailing whitespace
     */
    std::string trim(const std::string& str);

    /**
     * Check if a line is a comment (starts with #)
     */
    bool isComment(const std::string& line);

    /**
     * Check if a line is a pattern definition
     * Returns true if line starts with "section ", "effect ", or "expression "
     */
    bool isPatternDefinition(const std::string& line);

    /**
     * Check if a line ends with a colon (indicating child section follows)
     */
    bool endsWithColon(const std::string& line);

    /**
     * Build section tree recursively
     */
    void buildSection(Section& section,
                       const std::vector<SourceLine>& lines,
                       size_t& index,
                       int parentIndent);

    std::vector<Diagnostic> diagnostics_;
};

} // namespace tbx
