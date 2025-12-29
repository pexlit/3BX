#include "parser/parser.hpp"
#include <stdexcept>
#include <iostream>

namespace tbx {

Parser::Parser(Lexer& lexer) : lexer_(lexer) {
    matcher_ = std::make_unique<PatternMatcher>(registry_);
    advance();
}

void Parser::setSharedRegistry(PatternRegistry* registry) {
    sharedRegistry_ = registry;
    if (sharedRegistry_) {
        matcher_ = std::make_unique<PatternMatcher>(*sharedRegistry_);
    }
}

PatternRegistry& Parser::activeRegistry() {
    return sharedRegistry_ ? *sharedRegistry_ : registry_;
}

void Parser::advance() {
    previous_ = current_;
    current_ = lexer_.nextToken();
}

bool Parser::check(TokenType type) {
    return current_.type == type;
}

bool Parser::checkWord(const std::string& word) {
    return current_.type == TokenType::IDENTIFIER && current_.lexeme == word;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::matchWord(const std::string& word) {
    if (checkWord(word)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        Token token = current_;
        advance();
        return token;
    }
    throw std::runtime_error(message + " at line " + std::to_string(current_.location.line));
}

Token Parser::consumeWord(const std::string& word, const std::string& message) {
    if (checkWord(word)) {
        Token token = current_;
        advance();
        return token;
    }
    throw std::runtime_error(message + " at line " + std::to_string(current_.location.line));
}

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();

    while (!check(TokenType::END_OF_FILE)) {
        // Skip empty lines
        while (match(TokenType::NEWLINE)) {}

        if (!check(TokenType::END_OF_FILE)) {
            program->statements.push_back(statement());
        }
    }

    return program;
}

bool Parser::checkAt(size_t pos, TokenType type) {
    if (pos >= tokens_.size()) return false;
    return tokens_[pos].type == type;
}

int Parser::skipNewlinesAndGetIndent() {
    // Skip newlines and count leading whitespace for indentation
    // For now, return 0 - full indentation tracking to be implemented
    while (match(TokenType::NEWLINE)) {}
    return 0;
}

StmtPtr Parser::statement() {
    // Pattern definition types - all matched by lexeme, not token type
    if (matchWord("pattern")) {
        return patternDefinition();
    }
    if (matchWord("class")) {
        return classDefinition();
    }
    if (matchWord("expression")) {
        return expressionDefinition();
    }
    if (matchWord("effect")) {
        return effectDefinition();
    }
    if (matchWord("section")) {
        return sectionDefinition();
    }
    if (matchWord("condition")) {
        return conditionDefinition();
    }
    if (matchWord("import")) {
        return importStatement();
    }
    if (matchWord("use")) {
        return useStatement();
    }

    // Try to match against registered patterns
    auto patternStmt = matchPatternStatement();
    if (patternStmt) {
        return patternStmt;
    }

    // Expression statement
    auto stmt = std::make_unique<ExpressionStmt>();
    stmt->expression = expression();
    return stmt;
}

StmtPtr Parser::matchPatternStatement() {
    // Collect tokens until end of line for pattern matching
    std::vector<Token> lineTokens;
    Token startToken = current_;

    // Save current position by storing tokens
    while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        lineTokens.push_back(current_);
        advance();
    }

    if (lineTokens.empty()) {
        return nullptr;
    }

    // Try to match against patterns
    auto result = matcher_->matchStatement(lineTokens, 0);

    if (result && result->pattern && result->pattern->definition) {
        // Create a PatternCall node
        auto call = std::make_unique<PatternCall>();
        call->pattern = result->pattern->definition;
        call->bindings = std::move(result->bindings);
        call->location = startToken.location;

        // Check if the pattern has a Section parameter - if so, parse the indented block
        for (const auto& elem : result->pattern->definition->syntax) {
            if (elem.is_param && elem.param_type == PatternParamType::Section) {
                // Consume the newline after the pattern line
                match(TokenType::NEWLINE);

                // Parse the indented block and assign it to the section binding
                auto blockStatements = parseBlock();
                auto blockExpr = std::make_unique<BlockExpr>();
                blockExpr->statements = std::move(blockStatements);
                blockExpr->location = startToken.location;
                call->bindings[elem.value] = std::move(blockExpr);
                break;
            }
        }

        // Wrap in expression statement
        auto stmt = std::make_unique<ExpressionStmt>();
        stmt->expression = std::move(call);
        return stmt;
    }

