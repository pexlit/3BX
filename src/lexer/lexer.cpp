#include "lexer/lexer.hpp"
#include <cctype>
#include <unordered_map>

namespace tbx {

static const std::unordered_map<std::string, TokenType> keywords = {
    {"if", TokenType::IF},
    {"then", TokenType::THEN},
    {"else", TokenType::ELSE},
    {"loop", TokenType::LOOP},
    {"while", TokenType::WHILE},
    {"function", TokenType::FUNCTION},
    {"return", TokenType::RETURN},
    {"set", TokenType::SET},
    {"to", TokenType::TO},
    {"is", TokenType::IS},
    // Boolean literals and logical operators
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    // Pattern system keywords
    {"pattern", TokenType::PATTERN},
    {"syntax", TokenType::SYNTAX},
    {"when", TokenType::WHEN},
    {"parsed", TokenType::PARSED},
    {"triggered", TokenType::TRIGGERED},
    {"priority", TokenType::PRIORITY},
    {"import", TokenType::IMPORT},
    {"use", TokenType::USE},
    {"from", TokenType::FROM},
    {"the", TokenType::THE},
    // Class system keywords
    {"class", TokenType::CLASS},
    {"expression", TokenType::EXPRESSION},
    {"members", TokenType::MEMBERS},
    {"created", TokenType::CREATED},
    {"new", TokenType::NEW},
    {"of", TokenType::OF},
    {"a", TokenType::A},
    {"an", TokenType::AN},
    {"with", TokenType::WITH},
    {"by", TokenType::BY},
    {"each", TokenType::EACH},
    {"member", TokenType::MEMBER},
    {"print", TokenType::PRINT},
    {"effect", TokenType::EFFECT},
    {"get", TokenType::GET},
    {"patterns", TokenType::PATTERNS},
    {"result", TokenType::RESULT},
    {"multiply", TokenType::MULTIPLY},
    {"section", TokenType::SECTION},
};

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

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        pos_--;
        column_--;
        return scanIdentifier();
    }

    // Single character tokens
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
            return makeToken(TokenType::ERROR, "=");
        case '!':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::NOT_EQUALS, "!=");
            }
            return makeToken(TokenType::ERROR, "!");
    }

    return makeToken(TokenType::ERROR, std::string(1, c));
}

Token Lexer::peek() {
    size_t saved_pos = pos_;
    size_t saved_line = line_;
    size_t saved_column = column_;

    Token token = nextToken();

    pos_ = saved_pos;
    line_ = saved_line;
    column_ = saved_column;

    return token;
}

Token Lexer::peekAhead(size_t n) {
    size_t saved_pos = pos_;
    size_t saved_line = line_;
    size_t saved_column = column_;

    Token token;
    for (size_t i = 0; i <= n; i++) {
        token = nextToken();
    }

    pos_ = saved_pos;
    line_ = saved_line;
    column_ = saved_column;

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
    std::string value;
    while (!atEnd() && current() != '"') {
        if (current() == '\n') {
            line_++;
            column_ = 1;
        }
        if (current() == '\\' && pos_ + 1 < source_.size()) {
            advance();
            switch (current()) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case '"': value += '"'; break;
                case '\\': value += '\\'; break;
                default: value += current();
            }
        } else {
            value += current();
        }
        advance();
    }

    if (atEnd()) {
        return makeToken(TokenType::ERROR, "Unterminated string");
    }

    advance(); // closing "

    Token token = makeToken(TokenType::STRING, "\"" + value + "\"");
    token.value = value;
    return token;
}

Token Lexer::scanNumber() {
    std::string num;
    bool is_float = false;

    while (!atEnd() && std::isdigit(current())) {
        num += advance();
    }

    if (current() == '.' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1])) {
        is_float = true;
        num += advance(); // .
        while (!atEnd() && std::isdigit(current())) {
            num += advance();
        }
    }

    Token token = makeToken(is_float ? TokenType::FLOAT : TokenType::INTEGER, num);
    if (is_float) {
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

    // Check for keywords
    auto it = keywords.find(id);
    if (it != keywords.end()) {
        return makeToken(it->second, id);
    }

    return makeToken(TokenType::IDENTIFIER, id);
}

} // namespace tbx
