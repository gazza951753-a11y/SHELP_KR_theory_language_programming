/*
 * lexer.cpp — Implementation of the struct-declaration tokenizer.
 *
 * Key design points
 * -----------------
 *  • Line/column tracking: col_ is incremented for every normal character;
 *    on '\n' we bump line_ and reset col_ to 1.
 *  • Comments: C++ single-line comments (//) are skipped entirely.
 *  • Keywords: identifiers that match a reserved word are reclassified.
 *  • Error: any unrecognised character throws LexerError.
 */

#include "lexer.h"

#include <cctype>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Token helpers
// ---------------------------------------------------------------------------

std::string Token::grammarSymbol() const {
    // Maps each token type to the terminal string used in grammar.txt and the
    // LL(1) parse table.
    switch (type) {
        case TokenType::KW_STRUCT:    return "struct";
        case TokenType::KW_INT:       return "int";
        case TokenType::KW_DOUBLE:    return "double";
        case TokenType::KW_FLOAT:     return "float";
        case TokenType::KW_CHAR:      return "char";
        case TokenType::KW_BOOL:      return "bool";
        case TokenType::KW_STRING:    return "string";
        case TokenType::IDENTIFIER:   return "<identifier>";
        case TokenType::INTEGER:      return "<integer>";
        case TokenType::LBRACE:       return "{";
        case TokenType::RBRACE:       return "}";
        case TokenType::SEMICOLON:    return ";";
        case TokenType::LBRACKET:     return "[";
        case TokenType::RBRACKET:     return "]";
        case TokenType::EOF_TOKEN:    return "$";
    }
    return "?";  // unreachable
}

std::string Token::typeName() const {
    switch (type) {
        case TokenType::KW_STRUCT:    return "keyword 'struct'";
        case TokenType::KW_INT:       return "keyword 'int'";
        case TokenType::KW_DOUBLE:    return "keyword 'double'";
        case TokenType::KW_FLOAT:     return "keyword 'float'";
        case TokenType::KW_CHAR:      return "keyword 'char'";
        case TokenType::KW_BOOL:      return "keyword 'bool'";
        case TokenType::KW_STRING:    return "keyword 'string'";
        case TokenType::IDENTIFIER:   return "identifier";
        case TokenType::INTEGER:      return "integer";
        case TokenType::LBRACE:       return "'{'";
        case TokenType::RBRACE:       return "'}'";
        case TokenType::SEMICOLON:    return "';'";
        case TokenType::LBRACKET:     return "'['";
        case TokenType::RBRACKET:     return "']'";
        case TokenType::EOF_TOKEN:    return "end-of-file";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Lexer constructor
// ---------------------------------------------------------------------------

Lexer::Lexer(const std::string& source)
    : src_(source), pos_(0), line_(1), col_(1)
{}

// ---------------------------------------------------------------------------
// Character helpers
// ---------------------------------------------------------------------------

bool Lexer::atEnd() const {
    return pos_ >= src_.size();
}

char Lexer::cur() const {
    if (atEnd()) return '\0';
    return src_[pos_];
}

char Lexer::peek(size_t offset) const {
    size_t idx = pos_ + offset;
    if (idx >= src_.size()) return '\0';
    return src_[idx];
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') {
        // New line: reset column counter
        ++line_;
        col_ = 1;
    } else {
        ++col_;
    }
    return c;
}

// ---------------------------------------------------------------------------
// Whitespace / comment skipping
// ---------------------------------------------------------------------------

void Lexer::skipWhitespace() {
    while (!atEnd()) {
        char c = cur();

        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '/' && peek() == '/') {
            // C++ single-line comment: skip until end of line
            while (!atEnd() && cur() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Reading composite tokens
// ---------------------------------------------------------------------------

/*
 * readIdentifierOrKeyword
 * -----------------------
 * Precondition: cur() is a letter or underscore.
 * Reads [A-Za-z_][A-Za-z0-9_]* then checks whether the result is a
 * reserved keyword.  Returns an appropriately typed token.
 */
Token Lexer::readIdentifierOrKeyword() {
    // Table of keywords recognised by this grammar
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"struct", TokenType::KW_STRUCT},
        {"int",    TokenType::KW_INT},
        {"double", TokenType::KW_DOUBLE},
        {"float",  TokenType::KW_FLOAT},
        {"char",   TokenType::KW_CHAR},
        {"bool",   TokenType::KW_BOOL},
        {"string", TokenType::KW_STRING},
    };

    int startLine = line_;
    int startCol  = col_;
    std::string lexeme;

    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(cur())) || cur() == '_')) {
        lexeme += advance();
    }

    // Check for keyword
    auto it = keywords.find(lexeme);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;

    return Token(type, lexeme, startLine, startCol);
}

/*
 * readInteger
 * -----------
 * Precondition: cur() is a digit.
 * Reads one or more decimal digits.
 */
Token Lexer::readInteger() {
    int startLine = line_;
    int startCol  = col_;
    std::string lexeme;

    while (!atEnd() && std::isdigit(static_cast<unsigned char>(cur()))) {
        lexeme += advance();
    }

    return Token(TokenType::INTEGER, lexeme, startLine, startCol);
}

// ---------------------------------------------------------------------------
// Public interface: tokenize the entire source
// ---------------------------------------------------------------------------

/*
 * tokenize()
 * ----------
 * Scans all characters, produces tokens in order.
 * Always appends an EOF_TOKEN as the last element.
 *
 * Algorithm:
 *   1. Skip whitespace / comments.
 *   2. Dispatch on the first character.
 *   3. Repeat until end of source.
 *   4. Append EOF sentinel.
 */
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();

        if (atEnd()) break;

        char c    = cur();
        int  line = line_;
        int  col  = col_;

        // ---- Identifier / keyword ----
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // ---- Integer literal ----
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readInteger());
            continue;
        }

        // ---- Single-character punctuation ----
        advance();   // consume the character
        switch (c) {
            case '{':  tokens.emplace_back(TokenType::LBRACE,    "{", line, col); break;
            case '}':  tokens.emplace_back(TokenType::RBRACE,    "}", line, col); break;
            case ';':  tokens.emplace_back(TokenType::SEMICOLON, ";", line, col); break;
            case '[':  tokens.emplace_back(TokenType::LBRACKET,  "[", line, col); break;
            case ']':  tokens.emplace_back(TokenType::RBRACKET,  "]", line, col); break;
            default:
                throw LexerError(
                    std::string("Unexpected character '") + c + "'",
                    line, col);
        }
    }

    // Always terminate the token stream with an EOF sentinel
    tokens.emplace_back(TokenType::EOF_TOKEN, "$", line_, col_);
    return tokens;
}