    // No pattern matched - check for special cases before falling back
    if (!lineTokens.empty()) {
        // Check if this is an intrinsic call (@intrinsic(...))
        if (lineTokens[0].type == TokenType::AT) {
            // Use the pattern matcher's parsePrimary to parse the intrinsic call
            auto intrinsicResult = matcher_->parsePrimary(lineTokens, 0);
            if (intrinsicResult) {
                auto stmt = std::make_unique<ExpressionStmt>();
                stmt->expression = std::move(intrinsicResult->first);
                return stmt;
            }
        }

        // Fall back to creating a natural expression from the tokens
        auto expr = std::make_unique<NaturalExpr>();
        expr->tokens = std::move(lineTokens);
        expr->location = startToken.location;

        auto stmt = std::make_unique<ExpressionStmt>();
        stmt->expression = std::move(expr);
        return stmt;
    }

    return nullptr;
}

ExprPtr Parser::expression(int minPrecedence) {
    std::vector<Token> tokens;
    int parenDepth = 0;

    // Collect tokens for the expression
    while (!check(TokenType::END_OF_FILE)) {
        if (check(TokenType::NEWLINE)) break;

        if (current_.type == TokenType::LPAREN) {
            parenDepth++;
        }
        else if (current_.type == TokenType::RPAREN) {
            if (parenDepth == 0) break;
            parenDepth--;
        }
        else if (parenDepth == 0) {
            // Context-sensitive delimiters
            if (check(TokenType::COMMA)) break;
            if (check(TokenType::COLON)) break;
            // Control flow words - check by lexeme
            if (checkWord("then") || checkWord("do")) break;
        }

        tokens.push_back(current_);
        advance();
    }

    if (tokens.empty()) {
        throw std::runtime_error("Expected expression");
    }

    // Delegate to PatternMatcher with dynamic precedence
    auto result = matcher_->parseExpression(tokens, 0, "", minPrecedence);
    if (!result) {
        throw std::runtime_error("Invalid or unrecognized expression pattern");
    }

    if (result->second < tokens.size()) {
        throw std::runtime_error("Unexpected token in expression: " + tokens[result->second].lexeme);
    }

    return std::move(result->first);
}

ExprPtr Parser::intrinsicCall() {
    // @intrinsic("name", arg1, arg2, ...)
    // We already consumed @, now expect 'intrinsic'
    consume(TokenType::IDENTIFIER, "Expected 'intrinsic' after '@'");
    if (previous_.lexeme != "intrinsic") {
        throw std::runtime_error("Expected 'intrinsic' after '@'");
    }

    consume(TokenType::LPAREN, "Expected '(' after '@intrinsic'");

    auto call = std::make_unique<IntrinsicCall>();

    // First argument is the intrinsic name (string)
    consume(TokenType::STRING, "Expected intrinsic name as string");
    call->name = std::get<std::string>(previous_.value);

    // Parse remaining arguments
    while (match(TokenType::COMMA)) {
        call->args.push_back(expression());
    }

    consume(TokenType::RPAREN, "Expected ')' after intrinsic arguments");

    return call;
}

