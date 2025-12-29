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

    // Operators (punctuation)
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
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,
    APOSTROPHE,

    // Special
    AT,
    SYMBOL,  // Any single character not otherwise recognized
    END_OF_FILE,
    ERROR
};

struct SourceLocation {
    size_t line{};
    size_t column{};
    std::string filename;
};

struct Token {
    TokenType type{};
    std::string lexeme;
    SourceLocation location;

    // Literal value if applicable
    std::variant<std::monostate, int64_t, double, std::string> value;
};

// Convert token type to string for debugging
std::string tokenTypeToString(TokenType type);

} // namespace tbx
