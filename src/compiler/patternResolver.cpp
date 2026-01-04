#include "compiler/patternResolver.hpp"

#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace tbx {

// No reserved words list - variables are deduced from @intrinsic usage

// ============================================================================
// Helper functions
// ============================================================================

PatternType patternTypeFromPrefix(const std::string& prefix) {
    if (prefix == "effect") return PatternType::Effect;
    if (prefix == "expression") return PatternType::Expression;
    if (prefix == "section") return PatternType::Section;
    if (prefix == "condition") return PatternType::Expression;  // Conditions are expressions
    // Default to Effect if unknown
    return PatternType::Effect;
}

std::string patternTypeToString(PatternType type) {
    switch (type) {
        case PatternType::Effect: return "effect";
        case PatternType::Expression: return "expression";
        case PatternType::Section: return "section";
    }
    return "unknown";
}

// Expand [option1|option2] alternatives into multiple pattern strings
std::vector<std::string> expandAlternatives(const std::string& patternText) {
    std::vector<std::string> results;
    results.push_back("");

    size_t i = 0;
    while (i < patternText.size()) {
        if (patternText[i] == '[') {
            // Find matching ]
            size_t depth = 1;
            size_t start = i + 1;
            size_t end = start;
            while (end < patternText.size() && depth > 0) {
                if (patternText[end] == '[') depth++;
                else if (patternText[end] == ']') depth--;
                end++;
            }
            end--; // Point to ]

            // Extract content between [ and ]
            std::string content = patternText.substr(start, end - start);

            // Split by | to get alternatives
            std::vector<std::string> alternatives;
            std::string current;
            size_t altDepth = 0;
            for (size_t j = 0; j < content.size(); j++) {
                char c = content[j];
                if (c == '[') altDepth++;
                else if (c == ']') altDepth--;
                else if (c == '|' && altDepth == 0) {
                    alternatives.push_back(current);
                    current.clear();
                    continue;
                }
                current += c;
            }
            alternatives.push_back(current);

            // Expand: for each existing result, create variants with each alternative
            std::vector<std::string> newResults;
            for (const auto& result : results) {
                for (const auto& alt : alternatives) {
                    // Recursively expand alternatives within this alternative
                    std::vector<std::string> expandedAlt = expandAlternatives(alt);
                    for (const auto& exp : expandedAlt) {
                        newResults.push_back(result + exp);
                    }
                }
            }
            results = std::move(newResults);

            i = end + 1;
        } else {
            // Regular character - append to all results
            for (auto& result : results) {
                result += patternText[i];
            }
            i++;
        }
    }

    return results;
}

// ============================================================================
// ResolvedPattern Implementation
// ============================================================================

bool ResolvedPattern::isSingleWord() const {
    // Count non-$ words in the pattern
    std::istringstream iss(pattern);
    std::string word;
    int wordCount = 0;
    while (iss >> word) {
        wordCount++;
    }
    return wordCount == 1 && pattern.find('$') == std::string::npos;
}

int ResolvedPattern::specificity() const {
    // Count literal words (non-$ elements)
    std::istringstream iss(pattern);
    std::string word;
    int literalCount = 0;
    while (iss >> word) {
        if (word != "$") {
            literalCount++;
        }
    }
    return literalCount;
}

void ResolvedPattern::print(int indent) const {
    std::string pad(indent, ' ');
    std::cout << pad << "- " << patternTypeToString(type) << " \"" << pattern << "\"\n";
    std::cout << pad << "    variables: [";
    for (size_t i = 0; i < variables.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << variables[i];
    }
    std::cout << "]\n";
    if (body && !body->lines.empty()) {
        std::cout << pad << "    body:\n";
        for (const auto& line : body->lines) {
            std::cout << pad << "      " << line.text << "\n";
        }
    }
}

// ============================================================================
// PatternMatch Implementation
// ============================================================================

void PatternMatch::print(int indent) const {
    std::string pad(indent, ' ');
    std::cout << pad << "matches: " << patternTypeToString(pattern->type)
              << " \"" << pattern->pattern << "\"\n";
    std::cout << pad << "arguments: {";
    bool first = true;
    for (const auto& [name, info] : arguments) {
        if (!first) std::cout << ", ";
        first = false;
        std::cout << name << ": ";

        const auto& value = info.value;
        // Print the value based on its type
        if (std::holds_alternative<int64_t>(value)) {
            std::cout << std::get<int64_t>(value);
        } else if (std::holds_alternative<double>(value)) {
            std::cout << std::get<double>(value);
        } else if (std::holds_alternative<std::string>(value)) {
            std::cout << "\"" << std::get<std::string>(value) << "\"";
        } else if (std::holds_alternative<std::shared_ptr<Section>>(value)) {
            std::cout << "[section]";
        }
    }
    std::cout << "}\n";
}

// ============================================================================
// SectionPatternResolver Implementation
// ============================================================================

SectionPatternResolver::SectionPatternResolver() = default;

