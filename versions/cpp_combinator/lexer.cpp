#include "tokens.h"
#include <vector>
#include <optional>
#include <cctype>
#include <iostream>
#include <unordered_map>
#include <string_view>

/**
 * CloudPol Lexer - Native Code-First / Combinator Approach
 * 
 * Instead of generating code from a grammar file, this approach builds
 * the flexer directly in C++ using small, testable, and composable functions.
 * 
 * Benefits:
 * - No separate transpilation or "build step" for the parser/lexer.
 * - Full native IDE support (autocomplete, refactoring, go-to-definition).
 * - Type-safety straight out of the box in the host language.
 * - Easy to unit-test specific token extractors in isolation.
 */
class CloudPolLexer {
private:
    std::string_view source;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    // --- Core Navigation Combinators ---

    void advance(size_t n = 1) {
        for (size_t i = 0; i < n && pos < source.length(); ++i) {
            if (source[pos] == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
            pos++;
        }
    }

    char peek() const { return pos < source.length() ? source[pos] : '\0'; }
    char peekNext() const { return pos + 1 < source.length() ? source[pos + 1] : '\0'; }

    bool match(std::string_view expected) {
        if (source.substr(pos, expected.length()) == expected) {
            advance(expected.length());
            return true;
        }
        return false;
    }

    void skipWhitespaceAndComments() {
        while (pos < source.length()) {
            char c = peek();
            if (std::isspace(c)) {
                advance();
            } else if (c == '/' && peekNext() == '/') {
                while (pos < source.length() && peek() != '\n') advance();
            } else if (c == '/' && peekNext() == '*') {
                advance(2); // Skip /*
                while (pos < source.length() && !(peek() == '*' && peekNext() == '/')) advance();
                advance(2); // Skip */
            } else {
                break;
            }
        }
    }

    // --- Tokenizer Combinators ---

    std::optional<Token> lexString() {
        if (peek() != '"') return std::nullopt;
        int startLine = line, startCol = col;
        advance(); // skip quote
        size_t startPos = pos;
        
        while (pos < source.length() && peek() != '"' && peek() != '\n') advance();
        
        if (peek() == '"') {
            std::string_view val = source.substr(startPos, pos - startPos);
            advance(); // skip closing quote
            return Token{TokenCode::STRING, std::string(val), startLine, startCol};
        }
        return std::nullopt; // Unterminated string
    }

    std::optional<Token> lexNumber() {
        if (!std::isdigit(peek())) return std::nullopt;
        int startLine = line, startCol = col;
        size_t startPos = pos;
        
        while (std::isdigit(peek())) advance();
        if (peek() == '.') {
            advance();
            while (std::isdigit(peek())) advance();
        }
        return Token{TokenCode::NUMBER, std::string(source.substr(startPos, pos - startPos)), startLine, startCol};
    }

    std::optional<Token> lexIdentifierOrKeyword() {
        if (!std::isalpha(peek()) && peek() != '_') return std::nullopt;
        int startLine = line, startCol = col;
        size_t startPos = pos;
        
        while (std::isalnum(peek()) || peek() == '_') advance();
        std::string val = std::string(source.substr(startPos, pos - startPos));
        
        // Dictionary mapper for tokens (pure C++ data structures rather than Flex rules)
        static const std::unordered_map<std::string, TokenCode> keywords = {
            {"ALLOW", TokenCode::KW_ALLOW}, {"DENY", TokenCode::KW_DENY},
            {"ROLE", TokenCode::KW_ROLE}, {"ACTION", TokenCode::KW_ACTION},
            {"ON", TokenCode::KW_ON}, {"RESOURCE", TokenCode::KW_RESOURCE},
            {"WHERE", TokenCode::KW_WHERE}, {"AND", TokenCode::KW_AND},
            {"OR", TokenCode::KW_OR}, {"NOT", TokenCode::KW_NOT},
            {"TRUE", TokenCode::KW_TRUE}, {"FALSE", TokenCode::KW_FALSE},
            {"ip", TokenCode::ATTR_IP}, {"time", TokenCode::ATTR_TIME},
            {"mfa", TokenCode::ATTR_MFA}, {"region", TokenCode::ATTR_REGION},
            {"tag", TokenCode::ATTR_TAG}
        };

        auto it = keywords.find(val);
        if (it != keywords.end()) return Token{it->second, val, startLine, startCol};
        
        return Token{TokenCode::UNKNOWN, val, startLine, startCol};
    }

public:
    CloudPolLexer(std::string_view src) : source(src) {}

    // Master tokenizer leveraging the smaller combinators
    Token nextToken() {
        skipWhitespaceAndComments();
        if (pos >= source.length()) return Token{TokenCode::END_OF_FILE, "", line, col};

        int startLine = line, startCol = col;

        if (auto strTok = lexString()) return *strTok;
        if (auto numTok = lexNumber()) return *numTok;
        if (auto idTok = lexIdentifierOrKeyword()) return *idTok;

        // Operators & Punctuation
        if (match("==")) return Token{TokenCode::OP_EQ, "==", startLine, startCol};
        if (match("!=")) return Token{TokenCode::OP_NEQ, "!=", startLine, startCol};
        if (match("<=")) return Token{TokenCode::OP_LEQ, "<=", startLine, startCol};
        if (match(">=")) return Token{TokenCode::OP_GEQ, ">=", startLine, startCol};
        if (match("<"))  return Token{TokenCode::OP_LT, "<", startLine, startCol};
        if (match(">"))  return Token{TokenCode::OP_GT, ">", startLine, startCol};
        if (match(";"))  return Token{TokenCode::SEMICOLON, ";", startLine, startCol};

        // Unknown character fallback
        char u = peek();
        advance();
        return Token{TokenCode::UNKNOWN, std::string(1, u), startLine, startCol};
    }
    
    std::vector<Token> lexAll() {
        std::vector<Token> tokens;
        Token t;
        do {
            t = nextToken();
            tokens.push_back(t);
        } while (t.code != TokenCode::END_OF_FILE);
        return tokens;
    }
};

/*
// Usage Example:
//
// std::string code = "ALLOW ROLE \"Admin\" ACTION \"*\" ON RESOURCE \"*\";";
// CloudPolLexer lexer(code);
// auto tokens = lexer.lexAll();
*/