StmtPtr Parser::patternDefinition() {
    // pattern:
    //     syntax: set var to val
    //     group: GroupName
    //     priority: before $ + $
    //     execute:
    //         @intrinsic("store", var, val)

    auto def = std::make_unique<PatternDef>();

    consume(TokenType::COLON, "Expected ':' after 'pattern'");
    match(TokenType::NEWLINE);

    // Parse pattern properties (syntax, priority, execute)
    while (!check(TokenType::END_OF_FILE)) {
        // Skip blank lines
        while (match(TokenType::NEWLINE)) {}

        // Check if we're at a top-level statement (column 1, not indented)
        if (current_.location.column == 1) {
            break;
        }

        if (check(TokenType::END_OF_FILE)) {
            break;
        }

        if (matchWord("syntax")) {
            consume(TokenType::COLON, "Expected ':' after 'syntax'");
            // Capture raw syntax tokens for semantic deduction
            while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                def->raw_syntax.push_back(current_.lexeme);
                advance();
            }
            match(TokenType::NEWLINE);
        }
        else if (matchWord("when")) {
            if (matchWord("parsed")) {
                consume(TokenType::COLON, "Expected ':' after 'when parsed'");
                match(TokenType::NEWLINE);
                def->when_parsed = parseBlock();
            }
            else if (matchWord("triggered")) {
                consume(TokenType::COLON, "Expected ':' after 'execute'");
                match(TokenType::NEWLINE);
                def->when_triggered = parseBlock();
            }
            else if (matchWord("created")) {
                consume(TokenType::COLON, "Expected ':' after 'when created'");
                match(TokenType::NEWLINE);
                def->when_triggered = parseBlock();
            }
            else {
                throw std::runtime_error("Expected 'parsed', 'triggered', or 'created' after 'when'");
            }
        }
        else if (checkWord("group")) {
            advance(); // consume 'group'
            consume(TokenType::COLON, "Expected ':' after 'group'");
            if (check(TokenType::IDENTIFIER)) {
                def->group = current_.lexeme;
                advance();
            }
            match(TokenType::NEWLINE);
        }
        else if (matchWord("priority")) {
            consume(TokenType::COLON, "Expected ':' after 'priority'");

            // Parse: [before|after] <syntax string>
            Relation rel = Relation::Before;
            if (check(TokenType::IDENTIFIER)) {
                if (current_.lexeme == "before") {
                    rel = Relation::Before;
                    advance();
                } else if (current_.lexeme == "after") {
                    rel = Relation::After;
                    advance();
                }
            }

            // Collect target syntax
            std::string targetSyntax;
            if (check(TokenType::STRING)) {
                 targetSyntax = std::get<std::string>(current_.value);
                 advance();
            } else {
                // Allow bare syntax tokens too? e.g. before $ + $
                while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                    if (!targetSyntax.empty()) targetSyntax += " ";
                    targetSyntax += current_.lexeme;
                    advance();
                }
            }

            if (!targetSyntax.empty()) {
                def->priority_rules.emplace_back(rel, targetSyntax);
            }
            match(TokenType::NEWLINE);
        }
        else {
            // Unknown property, skip line
            while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                advance();
            }
            match(TokenType::NEWLINE);
        }
    }

    // Register raw definition (Resolver will handle syntax/compilation later)
    activeRegistry().registerFromDef(def.get());

    return def;
}

StmtPtr Parser::classDefinition() {
    // class:
    //     pattern:
    //         vector
    //     members:
    //         x, y, z
    //     when created:
    //         set each member to 0

    consume(TokenType::COLON, "Expected ':' after 'class'");
    match(TokenType::NEWLINE);

    auto classDef = std::make_unique<ClassDef>();
    std::vector<StmtPtr> classStatements;

    // Parse class body
    while (!check(TokenType::END_OF_FILE)) {
        while (match(TokenType::NEWLINE)) {}

        // Check indentation - any token at column 1 (not indented) ends the class body
        if (current_.location.column == 1) {
            break;
        }

        if (check(TokenType::END_OF_FILE)) {
            break;
        }

        if (matchWord("pattern") || matchWord("patterns")) {
            consume(TokenType::COLON, "Expected ':' after 'pattern'/'patterns'");
            // Parse class pattern (the name/syntax to match for this class)
            while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                if (check(TokenType::IDENTIFIER)) {
                    classDef->name = current_.lexeme;
                    PatternElement elem;
                    elem.is_param = false;
                    elem.is_optional = false;
                    elem.value = current_.lexeme;
                    classDef->patternSyntax.push_back(elem);
                }
                advance();
            }
            match(TokenType::NEWLINE);
        }
        else if (matchWord("members")) {
            consume(TokenType::COLON, "Expected ':' after 'members'");
            classDef->members = parseIdentifierList();
            match(TokenType::NEWLINE);
        }
        else if (matchWord("when")) {
            if (matchWord("created")) {
                consume(TokenType::COLON, "Expected ':' after 'when created'");
                match(TokenType::NEWLINE);
                classDef->constructorBody = parseBlock();
            }
            else {
                // Skip unknown when clause
                while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                    advance();
                }
                match(TokenType::NEWLINE);
            }
        }
        else {
            // Skip unknown class property
            while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                advance();
            }
            match(TokenType::NEWLINE);
        }
    }

    // Register the class
    activeRegistry().registerClass(std::move(classDef));

    // Return an empty statement for now (class definitions don't generate code directly)
    auto emptyDef = std::make_unique<PatternDef>();
    return emptyDef;
}

