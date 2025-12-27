#pragma once

#include <string>
#include <variant>

namespace tbx {

enum class TokenType {
    // Literals
    INTEGER,
    FLOAT,
    STRING,
    IDENTIFIER,

    // Keywords (Skript-like)
    IF,
    THEN,
    ELSE,
    LOOP,
    WHILE,
    FUNCTION,
    RETURN,
    SET,
    TO,
    IS,

    // Pattern system keywords
    PATTERN,
    SYNTAX,
    WHEN,
    PARSED,
    TRIGGERED,
    PRIORITY,
    IMPORT,
    USE,
    FROM,
    THE,        // Common word in natural language patterns
    CLASS,
    EXPRESSION,
    MEMBERS,
    CREATED,
    NEW,
    OF,
    A,
    AN,
    WITH,
    BY,
    EACH,
    MEMBER,
    PRINT,
    EFFECT,
    GET,
    PATTERNS,
    RESULT,
    MULTIPLY,
    APOSTROPHE,
    SECTION,    // section pattern type for indented blocks

    // Boolean literals
    TRUE,
    FALSE,

    // Logical operators
    AND,
    OR,
    NOT,

    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    EQUALS,
    NOT_EQUALS,
    LESS,
    GREATER,
    LESS_EQUAL,
    GREATER_EQUAL,

    // Delimiters
    COLON,
    DOT,
    NEWLINE,
    INDENT,
    DEDENT,
    COMMA,
    LPAREN,
    RPAREN,
    LBRACKET,   // [ for optional syntax elements
    RBRACKET,   // ] for optional syntax elements
    LBRACE,     // { for lazy expressions and section parameters
    RBRACE,     // } for lazy expressions and section parameters

    // Intrinsic
    AT,             // @ symbol for @intrinsic

    // Special
    END_OF_FILE,
    ERROR
};

struct SourceLocation {
    size_t line;
    size_t column;
    std::string filename;
};

struct Token {
    TokenType type;
    std::string lexeme;
    SourceLocation location;

    // Literal value if applicable
    std::variant<std::monostate, int64_t, double, std::string> value;
};

// Convert token type to string for debugging
std::string tokenTypeToString(TokenType type);

} // namespace tbx
