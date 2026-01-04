#pragma once

#include <string>
#include <string_view>
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
constexpr std::string_view tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INTEGER:      return "INTEGER";
        case TokenType::FLOAT:        return "FLOAT";
        case TokenType::STRING:       return "STRING";
        case TokenType::IDENTIFIER:   return "IDENTIFIER";
        case TokenType::PLUS:         return "PLUS";
        case TokenType::MINUS:        return "MINUS";
        case TokenType::STAR:         return "STAR";
        case TokenType::SLASH:        return "SLASH";
        case TokenType::EQUALS:       return "EQUALS";
        case TokenType::NOT_EQUALS:   return "NOT_EQUALS";
        case TokenType::LESS:         return "LESS";
        case TokenType::GREATER:      return "GREATER";
        case TokenType::LESS_EQUAL:   return "LESS_EQUAL";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::COLON:        return "COLON";
        case TokenType::DOT:          return "DOT";
        case TokenType::NEWLINE:      return "NEWLINE";
        case TokenType::INDENT:       return "INDENT";
        case TokenType::DEDENT:       return "DEDENT";
        case TokenType::COMMA:        return "COMMA";
        case TokenType::LPAREN:       return "LPAREN";
        case TokenType::RPAREN:       return "RPAREN";
        case TokenType::LBRACKET:     return "LBRACKET";
        case TokenType::RBRACKET:     return "RBRACKET";
        case TokenType::LBRACE:       return "LBRACE";
        case TokenType::RBRACE:       return "RBRACE";
        case TokenType::APOSTROPHE:   return "APOSTROPHE";
        case TokenType::AT:           return "AT";
        case TokenType::SYMBOL:       return "SYMBOL";
        case TokenType::END_OF_FILE:  return "EOF";
        case TokenType::ERROR:        return "ERROR";
        default:                      return "UNKNOWN";
    }
}

} // namespace tbx