bool SectionPatternResolver::resolve(Section* root) {
    if (!root) {
        diagnostics_.emplace_back("Cannot resolve null section");
        return false;
    }

    // Clear previous state
    patternDefinitions_.clear();
    patternMatches_.clear();
    allLines_.clear();
    allSections_.clear();
    lineToPattern_.clear();
    lineToMatch_.clear();
    diagnostics_.clear();

    // Collect all code lines and sections
    collectCodeLines(root, allLines_);
    collectSections(root, allSections_);

    // Extract all pattern definitions
    for (CodeLine* line : allLines_) {
        if (line->isPatternDefinition) {
            auto pattern = extractPatternDefinition(line);
            if (pattern) {
                lineToPattern_[line] = pattern.get();
                patternDefinitions_.push_back(std::move(pattern));
            }
        }
    }

    // Run the resolution algorithm iteratively
    int maxIterations = 100;  // Prevent infinite loops
    int iteration = 0;

    // Phase 1: Resolve single-word pattern definitions
    resolveSingleWordPatterns();

    // Iterate Phases 2, 3, and 4 until convergence
    while (iteration < maxIterations) {
        iteration++;
        bool progress = false;

        // Phase 2: Match pattern references to definitions
        if (resolvePatternReferences()) {
            progress = true;
        }

        // Phase 3: Resolve sections when all lines are resolved
        if (resolveSections()) {
            progress = true;
        }

        // Phase 4: Propagate variables from resolved pattern calls
        // When a body line matches a pattern, the arguments become variables
        if (propagateVariablesFromCalls()) {
            progress = true;
        }

        // Check if all patterns are resolved
        bool allResolved = true;
        for (CodeLine* line : allLines_) {
            if (!line->isResolved) {
                allResolved = false;
                break;
            }
        }

        if (allResolved || !progress) {
            break;
        }
    }

    // Check for unresolved patterns
    for (CodeLine* line : allLines_) {
        if (!line->isResolved) {
            std::string filePath = line->filePath;
            int lineNum = line->lineNumber;
            
            diagnostics_.emplace_back("Unresolved pattern: " + line->text, filePath, lineNum, line->startColumn, lineNum, line->endColumn);
        }
    }

    return diagnostics_.empty();
}

void SectionPatternResolver::collectCodeLines(Section* section, std::vector<CodeLine*>& lines) {
    if (!section) return;

    for (auto& line : section->lines) {
        lines.push_back(&line);
        if (line.childSection) {
            collectCodeLines(line.childSection.get(), lines);
        }
    }
}

void SectionPatternResolver::collectSections(Section* section, std::vector<Section*>& sections) {
    if (!section) return;

    sections.push_back(section);
    for (auto& line : section->lines) {
        if (line.childSection) {
            collectSections(line.childSection.get(), sections);
        }
    }
}

std::unique_ptr<ResolvedPattern> SectionPatternResolver::extractPatternDefinition(CodeLine* line) {
    if (!line || !line->isPatternDefinition) {
        return nullptr;
    }

    auto pattern = std::make_unique<ResolvedPattern>();
    pattern->sourceLine = line;
    pattern->body = line->childSection.get();
    pattern->isPrivate = line->isPrivate;
    pattern->type = line->type;

    // Get the pattern text (without type prefixes or "private ")
    std::string text = line->getPatternText();
    pattern->originalText = text;

    // Parse the pattern into words
    std::vector<std::string> words = parsePatternWords(text);

    // Identify which words are variables by analyzing @intrinsic usage in body
    pattern->variables = identifyVariablesFromBody(words, pattern->body);

    // Create the pattern string with $ for variable slots
    pattern->pattern = createPatternString(words, pattern->variables);

    return pattern;
}

std::vector<std::string> SectionPatternResolver::parsePatternWords(const std::string& text) {
    std::vector<std::string> words;
    std::string current;
    bool inQuotes = false;
    char quoteChar = '\0';

    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];

        if (inQuotes) {
            current += c;
            if (c == quoteChar) {
                inQuotes = false;
                words.push_back(current);
                current.clear();
            }
        } else if (c == '"' || c == '\'') {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
            inQuotes = true;
            quoteChar = c;
            current += c;
        } else if (std::isspace(c)) {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        words.push_back(current);
    }

    return words;
}

// Helper to extract intrinsic arguments from a line
static std::vector<std::string> extractIntrinsicArgs(const std::string& line) {
    std::vector<std::string> args;

    size_t pos = line.find("@intrinsic(");
    if (pos == std::string::npos) return args;

    // Find the opening paren
    pos += 10;  // length of "@intrinsic"
    if (pos >= line.size() || line[pos] != '(') return args;
    pos++;  // skip '('

    // Find matching closing paren
    int depth = 1;
    size_t start = pos;
    size_t end = pos;
    while (end < line.size() && depth > 0) {
        if (line[end] == '(') depth++;
        else if (line[end] == ')') depth--;
        end++;
    }
    if (depth != 0) return args;
    end--;  // point to ')'

    // Parse comma-separated arguments
    std::string content = line.substr(start, end - start);
    std::string current;
    bool inQuotes = false;
    char quoteChar = '\0';
    int parenDepth = 0;

    for (size_t i = 0; i < content.size(); i++) {
        char c = content[i];

        if (inQuotes) {
            current += c;
            if (c == quoteChar) {
                inQuotes = false;
            }
        } else if (c == '"' || c == '\'') {
            inQuotes = true;
            quoteChar = c;
            current += c;
        } else if (c == '(') {
            parenDepth++;
            current += c;
        } else if (c == ')') {
            parenDepth--;
            current += c;
        } else if (c == ',' && parenDepth == 0) {
            // Trim and add
            size_t s = current.find_first_not_of(" \t");
            size_t e = current.find_last_not_of(" \t");
            if (s != std::string::npos) {
                args.push_back(current.substr(s, e - s + 1));
            }
            current.clear();
        } else {
            current += c;
        }
    }

    // Add last argument
    if (!current.empty()) {
        size_t s = current.find_first_not_of(" \t");
        size_t e = current.find_last_not_of(" \t");
        if (s != std::string::npos) {
            args.push_back(current.substr(s, e - s + 1));
        }
    }

    // Skip first argument (intrinsic name in quotes)
    if (!args.empty()) {
        args.erase(args.begin());
    }

    return args;
}