StmtPtr Parser::expressionDefinition() {
    // Supports two forms:
    // 1. expression:
    //        syntax: ...
    //        get: ...
    // 2. expression <syntax>:
    //        get: ...

    auto def = std::make_unique<PatternDef>();

    // Check if next token is colon (old form) or syntax inline (new form)
    if (check(TokenType::COLON)) {
        consume(TokenType::COLON, "Expected ':' after 'expression'");
        match(TokenType::NEWLINE);

        // Parse expression properties
        while (!check(TokenType::END_OF_FILE)) {
            while (match(TokenType::NEWLINE)) {}

            if (check(TokenType::END_OF_FILE)) break;

            if (current_.location.column == 1) break;

            if (matchWord("syntax")) {
                consume(TokenType::COLON, "Expected ':' after 'syntax'");
                def->syntax = parsePatternSyntax();
            }
            else if (matchWord("get")) {
                consume(TokenType::COLON, "Expected ':' after 'get'");
                match(TokenType::NEWLINE);
                def->when_triggered = parseBlock();
            }
            else {
                while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                    advance();
                }
                match(TokenType::NEWLINE);
            }
        }
    } else {
        // Inline syntax: expression <syntax>:
        def->syntax = parsePatternSyntaxUntilColon();
        consume(TokenType::COLON, "Expected ':' after expression syntax");
        match(TokenType::NEWLINE);

        // Parse expression body
        while (!check(TokenType::END_OF_FILE)) {
            while (match(TokenType::NEWLINE)) {}

            if (check(TokenType::END_OF_FILE)) break;

            if (current_.location.column == 1) break;

            if (matchWord("get")) {
                consume(TokenType::COLON, "Expected ':' after 'get'");
                match(TokenType::NEWLINE);
                def->when_triggered = parseBlock();
            }
            else {
                while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                    advance();
                }
                match(TokenType::NEWLINE);
            }
        }
    }

    // Populate raw_syntax from parsed syntax elements for variable deduction
    for (const auto& elem : def->syntax) {
        def->raw_syntax.push_back(elem.value);
    }

    // Register the pattern
    activeRegistry().registerFromDef(def.get());

    return def;
}

StmtPtr Parser::effectDefinition() {
    // effect <syntax>:
    //     execute:
    //         ...
    // OR (shorthand):
    // effect <syntax>:
    //     <statements>

    auto def = std::make_unique<PatternDef>();

    // Parse inline syntax: effect <syntax>:
    def->syntax = parsePatternSyntaxUntilColon();

    // Populate raw_syntax from parsed syntax elements for variable deduction
    for (const auto& elem : def->syntax) {
        def->raw_syntax.push_back(elem.value);
    }

    consume(TokenType::COLON, "Expected ':' after effect syntax");
    match(TokenType::NEWLINE);

    // Skip blank lines and check what comes next
    while (match(TokenType::NEWLINE)) {}

    if (check(TokenType::END_OF_FILE)) {
        // Empty effect body
        activeRegistry().registerFromDef(def.get());
        return def;
    }

    if (current_.location.column == 1) {
        // No body, another top-level definition follows
        activeRegistry().registerFromDef(def.get());
        return def;
    }

    // Check if body starts with 'execute:' or if it's direct statements
    if (checkWord("when")) {
        // Parse effect body with 'execute:' section
        while (!check(TokenType::END_OF_FILE)) {
            while (match(TokenType::NEWLINE)) {}

            if (check(TokenType::END_OF_FILE)) break;
            if (current_.location.column == 1) break;

            if (matchWord("when")) {
                if (matchWord("triggered")) {
                    consume(TokenType::COLON, "Expected ':' after 'execute'");
                    match(TokenType::NEWLINE);
                    def->when_triggered = parseBlock();
                }
                else {
                    while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                        advance();
                    }
                    match(TokenType::NEWLINE);
                }
            }
            else {
                while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                    advance();
                }
                match(TokenType::NEWLINE);
            }
        }
    }
    else {
        // Shorthand syntax: direct statements are the when_triggered body
        def->when_triggered = parseBlock();
    }

    // Register the pattern
    activeRegistry().registerFromDef(def.get());

    return def;
}

