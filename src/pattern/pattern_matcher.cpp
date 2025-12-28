#include "pattern/pattern_matcher.hpp"
#include <iostream>
#include <unordered_set>

namespace tbx {

PatternMatcher::PatternMatcher(PatternRegistry& registry)
    : registry_(registry) {}

std::optional<MatchResult> PatternMatcher::matchStatement(
    const std::vector<Token>& tokens,
    size_t pos
) {
    if (pos >= tokens.size()) return std::nullopt;

    // Get candidate patterns based on first token
    std::string firstWord;
    if (tokens[pos].type == TokenType::IDENTIFIER) {
        firstWord = tokens[pos].lexeme;
    } else {
        // For keyword tokens, use their lexeme
        firstWord = tokens[pos].lexeme;
    }

    auto candidates = registry_.getCandidates(firstWord);

    MatchResult bestMatch;
    bool found = false;

    for (Pattern* pattern : candidates) {
        auto result = tryMatch(pattern, tokens, pos);
        if (result) {
            if (!found || result->specificity > bestMatch.specificity) {
                bestMatch = std::move(*result);
                found = true;
            }
        }
    }

    if (found) {
        return bestMatch;
    }
    return std::nullopt;
}

std::optional<MatchResult> PatternMatcher::matchExpression(
    const std::vector<Token>& tokens,
    size_t pos,
    int minPrecedence
) {
    // 1. Parse LHS (Primary or Prefix) using parsePrimary logic?
    // matchStatement typically matches "Statement Patterns" which start with literals (usually).
    // Expression patterns can start with params. e.g. "a + b".
    // Or they can be prefix: "not a".
    
    // We try to find a Prefix pattern or Primary first.
    // Try parsePrimary first.
    auto lhsResult = parsePrimary(tokens, pos);
    
    ExprPtr lhs;
    size_t currentPos;
    
    if (lhsResult) {
        lhs = std::move(lhsResult->first);
        currentPos = lhsResult->second;
    } else {
        // Try specific Prefix pattern matching?
        // Assume for now expressions start with Primary.
        // If we want "not a", "not" is identifier, matched by parsePrimary.
        // But then we need "tail" matching for "not a" where "not" is literal?
        // Actually "not a" pattern: literal "not", param "a".
        // This is a statement-like pattern.
        return matchStatement(tokens, pos);
    }

    // 2. Precedence Climbing Loop
    while (currentPos < tokens.size()) {
        const Token& opToken = tokens[currentPos];
        std::string opLexeme = opToken.lexeme;
        
        // Find best infix pattern starting with this token
        // Candidate must be: [Param, Literal(op), ...]
        // PatternRegistry indexes by first *literal*. 
        // For infix "$ + $", first literal is "+".
        // So candidates for "+" will include "$ + $".
        auto candidates = registry_.getCandidates(opLexeme);
        
        Pattern* bestPattern = nullptr;
        int bestPriority = -1;
        
        for (auto* pattern : candidates) {
            // Must start with Param
            if (pattern->elements.empty() || !pattern->elements[0].is_param) continue;
            
            // Check if second element matches current token
            if (pattern->elements.size() < 2 || pattern->elements[1].value != opLexeme) continue;
            
            // Get Priority
            int prio = 0;
            auto p = registry_.getPrecedenceRegistry().getPriority(
                PrecedenceRegistry::canonicalize(pattern->elements)
            );
            if (p) prio = *p;
            
            if (prio >= minPrecedence && prio > bestPriority) {
                bestPriority = prio;
                bestPattern = pattern;
            }
        }
        
        if (!bestPattern) break;
        
        // Start matching tail from element 1 (the operator)
        // matchTail(pattern, 1, tokens, currentPos)
        // Wait, matchTail starts matching elements[startIndex].
        // elements[1] is the operator literal.
        // We are at `opToken`. Does matchTail consume it?
        // Yes, matchTail logic checks literal against token.
        
        auto tailResult = matchTail(bestPattern, 1, tokens, currentPos);
        if (!tailResult) break;
        
        // Success! Bind LHS to the first parameter
        // The first parameter is elements[0].
        // We need to inject LHS into the bindings.
        std::string lhsParamName = bestPattern->elements[0].value;
        tailResult->bindings[lhsParamName] = std::move(lhs);
        
        // Update Result
        MatchResult result = std::move(*tailResult);
        
        // Wrap in PatternCall? 
        // matchTail assumes it returns a MatchResult.
        // But we are inside a climbing loop. 
        // We need to wrap this match into a "LHS expression" for the next iteration.
        // The AST node for this pattern match is PatternCall.
        
        auto call = std::make_unique<PatternCall>();
        call->pattern = bestPattern->definition;
        call->bindings = std::move(result.bindings);
        call->location = lhs->location; // or start token?
        
        lhs = std::move(call);
        currentPos += result.tokensConsumed;
        
        // Continue loop with new LHS
    }
    
    // We matched an expression!
    // But matchExpression is declared to return MatchResult (pattern match).
    // The parser expects ExprPtr?
    // Wait, `matchExpression` returns `optional<MatchResult>`.
    // Parser expects `ExprPtr`.
    // This method signature is for finding a *pattern*.
    // But we are constructing an AST of potentially nested calls `(a + b) * c`.
    // That AST is an ExprPtr, not a single MatchResult for one pattern.
    // The caller of matchExpression in Parser likely expects an Expr AST.
    
    // I need to change PatternMatcher::matchExpression return type?
    // Or add `parseExpression` to PatternMatcher that returns ExprPtr?
    // There is already `PatternMatcher::parseExpression` (line 390).
    // I should update THAT one.
    
    // Let's abort modifying matchExpression here and modify parseExpression instead.
    return std::nullopt; // placeholder
}

std::optional<MatchResult> PatternMatcher::tryMatch(
    Pattern* pattern,
    const std::vector<Token>& tokens,
    size_t pos
) {
    size_t currentPos = pos;
    std::unordered_map<std::string, ExprPtr> bindings;

    for (size_t elementIndex = 0; elementIndex < pattern->elements.size(); ++elementIndex) {
        const auto& elem = pattern->elements[elementIndex];

        if (currentPos >= tokens.size()) {
            // Check if remaining elements are all optional (literals or parameters)
            bool allOptional = true;
            for (size_t j = elementIndex; j < pattern->elements.size(); ++j) {
                const auto& remaining = pattern->elements[j];
                // Optional literals can be skipped
                if (remaining.is_optional) {
                    continue;
                }
                // Parameters without remaining input cannot be satisfied
                if (remaining.is_param) {
                    allOptional = false;
                    break;
                }
                // Required literals without remaining input
                allOptional = false;
                break;
            }
            if (!allOptional) return std::nullopt;
            break;
        }

        if (!elem.is_param) {
            // Literal word
            const Token& token = tokens[currentPos];

            // Special handling for "'s" which is tokenized as APOSTROPHE + IDENTIFIER "s"
            if (elem.value == "'s") {
                if (token.type == TokenType::APOSTROPHE &&
                    currentPos + 1 < tokens.size() &&
                    tokens[currentPos + 1].lexeme == "s") {
                    currentPos += 2;  // Consume both ' and s
                } else if (elem.is_optional) {
                    // Optional 's not present, skip
                } else {
                    return std::nullopt;
                }
            } else if (elem.is_optional) {
                // Optional literal - try to match, but skip if not present
                if (token.lexeme == elem.value) {
                    // Optional word is present, consume it
                    currentPos++;
                }
                // If not present, just continue to next element (skip the optional)
            } else {
                // Required literal - must match exactly
                if (token.lexeme != elem.value) {
                    return std::nullopt;
                }
                currentPos++;
            }
        } else {
            // Parameter - capture expression based on parameter type
            std::vector<PatternElement> remaining(
                pattern->elements.begin() + elementIndex + 1,
                pattern->elements.end()
            );

            if (elem.param_type == PatternParamType::Section) {
                // Section parameters capture an indented block
                // This is handled at the statement matching level, not here
                // Create a placeholder BlockExpr that will be filled in later
                auto blockExpr = std::make_unique<BlockExpr>();
                blockExpr->location = (currentPos < tokens.size()) ? tokens[currentPos].location : SourceLocation{};
                bindings[elem.value] = std::move(blockExpr);
                // Section parameters don't consume tokens here - block is captured separately
            }
            else if (elem.param_type == PatternParamType::Lazy) {
                // Lazy parameter - wrap in LazyExpr if not already
                auto paramResult = matchParam(elem, tokens, currentPos, remaining);
                if (!paramResult) {
                    return std::nullopt;
                }

                // Check if the captured expression is already a LazyExpr
                LazyExpr* existing = dynamic_cast<LazyExpr*>(paramResult->first.get());
                if (existing) {
                    bindings[elem.value] = std::move(paramResult->first);
                } else {
                    // Wrap in LazyExpr
                    auto lazy = std::make_unique<LazyExpr>();
                    lazy->location = paramResult->first->location;
                    lazy->inner = std::move(paramResult->first);
                    bindings[elem.value] = std::move(lazy);
                }
                currentPos = paramResult->second;
            }
            else {
                // Normal eager parameter
                auto paramResult = matchParam(elem, tokens, currentPos, remaining);
                if (!paramResult) {
                    return std::nullopt;
                }

                bindings[elem.value] = std::move(paramResult->first);
                currentPos = paramResult->second;
            }
        }
    }

    MatchResult result;
    result.pattern = pattern;
    result.bindings = std::move(bindings);
    result.tokensConsumed = currentPos - pos;
    result.specificity = pattern->specificity();

    return result;
}

std::optional<MatchResult> PatternMatcher::matchTail(
    Pattern* pattern,
    size_t startIndex,
    const std::vector<Token>& tokens,
    size_t pos
) {
    size_t currentPos = pos;
    std::unordered_map<std::string, ExprPtr> bindings;

    for (size_t elementIndex = startIndex; elementIndex < pattern->elements.size(); ++elementIndex) {
        const auto& elem = pattern->elements[elementIndex];

        if (currentPos >= tokens.size()) {
            // Check if remaining elements are optional
            bool allOptional = true;
            for (size_t j = elementIndex; j < pattern->elements.size(); ++j) {
                const auto& remaining = pattern->elements[j];
                if (remaining.is_optional && !remaining.is_param) {
                    continue;
                }
                allOptional = false;
                break;
            }
            if (!allOptional) return std::nullopt;
            break;
        }

        if (!elem.is_param) {
            // Literal matching (copy from tryMatch)
            const Token& token = tokens[currentPos];
            if (elem.value == "'s") {
                if (token.type == TokenType::APOSTROPHE &&
                    currentPos + 1 < tokens.size() &&
                    tokens[currentPos + 1].lexeme == "s") {
                    currentPos += 2;
                } else if (!elem.is_optional) {
                    return std::nullopt;
                }
            } else if (elem.is_optional) {
                if (token.lexeme == elem.value) currentPos++;
            } else {
                if (token.lexeme != elem.value) return std::nullopt;
                currentPos++;
            }
        } else {
            // Parameter matching (adapted from tryMatch)
            std::vector<PatternElement> remaining(
                pattern->elements.begin() + elementIndex + 1,
                pattern->elements.end()
            );

            // Match logic... reusing matchParam
            // We need to handle Section param here too if needed?
            // Assuming matchParam handles Lazy/Normal.
            
            if (elem.param_type == PatternParamType::Section) {
                 auto blockExpr = std::make_unique<BlockExpr>();
                 // Location logic..
                 bindings[elem.value] = std::move(blockExpr);
            } else {
                auto paramResult = matchParam(elem, tokens, currentPos, remaining);
                if (!paramResult) return std::nullopt;
                
                if (elem.param_type == PatternParamType::Lazy) {
                    // Wrap if needed... same logic
                    if (dynamic_cast<LazyExpr*>(paramResult->first.get())) {
                        bindings[elem.value] = std::move(paramResult->first);
                    } else {
                        auto lazy = std::make_unique<LazyExpr>();
                        lazy->inner = std::move(paramResult->first);
                        bindings[elem.value] = std::move(lazy);
                    }
                } else {
                    bindings[elem.value] = std::move(paramResult->first);
                }
                currentPos = paramResult->second;
            }
        }
    }

    MatchResult result;
    result.pattern = pattern;
    result.bindings = std::move(bindings);
    result.tokensConsumed = currentPos - pos;
    result.specificity = pattern->specificity();
    return result;
}

std::optional<std::pair<ExprPtr, size_t>> PatternMatcher::matchParam(
    const PatternElement& param,
    const std::vector<Token>& tokens,
    size_t pos,
    const std::vector<PatternElement>& remainingElements
) {
    if (pos >= tokens.size()) return std::nullopt;

    // Find the next literal word in remaining elements (if any)
    // Skip optional literals to find the next required stop word
    // But also track optional literals as potential stop words
    std::string nextRequiredLiteral;
    std::vector<std::string> optionalLiterals;

    for (const auto& elem : remainingElements) {
        if (!elem.is_param) {
            if (elem.is_optional) {
                optionalLiterals.push_back(elem.value);
            } else {
                nextRequiredLiteral = elem.value;
                break;
            }
        }
    }

    // Build a set of all stop words (both required and optional)
    std::unordered_set<std::string> stopWords;
    bool hasPossessiveStop = false;  // Special handling for "'s" stop word
    if (!nextRequiredLiteral.empty()) {
        stopWords.insert(nextRequiredLiteral);
        if (nextRequiredLiteral == "'s") hasPossessiveStop = true;
    }
    for (const auto& opt : optionalLiterals) {
        stopWords.insert(opt);
        if (opt == "'s") hasPossessiveStop = true;
    }

    // Helper to check if position is at a stop word (handles "'s" specially)
    auto isAtStopWord = [&](size_t checkPos) -> bool {
        if (checkPos >= tokens.size()) return false;
        // Check for "'s" as APOSTROPHE + "s"
        if (hasPossessiveStop &&
            tokens[checkPos].type == TokenType::APOSTROPHE &&
            checkPos + 1 < tokens.size() &&
            tokens[checkPos + 1].lexeme == "s") {
            return true;
        }
        return stopWords.count(tokens[checkPos].lexeme) > 0;
    };

    // Parse an expression, stopping at any stop word or end of line
    size_t endPos = pos;

    // Try to parse a full expression with operator precedence
    auto result = parseBinaryExpr(tokens, pos, 0);
    if (result) {
        endPos = result->second;

        // If there are stop words, make sure we stopped before or at one
        if (!stopWords.empty()) {
            // Check if the expression ended before a stop word
            if (isAtStopWord(endPos)) {
                return result;
            }

            // If we went past, try scanning for stop words
            size_t scanPos = pos;
            while (scanPos < tokens.size() &&
                   !isAtStopWord(scanPos) &&
                   tokens[scanPos].type != TokenType::NEWLINE &&
                   tokens[scanPos].type != TokenType::END_OF_FILE) {
                scanPos++;
            }

            if (scanPos > pos && (scanPos >= tokens.size() || isAtStopWord(scanPos))) {
                // Re-parse just the tokens before the stop word
                return parseBinaryExpr(tokens, pos, 0);
            }
        }

        return result;
    }

    // Fallback: try parsing just a primary expression
    return parsePrimary(tokens, pos);
}

std::optional<std::pair<ExprPtr, size_t>> PatternMatcher::parsePrimary(
    const std::vector<Token>& tokens,
    size_t pos
) {
    if (pos >= tokens.size()) return std::nullopt;

    const Token& token = tokens[pos];

    switch (token.type) {
        case TokenType::INTEGER: {
            auto lit = std::make_unique<IntegerLiteral>();
            lit->value = std::get<int64_t>(token.value);
            lit->location = token.location;
            return std::make_pair(std::move(lit), pos + 1);
        }
        case TokenType::FLOAT: {
            auto lit = std::make_unique<FloatLiteral>();
            lit->value = std::get<double>(token.value);
            lit->location = token.location;
            return std::make_pair(std::move(lit), pos + 1);
        }
        case TokenType::STRING: {
            auto lit = std::make_unique<StringLiteral>();
            lit->value = std::get<std::string>(token.value);
            lit->location = token.location;
            return std::make_pair(std::move(lit), pos + 1);
        }
        case TokenType::IDENTIFIER: {
            // All words are identifiers
            auto id = std::make_unique<Identifier>();
            id->name = token.lexeme;
            id->location = token.location;
            return std::make_pair(std::move(id), pos + 1);
        }
        case TokenType::LPAREN: {
            // Parenthesized expression
            auto inner = parseBinaryExpr(tokens, pos + 1, 0);
            if (inner && inner->second < tokens.size() &&
                tokens[inner->second].type == TokenType::RPAREN) {
                return std::make_pair(std::move(inner->first), inner->second + 1);
            }
            return std::nullopt;
        }
        case TokenType::AT: {
            // Intrinsic call: @intrinsic("name", arg1, arg2, ...)
            // 1. Check that next token is 'intrinsic'
            if (pos + 1 >= tokens.size() ||
                tokens[pos + 1].type != TokenType::IDENTIFIER ||
                tokens[pos + 1].lexeme != "intrinsic") {
                return std::nullopt;
            }

            // 2. Check for '('
            if (pos + 2 >= tokens.size() || tokens[pos + 2].type != TokenType::LPAREN) {
                return std::nullopt;
            }

            // 3. First argument must be the intrinsic name as a string literal
            if (pos + 3 >= tokens.size() || tokens[pos + 3].type != TokenType::STRING) {
                return std::nullopt;
            }

            auto call = std::make_unique<IntrinsicCall>();
            call->name = std::get<std::string>(tokens[pos + 3].value);
            call->location = tokens[pos].location;

            size_t currentPos = pos + 4;  // After @ intrinsic ( "name"

            // 4. Parse remaining arguments after the name
            while (currentPos < tokens.size() && tokens[currentPos].type == TokenType::COMMA) {
                currentPos++;  // Consume comma

                // Parse argument expression
                auto arg = parseBinaryExpr(tokens, currentPos, 0);
                if (!arg) return std::nullopt;

                call->args.push_back(std::move(arg->first));
                currentPos = arg->second;
            }

            // 5. Consume ')'
            if (currentPos >= tokens.size() || tokens[currentPos].type != TokenType::RPAREN) {
                return std::nullopt;
            }
            currentPos++;

            return std::make_pair(std::move(call), currentPos);
        }
        default:
            return std::nullopt;
    }
}

int PatternMatcher::getOperatorPrecedence(TokenType type) {
    // Operator precedence is no longer hardcoded.
    // All operators are defined as patterns in prelude.3bx with priority rules.
    // The PrecedenceRegistry handles the precedence graph.
    // This function returns -1 (unknown) for all types since precedence
    // comes from pattern definitions, not token types.
    (void)type;
    return -1;
}

bool PatternMatcher::isOperator(TokenType type) {
    // Operators are no longer hardcoded as special token types.
    // All operators are matched as patterns via the pattern registry.
    // This function is kept for backwards compatibility but always returns false.
    (void)type;
    return false;
}

bool PatternMatcher::isStopWord(const std::string& word, const std::vector<PatternElement>& remaining) {
    for (const auto& elem : remaining) {
        if (!elem.is_param && elem.value == word) {
            return true;
        }
    }
    return false;
}

std::optional<std::pair<ExprPtr, size_t>> PatternMatcher::parseExpression(
    const std::vector<Token>& tokens,
    size_t pos,
    const std::string& stopWord,
    int minPrecedence
) {
    if (pos >= tokens.size()) return std::nullopt;
    return parseBinaryExpr(tokens, pos, minPrecedence, stopWord);
}

// Note: Ensure parseBinaryExpr implementation matches the new signature
std::optional<std::pair<ExprPtr, size_t>> PatternMatcher::parseBinaryExpr(
    const std::vector<Token>& tokens,
    size_t pos,
    int minPrecedence,
    const std::string& stopWord
) {
    if (pos >= tokens.size()) return std::nullopt;

    ExprPtr lhs;
    size_t currentPos = pos;

    // Parse the LHS as a primary expression first
    // NOTE: We do NOT try to match prefix patterns here to avoid infinite recursion.
    // Prefix patterns should be matched at the statement level, not in expression parsing.
    auto prim = parsePrimary(tokens, pos);
    if (!prim) return std::nullopt;
    lhs = std::move(prim->first);
    currentPos = prim->second;

    // 3. Precedence Climbing
    while (currentPos < tokens.size()) {
        const Token& opToken = tokens[currentPos];

        // Stop checks
        if (!stopWord.empty() && opToken.lexeme == stopWord) break;
        if (opToken.type == TokenType::NEWLINE ||
            opToken.type == TokenType::COMMA ||
            opToken.type == TokenType::RPAREN) {
            break;
        }

        std::string opLexeme = opToken.lexeme;

        // Special handling for possessive patterns ('s)
        // When we see an apostrophe followed by 's', treat it as "'s" for matching
        bool isPossessive = false;
        if (opToken.type == TokenType::APOSTROPHE &&
            currentPos + 1 < tokens.size() &&
            tokens[currentPos + 1].lexeme == "s") {
            opLexeme = "'s";
            isPossessive = true;
        }

        // For infix patterns, the operator is typically in position 1 (after the LHS param)
        // We need to iterate ALL patterns and check if they match this operator
        const auto& allPatterns = registry_.allPatterns();

        Pattern* bestPattern = nullptr;
        int bestPriority = -1;

        for (auto* pattern : allPatterns) {
            // Must have at least 2 elements: LHS param + operator
            if (pattern->elements.size() < 2) continue;
            // First element must be a parameter (the LHS)
            if (!pattern->elements[0].is_param) continue;
            // Second element should be the operator literal
            const auto& opElem = pattern->elements[1];
            if (opElem.is_param || opElem.value != opLexeme) continue; 

            int prio = 0;
            auto p = registry_.getPrecedenceRegistry().getPriority(
                PrecedenceRegistry::canonicalize(pattern->elements)
            );
            if (p) prio = *p;
            
            // Use >= for left-associative operators (same priority allowed)
            if (prio >= minPrecedence) {
                if (prio > bestPriority) {
                    bestPriority = prio;
                    bestPattern = pattern;
                }
            }
        }
        
        if (!bestPattern) break;
        
        auto tailMatch = matchTail(bestPattern, 1, tokens, currentPos);
        if (!tailMatch) break;
        
        std::string lhsParam = bestPattern->elements[0].value;
        tailMatch->bindings[lhsParam] = std::move(lhs);
        
        auto call = std::make_unique<PatternCall>();
        call->pattern = bestPattern->definition;
        call->bindings = std::move(tailMatch->bindings);
        call->location = opToken.location;
        
        lhs = std::move(call);
        currentPos += tailMatch->tokensConsumed;
    }

    return std::make_pair(std::move(lhs), currentPos);
}

} // namespace tbx
