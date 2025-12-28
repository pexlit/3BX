#pragma once

#include "lexer/lexer.hpp"
#include "ast/ast.hpp"
#include "pattern/pattern_registry.hpp"
#include "pattern/pattern_matcher.hpp"
#include <memory>
#include <vector>

namespace tbx {

class Parser {
public:
    explicit Parser(Lexer& lexer);

    // Parse entire program
    std::unique_ptr<Program> parse();

    // Get the pattern registry (patterns defined in parsed code)
    PatternRegistry& getRegistry() { return registry_; }

    // Parse expression with minimum precedence (Pratt Parsing)
    ExprPtr expression(int minPrecedence = 0);
    
    // Set shared registry for multi-file parsing
    void setSharedRegistry(PatternRegistry* registry);

private:
    Lexer& lexer_;
    Token current_;
    Token previous_;
    PatternRegistry registry_;
    PatternRegistry* sharedRegistry_ = nullptr;
    std::unique_ptr<PatternMatcher> matcher_;

    // All tokens for pattern matching
    std::vector<Token> tokens_;
    size_t tokenPos_ = 0;

    // Current indentation tracking
    int currentIndent_ = 0;

    void advance();
    bool check(TokenType type);
    bool checkWord(const std::string& word);
    bool match(TokenType type);
    bool matchWord(const std::string& word);
    Token consume(TokenType type, const std::string& message);
    Token consumeWord(const std::string& word, const std::string& message);
    bool isOperator(TokenType type);

    // Check if at specific token type
    bool checkAt(size_t pos, TokenType type);

    // Skip newlines and track indentation
    int skipNewlinesAndGetIndent();

    // Get the active registry (shared or local)
    PatternRegistry& activeRegistry();

    // Grammar rules
    StmtPtr statement();
    StmtPtr patternDefinition();
    StmtPtr classDefinition();
    StmtPtr expressionDefinition();
    StmtPtr effectDefinition();
    StmtPtr sectionDefinition();
    StmtPtr conditionDefinition();
    StmtPtr importStatement();
    StmtPtr importFunctionDeclaration();
    StmtPtr useStatement();

    // Try to match a statement against registered patterns
    StmtPtr matchPatternStatement();

    // Parse pattern syntax line
    std::vector<PatternElement> parsePatternSyntax();
    std::vector<PatternElement> parsePatternSyntaxUntilColon();

    // Parse a comma-separated list of identifiers
    std::vector<std::string> parseIdentifierList();

    // Parse indented block
    std::vector<StmtPtr> parseBlock();
    std::vector<StmtPtr> parseBlockWithIndent(int baseIndent);

    // Expression helpers
    ExprPtr intrinsicCall();
    ExprPtr lazyExpression();
};

} // namespace tbx
