#include "lexer/lexer.hpp"
#include <cctype>

namespace tbx {

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!atEnd()) {
        tokens.push_back(nextToken());
        if (tokens.back().type == TokenType::END_OF_FILE) {
            break;
        }
    }
    return tokens;
}

Token Lexer::nextToken() {
    skipWhitespace();

    if (atEnd()) {
        return makeToken(TokenType::END_OF_FILE, "");
    }

    char c = advance();

    // Numbers
    if (std::isdigit(c)) {
        pos_--;
        column_--;
        return scanNumber();
    }

    // Strings
    if (c == '"') {
        return scanString();
    }

    // Identifiers (all words are identifiers - no keyword distinction)
    if (std::isalpha(c) || c == '_') {
        pos_--;
        column_--;
        return scanIdentifier();
    }

    // Single and multi-character tokens
    switch (c) {
        case '+': return makeToken(TokenType::PLUS, "+");
        case '-': return makeToken(TokenType::MINUS, "-");
        case '*': return makeToken(TokenType::STAR, "*");
        case '/': return makeToken(TokenType::SLASH, "/");
        case ':': return makeToken(TokenType::COLON, ":");
        case '<':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::LESS_EQUAL, "<=");
            }
            return makeToken(TokenType::LESS, "<");
        case '>':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::GREATER_EQUAL, ">=");
            }
            return makeToken(TokenType::GREATER, ">");
        case '@': return makeToken(TokenType::AT, "@");
        case '(': return makeToken(TokenType::LPAREN, "(");
        case ')': return makeToken(TokenType::RPAREN, ")");
        case '[': return makeToken(TokenType::LBRACKET, "[");
        case ']': return makeToken(TokenType::RBRACKET, "]");
        case '{': return makeToken(TokenType::LBRACE, "{");
        case '}': return makeToken(TokenType::RBRACE, "}");
        case ',': return makeToken(TokenType::COMMA, ",");
        case '.': return makeToken(TokenType::DOT, ".");
        case '\'': return makeToken(TokenType::APOSTROPHE, "'");
        case '\n':
            line_++;
            column_ = 1;
            return makeToken(TokenType::NEWLINE, "\\n");
        case '=':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::EQUALS, "==");
            }
            // Single '=' is valid for pattern matching (e.g., "a = b")
            return makeToken(TokenType::EQUALS, "=");
        case '!':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::NOT_EQUALS, "!=");
            }
            return makeToken(TokenType::SYMBOL, "!");
    }

    // Any other character becomes a SYMBOL token
    return makeToken(TokenType::SYMBOL, std::string(1, c));
}

Token Lexer::peek() {
    size_t savedPos = pos_;
    size_t savedLine = line_;
    size_t savedColumn = column_;

    Token token = nextToken();

    pos_ = savedPos;
    line_ = savedLine;
    column_ = savedColumn;

    return token;
}

Token Lexer::peekAhead(size_t n) {
    size_t savedPos = pos_;
    size_t savedLine = line_;
    size_t savedColumn = column_;

    Token token;
    for (size_t i = 0; i <= n; i++) {
        token = nextToken();
    }

    pos_ = savedPos;
    line_ = savedLine;
    column_ = savedColumn;

    return token;
}

char Lexer::current() const {
    if (atEnd()) return '\0';
    return source_[pos_];
}

char Lexer::advance() {
    column_++;
    return source_[pos_++];
}

bool Lexer::atEnd() const {
    return pos_ >= source_.size();
}

void Lexer::skipWhitespace() {
    while (!atEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '#') {
            // Skip comments
            while (!atEnd() && current() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::makeToken(TokenType type, const std::string& lexeme) {
    Token token;
    token.type = type;
    token.lexeme = lexeme;
    token.location = {line_, column_ - lexeme.size(), filename_};
    return token;
}

Token Lexer::scanString() {
    std::string stringValue;
    while (!atEnd() && current() != '"') {
        if (current() == '\n') {
            line_++;
            column_ = 1;
        }
        if (current() == '\\' && pos_ + 1 < source_.size()) {
            advance();
            switch (current()) {
                case 'n': stringValue += '\n'; break;
                case 't': stringValue += '\t'; break;
                case '"': stringValue += '"'; break;
                case '\\': stringValue += '\\'; break;
                default: stringValue += current();
            }
        } else {
            stringValue += current();
        }
        advance();
    }

    if (atEnd()) {
        return makeToken(TokenType::ERROR, "Unterminated string");
    }

    advance(); // closing "

    Token token = makeToken(TokenType::STRING, "\"" + stringValue + "\"");
    token.value = stringValue;
    return token;
}

Token Lexer::scanNumber() {
    std::string num;
    bool isFloat = false;

    while (!atEnd() && std::isdigit(current())) {
        num += advance();
    }

    if (current() == '.' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1])) {
        isFloat = true;
        num += advance(); // .
        while (!atEnd() && std::isdigit(current())) {
            num += advance();
        }
    }

    Token token = makeToken(isFloat ? TokenType::FLOAT : TokenType::INTEGER, num);
    if (isFloat) {
        token.value = std::stod(num);
    } else {
        token.value = std::stoll(num);
    }
    return token;
}

Token Lexer::scanIdentifier() {
    std::string id;
    while (!atEnd() && (std::isalnum(current()) || current() == '_')) {
        id += advance();
    }

    // All words are identifiers - no keyword distinction
    // Pattern matching determines if a word is a literal or parameter
    return makeToken(TokenType::IDENTIFIER, id);
}

} // namespace tbx