StmtPtr Parser::conditionDefinition() {
    // condition <syntax>:
    //     execute:
    //         ...
    // Similar to section but for conditional patterns like "if cond:"

    auto def = std::make_unique<PatternDef>();

    // Parse inline syntax until colon
    def->syntax = parsePatternSyntaxUntilColon();

    // Populate raw_syntax
    for (const auto& elem : def->syntax) {
        def->raw_syntax.push_back(elem.value);
    }

    // Add implicit section parameter for the indented block
    PatternElement sectionParam;
    sectionParam.is_param = true;
    sectionParam.is_optional = false;
    sectionParam.value = "section";
    sectionParam.param_type = PatternParamType::Section;
    def->syntax.push_back(sectionParam);
    def->raw_syntax.push_back("section");

    consume(TokenType::COLON, "Expected ':' after condition syntax");
    match(TokenType::NEWLINE);

    // Parse condition body (execute, etc.)
    while (!check(TokenType::END_OF_FILE)) {
        while (match(TokenType::NEWLINE)) {}

        if (check(TokenType::END_OF_FILE)) break;
        if (current_.location.column == 1) break;

        if (matchWord("when")) {
            if (matchWord("triggered")) {
                consume(TokenType::COLON, "Expected ':' after 'execute'");
                match(TokenType::NEWLINE);
                def->when_triggered = parseBlock();
            }
            else {
                while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                    advance();
                }
                match(TokenType::NEWLINE);
            }
        }
        else {
            while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                advance();
            }
            match(TokenType::NEWLINE);
        }
    }

    // Register the pattern
    activeRegistry().registerFromDef(def.get());

    return def;
}

StmtPtr Parser::importStatement() {
    // Check if this is an import function declaration
    // import function name(params) from "header"
    if (checkWord("function")) {
        return importFunctionDeclaration();
    }

    // import module.3bx
    auto stmt = std::make_unique<ImportStmt>();

    // Parse module path (identifier.3bx or identifier/path.3bx)
    std::string path;
    while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        path += current_.lexeme;
        advance();
    }

    stmt->module_path = path;
    return stmt;
}

StmtPtr Parser::importFunctionDeclaration() {
    // import function name(params) from "header"
    // We already matched 'import', now expect 'function'
    consumeWord("function", "Expected 'function' after 'import'");

    auto decl = std::make_unique<ImportFunctionDecl>();

    // Parse function name
    Token name = consume(TokenType::IDENTIFIER, "Expected function name after 'import function'");
    decl->name = name.lexeme;

    // Parse parameter list
    consume(TokenType::LPAREN, "Expected '(' after function name");

    if (!check(TokenType::RPAREN)) {
        do {
            Token param = consume(TokenType::IDENTIFIER, "Expected parameter name");
            decl->params.push_back(param.lexeme);
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RPAREN, "Expected ')' after parameters");

    // Parse 'from "header"'
    consumeWord("from", "Expected 'from' after parameter list");
    Token header = consume(TokenType::STRING, "Expected header path string after 'from'");
    decl->header = std::get<std::string>(header.value);

    return decl;
}

StmtPtr Parser::useStatement() {
    // use thing from module.3bx
    auto stmt = std::make_unique<UseStmt>();

    // Parse the item name
    Token item = consume(TokenType::IDENTIFIER, "Expected item name after 'use'");
    stmt->item_name = item.lexeme;

    // Expect 'from'
    consumeWord("from", "Expected 'from' after item name");

    // Parse module path (identifier.3bx or identifier/path.3bx)
    std::string path;
    while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        path += current_.lexeme;
        advance();
    }

    stmt->module_path = path;
    return stmt;
}

bool Parser::isOperator(TokenType type) {
    return type == TokenType::PLUS || type == TokenType::MINUS ||
           type == TokenType::STAR || type == TokenType::SLASH ||
           type == TokenType::APOSTROPHE;
}