// Helper to collect all @intrinsic arguments from a section recursively
static void collectIntrinsicArgsFromSection(Section* section, std::vector<std::string>& allArgs) {
    if (!section) return;

    for (const auto& line : section->lines) {
        auto args = extractIntrinsicArgs(line.text);
        for (const auto& arg : args) {
            allArgs.push_back(arg);
        }

        // Recurse into child sections
        if (line.childSection) {
            collectIntrinsicArgsFromSection(line.childSection.get(), allArgs);
        }
    }
}

std::vector<std::string> SectionPatternResolver::identifyVariables(const std::vector<std::string>& words) {
    // This version is called without body context - return empty
    // The actual variable deduction happens in identifyVariablesFromBody
    (void)words;
    return {};
}

std::vector<std::string> SectionPatternResolver::identifyVariablesFromBody(
    const std::vector<std::string>& patternWords,
    Section* body
) {
    std::vector<std::string> variables;

    // Special case: if there's only one word, it's the pattern name (literal), not a variable
    if (patternWords.size() == 1 &&
        !(patternWords[0].size() >= 3 && patternWords[0][0] == '{' && patternWords[0].back() == '}')) {
        return variables;
    }

    // Collect all @intrinsic arguments from the body
    std::vector<std::string> intrinsicArgs;
    collectIntrinsicArgsFromSection(body, intrinsicArgs);

    // A word from the pattern is a variable if:
    // 1. It's a braced variable {word} or {type:name} - always a variable (typed capture)
    // 2. It appears as an intrinsic argument
    for (const auto& word : patternWords) {
        // Check for braced variables: {word} or {type:name} indicates capture, ALWAYS a variable
        if (word.size() >= 3 && word[0] == '{' && word.back() == '}') {
            std::string inner = word.substr(1, word.size() - 2);

            // Check for typed capture: {type:name}
            size_t colonPos = inner.find(':');
            std::string varName;
            if (colonPos != std::string::npos) {
                // Typed capture - extract name after colon
                varName = inner.substr(colonPos + 1);
            } else {
                // Legacy syntax {name} - use the whole inner part
                varName = inner;
            }

            // Check we haven't already added it
            bool alreadyAdded = false;
            for (const auto& v : variables) {
                if (v == varName) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) {
                variables.push_back(varName);
            }
            continue;
        }

        // Skip quoted strings - they are literals in the pattern
        if (!word.empty() && (word[0] == '"' || word[0] == '\'')) {
            continue;
        }

        // Skip pure operators/punctuation
        bool hasAlnum = false;
        for (char c : word) {
            if (std::isalnum(c) || c == '_') {
                hasAlnum = true;
                break;
            }
        }
        if (!hasAlnum) {
            continue;
        }

        // Check if this word appears as an intrinsic argument
        for (const auto& arg : intrinsicArgs) {
            // The argument must be the word itself (not a literal)
            // Literals: numbers, quoted strings
            if (arg == word) {
                // Check we haven't already added it
                bool alreadyAdded = false;
                for (const auto& v : variables) {
                    if (v == word) {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded) {
                    variables.push_back(word);
                }
                break;
            }
        }
    }

    return variables;
}

std::string SectionPatternResolver::createPatternString(const std::vector<std::string>& words,
                                                     const std::vector<std::string>& variables) {
    std::string result;

    for (const auto& word : words) {
        if (!result.empty()) {
            result += " ";
        }

        // Check if this is a braced variable {foo} or {type:name}
        if (word.size() >= 3 && word[0] == '{' && word.back() == '}') {
            std::string inner = word.substr(1, word.size() - 2);

            // Check for typed capture: {type:name}
            size_t colonPos = inner.find(':');
            std::string varName;
            if (colonPos != std::string::npos) {
                // Typed capture - extract name after colon
                varName = inner.substr(colonPos + 1);
            } else {
                // Legacy syntax {name} - use the whole inner part
                varName = inner;
            }

            // Check if the variable name is in our variables list
            bool isVar = false;
            for (const auto& var : variables) {
                if (varName == var) {
                    isVar = true;
                    break;
                }
            }
            if (isVar) {
                // Keep the full braced syntax for typed captures
                // The pattern tree parser will handle it
                result += word;
            } else {
                result += word;
            }
            continue;
        }

        // Check if this word is a variable
        bool isVar = false;
        for (const auto& var : variables) {
            if (word == var) {
                isVar = true;
                break;
            }
        }

        if (isVar) {
            result += "$";
        } else {
            result += word;
        }
    }

    return result;
}

bool SectionPatternResolver::isIntrinsicCall(const std::string& text) const {
    // Check if the text is an intrinsic call like @intrinsic("name", ...)
    std::string trimmed = text;
    // Remove leading/trailing whitespace
    size_t start = trimmed.find_first_not_of(" \t");
    if (start == std::string::npos) return false;
    trimmed = trimmed.substr(start);

    return trimmed.rfind("@intrinsic(", 0) == 0 ||
           trimmed.rfind("return @intrinsic(", 0) == 0 ||
           trimmed.rfind("return ", 0) == 0;
}

bool SectionPatternResolver::isPatternBodyDirective(const std::string& text) const {
    // Check if this is a special directive within a pattern body
    // These are not pattern references - they're metadata/structure
    std::string trimmed = text;
    size_t start = trimmed.find_first_not_of(" \t");
    if (start == std::string::npos) return false;
    trimmed = trimmed.substr(start);

    // Check for well-known pattern body sections/directives
    // These all start with a keyword followed by colon (with optional content after)
    const std::vector<std::string> bodyDirectives = {
        "execute:",
        "get:",
        "check:",
        "run:",
        "priority:",
        "patterns:",
        "when parsed:",
        "when triggered:",
        "syntax:",
        "aliases:",
        "set to value:",
        "used inside:",
        "set to val:",
        "add val:",
        "subtract val:",
        "multiply by val:",
        "divide by val:"
    };

    for (const auto& directive : bodyDirectives) {
        if (trimmed.rfind(directive, 0) == 0) {
            return true;
        }
    }

    return false;
}

bool SectionPatternResolver::isSingleWordWithSection(const CodeLine* line) const {
    if (!line || !line->hasChildSection()) {
        return false;
    }

    // Get pattern text without trailing colon
    std::string text = line->getPatternText();

    // Check if it's a single word (no spaces)
    return text.find(' ') == std::string::npos && !text.empty();
}

bool SectionPatternResolver::isInsidePatternsSection(CodeLine* line) const {
    if (!line) return false;

    // Find the parent section of this line
    for (Section* section : allSections_) {
        for (const auto& secLine : section->lines) {
            if (&secLine == line) {
                // Found the parent section, now check if this section's parent line is "patterns:"
                if (section->parent) {
                    // Find the parent line that owns this section
                    for (auto& parentLine : section->parent->lines) {
                        if (parentLine.childSection.get() == section) {
                            // Check if the parent line is "patterns:"
                            std::string parentText = parentLine.getPatternText();
                            // Trim whitespace
                            size_t start = parentText.find_first_not_of(" \t");
                            if (start != std::string::npos) {
                                parentText = parentText.substr(start);
                            }
                            return parentText == "patterns";
                        }
                    }
                }
                return false;
            }
        }
    }
    return false;
}

void SectionPatternResolver::resolveSingleWordPatterns() {
    // Phase 1: Resolve single-word pattern definitions
    // These are patterns like "execute" or "get" that have no variables

    // First, resolve all pattern definitions from the patternDefinitions_ list
    // A pattern definition is considered resolved if it is well-formed
    for (auto& pattern : patternDefinitions_) {
        pattern->sourceLine->isResolved = true;
        
        // Single-word patterns resolve immediately, and their bodies too
        if (pattern->isSingleWord()) {
            if (pattern->body) {
                pattern->body->isResolved = true;
            }
        }
    }

    // Also resolve single-word section definitions (like "execute:" or "get:")
    // These are not pattern definitions but they are single words with child sections
    for (CodeLine* line : allLines_) {
        if (line->isResolved) {
            continue;
        }

        // Check if this is a single-word section definition (like "execute:")
        if (isSingleWordWithSection(line)) {
            line->isResolved = true;
            if (line->childSection) {
                // Don't automatically resolve the child section here
                // It will be resolved when all its lines are resolved
            }
        }

        // Also resolve intrinsic calls immediately
        // @intrinsic(...) and return @intrinsic(...) are special syntax
        if (isIntrinsicCall(line->text)) {
            line->isResolved = true;
        }

        // Resolve pattern body directives (priority:, execute:, get:, etc.)
        // These are special sections within pattern bodies that don't need matching
        if (isPatternBodyDirective(line->text)) {
            line->isResolved = true;
        }

        // Resolve lines inside "patterns:" sections
        // These are syntax alternatives (like "val1 != val2") and should auto-resolve
        if (isInsidePatternsSection(line)) {
            line->isResolved = true;
        }
    }
}

bool SectionPatternResolver::resolvePatternReferences() {
    bool progress = false;

    for (CodeLine* line : allLines_) {
        // Skip already resolved lines
        if (line->isResolved) {
            continue;
        }

        // Skip pattern definitions (they resolve differently)
        if (line->isPatternDefinition) {
            continue;
        }

        // Try to match this reference against all resolved patterns
        PatternMatch* match = tryMatchReference(line);
        if (match) {
            line->isResolved = true;
            progress = true;

            // Add arguments to the parent section's resolved variables
            Section* parent = nullptr;
            for (Section* section : allSections_) {
                for (auto& secLine : section->lines) {
                    if (&secLine == line) {
                        parent = section;
                        break;
                    }
                }
                if (parent) break;
            }

            if (parent) {
                for (const auto& [name, info] : match->arguments) {
                    parent->resolvedVariables[name] = info.value;
                }
            }
        }
    }

    return progress;
}

bool SectionPatternResolver::resolveSections() {
    bool progress = false;

    for (Section* section : allSections_) {
        // Skip already resolved sections
        if (section->isResolved) {
            continue;
        }

        // Check if all lines in this section are resolved
        if (section->allLinesResolved()) {
            section->isResolved = true;
            progress = true;

            // Find any pattern definition that has this section as its body
            // and mark it as resolved
            for (auto& pattern : patternDefinitions_) {
                if (pattern->body == section && !pattern->sourceLine->isResolved) {
                    pattern->sourceLine->isResolved = true;
                }
            }
        }
    }

    return progress;
}

bool SectionPatternResolver::propagateVariablesFromCalls() {
    bool progress = false;

    // For each pattern definition, look at its body's resolved pattern calls
    for (auto& pattern : patternDefinitions_) {
        if (!pattern->body) continue;

        // Get the original words from this pattern's text
        std::vector<std::string> originalWords = parsePatternWords(pattern->originalText);

        // Collect new variables found from pattern calls
        std::vector<std::string> newVariables;

        // Check each line in the body
        for (const auto& line : pattern->body->lines) {
            // Skip section headers like "execute:", "get:"
            std::string lineText = line.text;
            size_t start = lineText.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            lineText = lineText.substr(start);

            // Skip if this is a section header
            if (!lineText.empty() && lineText.back() == ':') {
                std::string header = lineText.substr(0, lineText.size() - 1);
                if (header == "execute" || header == "get" || header == "check" ||
                    header == "patterns" || header == "priority") {
                    // Recurse into child section
                    if (line.childSection) {
                        for (const auto& childLine : line.childSection->lines) {
                            // Find if this line has a pattern match
                            auto it = lineToMatch_.find(const_cast<CodeLine*>(&childLine));
                            if (it != lineToMatch_.end()) {
                                PatternMatch* match = it->second;
                                // Check each argument
                                for (const auto& [varName, info] : match->arguments) {
                                    // If the value is a string, check if it's one of our pattern words
                                    if (std::holds_alternative<std::string>(info.value)) {
                                        const std::string& argStr = std::get<std::string>(info.value);
                                        // Check if this argument is a word from our pattern
                                        for (const auto& word : originalWords) {
                                            if (word == argStr) {
                                                // This word is used as an argument to a pattern call
                                                // So it should be a variable
                                                bool alreadyVar = false;
                                                for (const auto& v : pattern->variables) {
                                                    if (v == word) {
                                                        alreadyVar = true;
                                                        break;
                                                    }
                                                }
                                                if (!alreadyVar) {
                                                    bool alreadyNew = false;
                                                    for (const auto& v : newVariables) {
                                                        if (v == word) {
                                                            alreadyNew = true;
                                                            break;
                                                        }
                                                    }
                                                    if (!alreadyNew) {
                                                        newVariables.push_back(word);
                                                    }
                                                }
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    continue;
                }
            }

            // Direct line (not inside a subsection)
            auto it = lineToMatch_.find(const_cast<CodeLine*>(&line));
            if (it != lineToMatch_.end()) {
                PatternMatch* match = it->second;
                for (const auto& [varName, info] : match->arguments) {
                    if (std::holds_alternative<std::string>(info.value)) {
                        const std::string& argStr = std::get<std::string>(info.value);
                        for (const auto& word : originalWords) {
                            if (word == argStr) {
                                bool alreadyVar = false;
                                for (const auto& v : pattern->variables) {
                                    if (v == word) {
                                        alreadyVar = true;
                                        break;
                                    }
                                }
                                if (!alreadyVar) {
                                    bool alreadyNew = false;
                                    for (const auto& v : newVariables) {
                                        if (v == word) {
                                            alreadyNew = true;
                                            break;
                                        }
                                    }
                                    if (!alreadyNew) {
                                        newVariables.push_back(word);
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // If we found new variables, update the pattern
        if (!newVariables.empty()) {
            for (const auto& var : newVariables) {
                pattern->variables.push_back(var);
            }
            // Rebuild the pattern string with the new variables
            pattern->pattern = createPatternString(originalWords, pattern->variables);
            progress = true;
        }
    }

    return progress;
}

PatternMatch* SectionPatternResolver::tryMatchReference(CodeLine* line) {
    if (!line || line->isPatternDefinition) {
        return nullptr;
    }

    std::string referenceText = line->getPatternText();
    ResolvedPattern* bestMatch = nullptr;
    std::map<std::string, PatternMatch::ArgumentInfo> bestArguments;
    int bestSpecificity = -1;

    // Try each resolved pattern
    for (auto& pattern : patternDefinitions_) {
        // Only try patterns that are resolved (their body is resolved)
        if (!pattern->sourceLine->isResolved) {
            continue;
        }

        // Handle private patterns: only allow if in the same file
        if (pattern->isPrivate && pattern->sourceLine->filePath != line->filePath) {
            continue;
        }

        // Check if the pattern type is compatible with the line type
        // For now, allow all types, but we might want to restrict this in the future
        // e.g. only allow effects to match lines in executable sections

        std::map<std::string, PatternMatch::ArgumentInfo> arguments;
        if (tryMatchPattern(pattern.get(), referenceText, arguments)) {
            int spec = pattern->specificity();
            if (spec > bestSpecificity) {
                bestSpecificity = spec;
                bestMatch = pattern.get();
                bestArguments = std::move(arguments);
            }
        }
    }

    if (bestMatch) {
        auto match = std::make_unique<PatternMatch>();
        match->pattern = bestMatch;
        match->arguments = std::move(bestArguments);

        lineToMatch_[line] = match.get();
        patternMatches_.push_back(std::move(match));
        return patternMatches_.back().get();
    }

    return nullptr;
}

bool SectionPatternResolver::tryMatchPattern(const ResolvedPattern* pattern,
                                         const std::string& referenceText,
                                         std::map<std::string, PatternMatch::ArgumentInfo>& arguments) {
    // Expand alternatives in the pattern and try each variant
    std::vector<std::string> expandedPatterns = expandAlternatives(pattern->pattern);

    for (const auto& expanded : expandedPatterns) {
        arguments.clear();
        if (tryMatchPatternSingle(pattern, expanded, referenceText, arguments)) {
            return true;
        }
    }
    return false;
}

bool SectionPatternResolver::tryMatchPatternSingle(const ResolvedPattern* pattern,
                                         const std::string& patternText,
                                         const std::string& referenceText,
                                         std::map<std::string, PatternMatch::ArgumentInfo>& arguments) {
    // Parse both the pattern and reference into words
    std::vector<std::string> patternWords = parsePatternWords(patternText);
    std::vector<std::string> refWordTexts = parsePatternWords(referenceText);
    
    // Quick check: if pattern has more literal words than reference has total words, no match
    if (patternWords.size() > refWordTexts.size()) {
        // Could still match if some pattern words are $
        int literalCount = 0;
        for (const auto& w : patternWords) {
            if (w != "$") literalCount++;
        }
        if (literalCount > static_cast<int>(refWordTexts.size())) {
            return false;
        }
    }

    // We need more info about the ref words to store positions
    struct RefWord {
        std::string text;
        int start;
        int length;
    };
    std::vector<RefWord> refWords;
    
    {
        size_t searchPos = 0;
        for (const auto& word : refWordTexts) {
            size_t foundPos = referenceText.find(word, searchPos);
            if (foundPos != std::string::npos) {
                // If the word is a string literal, we need to be careful about finding it.
                // parsePatternWords already handled the grouping, but find(word) might find
                // the same text inside another string if we're not careful.
                // However, since we process words in order, searchPos should keep us safe.
                refWords.push_back({word, (int)foundPos, (int)word.size()});
                searchPos = foundPos + word.size();
            }
        }
    }

    // Try to match pattern against reference
    // Use a simple greedy matching algorithm
    size_t pIdx = 0;  // Pattern index
    size_t rIdx = 0;  // Reference index
    size_t varIdx = 0; // Variable index

    while (pIdx < patternWords.size() && rIdx < refWords.size()) {
        const std::string& pWord = patternWords[pIdx];

        if (pWord == "$") {
            // This is a variable slot - consume one word from reference
            if (varIdx < pattern->variables.size()) {
                const std::string& varName = pattern->variables[varIdx];

                // Try to parse the value
                const auto& refWordInfo = refWords[rIdx];
                const std::string& refWord = refWordInfo.text;

                // Check if it's a number
                bool isNumber = true;
                bool hasDot = false;
                for (size_t i = 0; i < refWord.size(); i++) {
                    char c = refWord[i];
                    if (c == '-' && i == 0) continue;
                    if (c == '.' && !hasDot) { hasDot = true; continue; }
                    if (!std::isdigit(c)) { isNumber = false; break; }
                }

                ResolvedValue val;
                if (isNumber && !refWord.empty()) {
                    if (hasDot) {
                        try {
                            val = std::stod(refWord);
                        } catch (...) {
                            val = refWord;
                        }
                    } else {
                        try {
                            val = (int64_t)std::stoll(refWord);
                        } catch (...) {
                            val = refWord;
                        }
                    }
                } else {
                    val = refWord;
                }
                arguments[varName] = PatternMatch::ArgumentInfo(val, refWordInfo.start, refWordInfo.length, true);

                varIdx++;
            }
            pIdx++;
            rIdx++;
        } else {
            // Literal word - must match exactly
            if (pWord != refWords[rIdx].text) {
                return false;
            }
            pIdx++;
            rIdx++;
        }
    }

    // Check if we consumed all pattern words
    if (pIdx != patternWords.size()) {
        return false;
    }

    // If reference has remaining words and pattern ended, this might still be valid
    // if we allow multi-word variable substitution
    // For now, require exact word count match (single-word substitution only)
    if (rIdx != refWords.size()) {
        // Check if the last pattern element was a $ (variable)
        // If so, capture remaining words as part of that variable
        if (!patternWords.empty() && patternWords.back() == "$" && varIdx > 0) {
            const std::string& varName = pattern->variables[varIdx - 1];
            auto& varInfo = arguments[varName];

            // Convert to string if it's a number, then append remaining words
            std::string strValue;
            if (std::holds_alternative<std::string>(varInfo.value)) {
                strValue = std::get<std::string>(varInfo.value);
            } else if (std::holds_alternative<int64_t>(varInfo.value)) {
                strValue = std::to_string(std::get<int64_t>(varInfo.value));
            } else if (std::holds_alternative<double>(varInfo.value)) {
                strValue = std::to_string(std::get<double>(varInfo.value));
            }

            while (rIdx < refWords.size()) {
                const auto& info = refWords[rIdx];
                strValue += " " + info.text;
                varInfo.length = (info.start + info.length) - varInfo.startCol;
                rIdx++;
            }
            varInfo.value = strValue;
        } else {
            return false;
        }
    }

    return true;
}

void SectionPatternResolver::printResults() const {
    std::cout << "Pattern Definitions:\n";
    for (const auto& pattern : patternDefinitions_) {
        pattern->print(2);
        std::cout << "\n";
    }

    std::cout << "Pattern References:\n";
    for (const auto& match : patternMatches_) {
        // Find the source line for this match
        for (const auto& [line, m] : lineToMatch_) {
            if (m == match.get()) {
                std::cout << "  - \"" << line->getPatternText() << "\"\n";
                match->print(6);
                std::cout << "\n";
                break;
            }
        }
    }
}

// ============================================================================
// Pattern Tree Integration
// ============================================================================

void SectionPatternResolver::buildPatternTrees() {
    // Clear existing trees
    effectTree_.clear();
    sectionTree_.clear();
    expressionTree_.clear();

    // Build each tree with file visibility context
    // NOTE: This is a simplified implementation. The pattern tree should
    // ideally store the filePath of each pattern to support 'private' visibility
    // during tree matching.
    
    // For now, we'll continue using the iterative resolver for matching
    // as it already correctly handles 'isPrivate' in tryMatchReference.
    
    // Switch to iterative matching for now to ensure visibility is respected
    // until the pattern tree is updated.
}

PatternMatch* SectionPatternResolver::matchWithTree(CodeLine* line) {
    if (!line || line->isPatternDefinition) {
        return nullptr;
    }

    std::string referenceText = line->getPatternText();

    // Phase 1: Detect literals (already done during preprocessing)
    // Phase 2: Intrinsic arguments are parsed during literal detection

    // Phase 3: Determine which tree to use
    // Lines ending with ":" are sections/effects with sections
    // Lines without ":" are pure effects
    bool hasSection = line->hasChildSection();

    std::optional<TreePatternMatch> treeMatch;

    if (hasSection) {
        // Try section tree first
        treeMatch = sectionTree_.match(referenceText);
        if (!treeMatch) {
            // Fall back to effect tree
            treeMatch = effectTree_.match(referenceText);
        }
    } else {
        // Pure effect (no child section)
        treeMatch = effectTree_.match(referenceText);
    }

    if (!treeMatch || !treeMatch->pattern) {
        return nullptr;
    }

    // Convert tree match to pattern match
    auto match = treeMatchToPatternMatch(*treeMatch, treeMatch->pattern);
    if (!match) {
        return nullptr;
    }

    lineToMatch_[line] = match.get();
    patternMatches_.push_back(std::move(match));
    return patternMatches_.back().get();
}

std::vector<SectionPatternResolver::ParsedLiteral>
SectionPatternResolver::detectLiterals(const std::string& input) {
    std::vector<ParsedLiteral> literals;
    size_t i = 0;

    while (i < input.size()) {
        // Skip whitespace
        if (std::isspace(input[i])) {
            i++;
            continue;
        }

        // Check for intrinsic call: @name(...)
        if (input[i] == '@') {
            std::string name;
            std::vector<std::string> args;
            size_t end = parseIntrinsicCall(input, i, name, args);
            if (end != std::string::npos) {
                ParsedLiteral lit;
                lit.type = ParsedLiteral::Type::Intrinsic;
                lit.text = input.substr(i, end - i);
                lit.startPos = i;
                lit.endPos = end;
                lit.intrinsicArgs = std::move(args);
                literals.push_back(std::move(lit));
                i = end;
                continue;
            }
        }

        // Check for string literal: "..." or '...'
        if (input[i] == '"' || input[i] == '\'') {
            char quote = input[i];
            size_t start = i;
            i++;  // Skip opening quote
            while (i < input.size() && input[i] != quote) {
                if (input[i] == '\\' && i + 1 < input.size()) {
                    i += 2;  // Skip escape sequence
                } else {
                    i++;
                }
            }
            if (i < input.size()) {
                i++;  // Skip closing quote
            }
            ParsedLiteral lit;
            lit.type = ParsedLiteral::Type::String;
            lit.text = input.substr(start, i - start);
            lit.startPos = start;
            lit.endPos = i;
            literals.push_back(std::move(lit));
            continue;
        }

        // Check for number literal: 123, 3.14, -7
        if (std::isdigit(input[i]) || (input[i] == '-' && i + 1 < input.size() && std::isdigit(input[i + 1]))) {
            size_t start = i;
            if (input[i] == '-') i++;
            while (i < input.size() && std::isdigit(input[i])) i++;
            if (i < input.size() && input[i] == '.') {
                i++;
                while (i < input.size() && std::isdigit(input[i])) i++;
            }
            ParsedLiteral lit;
            lit.type = ParsedLiteral::Type::Number;
            lit.text = input.substr(start, i - start);
            lit.startPos = start;
            lit.endPos = i;
            literals.push_back(std::move(lit));
            continue;
        }

        // Check for grouping parentheses: (...)
        if (input[i] == '(') {
            size_t start = i;
            int depth = 1;
            i++;
            while (i < input.size() && depth > 0) {
                if (input[i] == '(') depth++;
                else if (input[i] == ')') depth--;
                i++;
            }
            ParsedLiteral lit;
            lit.type = ParsedLiteral::Type::Group;
            lit.text = input.substr(start, i - start);
            lit.startPos = start;
            lit.endPos = i;
            literals.push_back(std::move(lit));
            continue;
        }

        // Skip other characters (identifiers, operators, etc.)
        i++;
    }

    return literals;
}

size_t SectionPatternResolver::parseIntrinsicCall(const std::string& input, size_t startPos,
                                                     std::string& name, std::vector<std::string>& args) {
    if (startPos >= input.size() || input[startPos] != '@') {
        return std::string::npos;
    }

    // Parse name: @name
    size_t i = startPos + 1;
    size_t nameStart = i;
    while (i < input.size() && (std::isalnum(input[i]) || input[i] == '_')) {
        i++;
    }
    if (i == nameStart) {
        return std::string::npos;  // No name
    }
    name = input.substr(nameStart, i - nameStart);

    // Expect opening paren
    if (i >= input.size() || input[i] != '(') {
        return std::string::npos;
    }
    i++;  // Skip '('

    // Parse arguments
    args.clear();
    std::string currentArg;
    int parenDepth = 1;
    bool inString = false;
    char stringChar = '\0';

    while (i < input.size() && parenDepth > 0) {
        char c = input[i];

        if (inString) {
            currentArg += c;
            if (c == stringChar && (currentArg.size() < 2 || currentArg[currentArg.size() - 2] != '\\')) {
                inString = false;
            }
        } else if (c == '"' || c == '\'') {
            inString = true;
            stringChar = c;
            currentArg += c;
        } else if (c == '(') {
            parenDepth++;
            currentArg += c;
        } else if (c == ')') {
            parenDepth--;
            if (parenDepth > 0) {
                currentArg += c;
            }
        } else if (c == ',' && parenDepth == 1) {
            // Argument separator
            // Trim whitespace
            size_t s = currentArg.find_first_not_of(" \t");
            size_t e = currentArg.find_last_not_of(" \t");
            if (s != std::string::npos) {
                args.push_back(currentArg.substr(s, e - s + 1));
            }
            currentArg.clear();
        } else {
            currentArg += c;
        }
        i++;
    }

    // Add last argument
    if (!currentArg.empty()) {
        size_t s = currentArg.find_first_not_of(" \t");
        size_t e = currentArg.find_last_not_of(" \t");
        if (s != std::string::npos) {
            args.push_back(currentArg.substr(s, e - s + 1));
        }
    }

    if (parenDepth != 0) {
        return std::string::npos;  // Unbalanced parens
    }

    return i;
}

std::unique_ptr<PatternMatch> SectionPatternResolver::treeMatchToPatternMatch(
    const TreePatternMatch& treeMatch,
    const ResolvedPattern* pattern) {
    if (!pattern) {
        return nullptr;
    }

    auto match = std::make_unique<PatternMatch>();
    match->pattern = const_cast<ResolvedPattern*>(pattern);

    // Map treeMatch.arguments to named variables
    size_t argIdx = 0;
    for (const auto& varName : pattern->variables) {
        if (argIdx >= treeMatch.arguments.size()) {
            break;
        }

        const auto& arg = treeMatch.arguments[argIdx];

        // Convert MatchedValue to ResolvedValue
        if (std::holds_alternative<int64_t>(arg)) {
            match->arguments[varName] = PatternMatch::ArgumentInfo(std::get<int64_t>(arg));
        } else if (std::holds_alternative<double>(arg)) {
            match->arguments[varName] = PatternMatch::ArgumentInfo(std::get<double>(arg));
        } else if (std::holds_alternative<std::string>(arg)) {
            match->arguments[varName] = PatternMatch::ArgumentInfo(std::get<std::string>(arg));
        } else if (std::holds_alternative<std::shared_ptr<ExpressionMatch>>(arg)) {
            // For nested expressions, store as string for now
            // TODO: Store the actual ExpressionMatch for proper code generation
            auto exprMatch = std::get<std::shared_ptr<ExpressionMatch>>(arg);
            if (exprMatch) {
                match->arguments[varName] = PatternMatch::ArgumentInfo(exprMatch->matchedText);
            }
        }

        argIdx++;
    }

    return match;
}

} // namespace tbx
