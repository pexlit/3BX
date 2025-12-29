#include "lexer/token.hpp"

namespace tbx {

std::string tokenTypeToString(TokenType type) {
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