std::vector<PatternElement> Parser::parsePatternSyntax() {
    // Parse pattern syntax: first identifier is literal (pattern name),
    // subsequent identifiers are parameters, operators are literals
    std::vector<PatternElement> elements;
    bool seenFirstIdentifier = false;

    while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        PatternElement elem;
        elem.is_optional = false;

        // Optional element syntax: [word]
        if (match(TokenType::LBRACKET)) {
            elem.is_optional = true;
            elem.is_param = false;
            elem.value = current_.lexeme;
            advance();
            if (!match(TokenType::RBRACKET)) {
                throw std::runtime_error("Expected ']' after optional element");
            }
            elements.push_back(elem);
            continue;
        }

        std::string word = current_.lexeme;

        // Operators are always literals
        if (isOperator(current_.type)) {
            elem.is_param = false;
            elem.value = word;
        }
        else if (current_.type == TokenType::IDENTIFIER) {
            // First identifier is literal (pattern name), rest are parameters
            if (!seenFirstIdentifier) {
                elem.is_param = false;
                seenFirstIdentifier = true;
            } else {
                elem.is_param = true;
            }
            elem.value = word;
        }
        else {
            advance();
            continue;
        }

        elements.push_back(elem);
        advance();
    }

    match(TokenType::NEWLINE);
    return elements;
}

std::vector<PatternElement> Parser::parsePatternSyntaxUntilColon() {
    // Parse syntax elements until colon
    // First identifier is the pattern name (literal)
    // Subsequent identifiers are parameters
    std::vector<PatternElement> elements;
    bool seenFirstIdentifier = false;

    while (!check(TokenType::COLON) && !check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        PatternElement elem;
        elem.is_optional = false;

        // Optional element syntax: [word]
        if (match(TokenType::LBRACKET)) {
            elem.is_optional = true;
            elem.is_param = false;
            elem.value = current_.lexeme;
            advance();
            if (!match(TokenType::RBRACKET)) {
                throw std::runtime_error("Expected ']' after optional element");
            }
            elements.push_back(elem);
            continue;
        }

        std::string word = current_.lexeme;

        // Operators are always literals
        if (isOperator(current_.type)) {
            elem.is_param = false;
            elem.value = word;
        }
        else if (current_.type == TokenType::IDENTIFIER) {
            // First identifier is literal (pattern name), rest are parameters
            if (!seenFirstIdentifier) {
                elem.is_param = false;
                seenFirstIdentifier = true;
            } else {
                elem.is_param = true;
            }
            elem.value = word;
        }
        else {
            advance();
            continue;
        }

        elements.push_back(elem);
        advance();
    }

    return elements;
}

std::vector<std::string> Parser::parseIdentifierList() {
    std::vector<std::string> identifiers;

    // Parse comma-separated list of identifiers
    while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        if (check(TokenType::IDENTIFIER)) {
            identifiers.push_back(current_.lexeme);
            advance();
        }
        else if (match(TokenType::COMMA)) {
            // Skip comma
            continue;
        }
        else {
            advance();
        }
    }

    return identifiers;
}

std::vector<StmtPtr> Parser::parseBlock() {
    // Parse indented block: all statements with indentation >= block's base level
    std::vector<StmtPtr> statements;

    // Skip leading newlines
    while (match(TokenType::NEWLINE)) {}

    // Record the indentation level of the first statement in the block
    size_t blockIndent = current_.location.column;

    // If the first token is at column 1, this is an empty block
    // (the indented content has ended and we're back at top level)
    if (blockIndent == 1) {
        return statements;
    }

    // Parse statements while they are indented at the block level
    while (!check(TokenType::END_OF_FILE)) {
        // Skip blank lines
        while (match(TokenType::NEWLINE)) {}

        if (check(TokenType::END_OF_FILE)) {
            break;
        }

        // Check if current token is at the block's indentation level or deeper
        size_t currentIndent = current_.location.column;
        if (currentIndent < blockIndent) {
            // Dedented - end of block
            break;
        }

        // Check for words that indicate we're outside the block
        // (pattern sections like syntax:, when:, priority:, members:)
        if (checkWord("syntax") || checkWord("when") ||
            checkWord("priority") || checkWord("members") || checkWord("get")) {
            break;
        }

        // Check for top-level definition keywords at column 1
        if (currentIndent == 1 &&
            (checkWord("pattern") || checkWord("class") || checkWord("expression") ||
             checkWord("effect") || checkWord("section") || checkWord("condition") ||
             checkWord("import") || checkWord("use") || checkWord("function"))) {
            break;
        }

        statements.push_back(statement());
    }

    return statements;
}

