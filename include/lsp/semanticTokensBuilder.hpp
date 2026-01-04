#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include "semanticTokenTypes.hpp"

namespace tbx {

struct SemanticToken {
    int start;
    int length;
    SemanticTokenType type;

    bool operator<(const SemanticToken& other) const {
        if (start != other.start) return start < other.start;
        return length > other.length; // Longer tokens first for nesting
    }
};

class SemanticTokensBuilder {
public:
    void addToken(int start, int length, SemanticTokenType type) {
        if (length <= 0) return;
        tokens_.push_back({start, length, type});
    }

    // Clean up overlapping tokens, keeping highest priority
    void resolve() {
        if (tokens_.empty()) return;

        std::sort(tokens_.begin(), tokens_.end());

        std::vector<SemanticToken> resolved;
        int lastEnd = -1;

        for (const auto& token : tokens_) {
            if (token.start >= lastEnd) {
                resolved.push_back(token);
                lastEnd = token.start + token.length;
            } else {
                // Potential overlapping token, for now just skip it
                // In a real implementation we'd handle nesting
            }
        }
        tokens_ = std::move(resolved);
    }

    static std::string tokenTypeToString(SemanticTokenType type) {
        switch (type) {
            case SemanticTokenType::Comment: return "comment";
            case SemanticTokenType::String: return "string";
            case SemanticTokenType::Number: return "number";
            case SemanticTokenType::Variable: return "variable";
            case SemanticTokenType::Function: return "function";
            case SemanticTokenType::Keyword: return "keyword";
            case SemanticTokenType::Operator: return "operator";
            case SemanticTokenType::Pattern: return "pattern";
            case SemanticTokenType::Effect: return "effect";
            case SemanticTokenType::Expression: return "expression";
            case SemanticTokenType::Section: return "section";
            default: return "other";
        }
    }

    void printTokens(std::ostream& os, const std::string& lineText, const std::string& prefix = "") {
        auto sortedTokens = getTokens();
        os << prefix << "[";
        for (size_t i = 0; i < sortedTokens.size(); ++i) {
            const auto& token = sortedTokens[i];
            os << "\"" << lineText.substr(token.start, token.length) << "\" (" 
               << tokenTypeToString(token.type) << ")";
            if (i < sortedTokens.size() - 1) os << ", ";
        }
        os << "]";
    }

    const std::vector<SemanticToken>& getTokens() {
        // Final sort before returning
        std::sort(tokens_.begin(), tokens_.end(), [](const SemanticToken& a, const SemanticToken& b) {
            if (a.start != b.start) return a.start < b.start;
            return a.length > b.length;
        });

        // Remove exact duplicates
        tokens_.erase(std::unique(tokens_.begin(), tokens_.end(), [](const SemanticToken& a, const SemanticToken& b) {
            return a.start == b.start && a.length == b.length && a.type == b.type;
        }), tokens_.end());

        return tokens_;
    }

private:
    std::vector<SemanticToken> tokens_;
};

} // namespace tbx