std::vector<StmtPtr> Parser::parseBlockWithIndent(int baseIndent) {
    // Parse statements with proper indentation tracking
    // For now, delegate to parseBlock
    return parseBlock();
}

ExprPtr Parser::lazyExpression() {
    // Parse lazy expression: {expression}
    // We already consumed the opening brace
    auto lazy = std::make_unique<LazyExpr>();
    lazy->location = previous_.location;
    lazy->inner = expression();
    consume(TokenType::RBRACE, "Expected '}' after lazy expression");
    return lazy;
}

StmtPtr Parser::sectionDefinition() {
    // section <syntax>:
    //     execute:
    //         ...
    // The section keyword creates a pattern that captures an indented block
    // Syntax: section loop while {condition}:

    auto def = std::make_unique<PatternDef>();

    // Parse inline syntax until colon, handling {param} as lazy parameters
    std::vector<PatternElement> elements;
    bool seenFirstIdentifier = false;

    while (!check(TokenType::COLON) && !check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        PatternElement elem;
        elem.is_optional = false;
        elem.param_type = PatternParamType::Normal;

        // Check for lazy parameter syntax: {param}
        if (match(TokenType::LBRACE)) {
            // This is a lazy parameter
            if (!check(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected parameter name after '{' in pattern syntax");
            }
            elem.is_param = true;
            elem.value = current_.lexeme;
            elem.param_type = PatternParamType::Lazy;
            advance();  // consume parameter name
            consume(TokenType::RBRACE, "Expected '}' after lazy parameter name");
            elements.push_back(elem);
            continue;
        }

        // Check for optional element syntax: [word]
        if (match(TokenType::LBRACKET)) {
            elem.is_optional = true;
            std::string word = current_.lexeme;
            elem.is_param = false;
            elem.value = word;
            advance();

            if (!match(TokenType::RBRACKET)) {
                throw std::runtime_error("Expected ']' after optional element");
            }

            elements.push_back(elem);
            continue;
        }

        std::string word = current_.lexeme;

        // Operators are always literals
        if (isOperator(current_.type)) {
            elem.is_param = false;
            elem.value = word;
        }
        else if (current_.type == TokenType::IDENTIFIER) {
            if (!seenFirstIdentifier) {
                elem.is_param = false;
                seenFirstIdentifier = true;
            } else {
                elem.is_param = true;
            }
            elem.value = word;
        }
        else {
            advance();
            continue;
        }

        elements.push_back(elem);
        advance();
    }

    // Add an implicit section parameter at the end to capture the indented block
    PatternElement sectionParam;
    sectionParam.is_param = true;
    sectionParam.is_optional = false;
    sectionParam.value = "section";
    sectionParam.param_type = PatternParamType::Section;
    elements.push_back(sectionParam);

    def->syntax = elements;

    // Populate raw_syntax from parsed syntax elements for variable deduction
    for (const auto& elem : def->syntax) {
        def->raw_syntax.push_back(elem.value);
    }

    consume(TokenType::COLON, "Expected ':' after section pattern syntax");
    match(TokenType::NEWLINE);

    // Parse section body (execute, etc.)
    while (!check(TokenType::END_OF_FILE)) {
        while (match(TokenType::NEWLINE)) {}

        if (check(TokenType::END_OF_FILE)) break;

        if (current_.location.column == 1) break;

        if (matchWord("when")) {
            if (matchWord("triggered")) {
                consume(TokenType::COLON, "Expected ':' after 'execute'");
                match(TokenType::NEWLINE);
                def->when_triggered = parseBlock();
            }
            else {
                while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                    advance();
                }
                match(TokenType::NEWLINE);
            }
        }
        else {
            while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
                advance();
            }
            match(TokenType::NEWLINE);
        }
    }

    // Register the pattern
    activeRegistry().registerFromDef(def.get());

    return def;
}

} // namespace tbx
