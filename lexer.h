/*
 * lexer.h — Tokenizer (Lexical Analyzer) for C++ struct declarations.
 *
 * Responsibility:
 *   Scan raw source text character by character and produce a flat sequence
 *   of typed tokens.  Every token records its EXACT line and column so that
 *   error messages can point the user to the right place.
 *
 * Token taxonomy:
 *   ┌─────────────────┬──────────────────────────────────────────────────┐
 *   │ TokenType       │ What it represents                               │
 *   ├─────────────────┼──────────────────────────────────────────────────┤
 *   │ KW_STRUCT       │ keyword  "struct"                                │
 *   │ KW_INT          │ keyword  "int"                                   │
 *   │ KW_DOUBLE       │ keyword  "double"                                │
 *   │ KW_FLOAT        │ keyword  "float"                                 │
 *   │ KW_CHAR         │ keyword  "char"                                  │
 *   │ KW_BOOL         │ keyword  "bool"                                  │
 *   │ KW_STRING       │ keyword  "string"                                │
 *   │ IDENTIFIER      │ any other [A-Za-z_][A-Za-z0-9_]* word           │
 *   │                 │  → grammar terminal  <identifier>               │
 *   │ INTEGER         │ [0-9]+                                           │
 *   │                 │  → grammar terminal  <integer>                  │
 *   │ LBRACE          │ '{'                                              │
 *   │ RBRACE          │ '}'                                              │
 *   │ SEMICOLON       │ ';'                                              │
 *   │ LBRACKET        │ '['                                              │
 *   │ RBRACKET        │ ']'                                              │
 *   │ EOF_TOKEN       │ end of input                                     │
 *   └─────────────────┴──────────────────────────────────────────────────┘
 *
 * The method grammarSymbol() maps each TokenType to the string that is used
 * as a terminal inside grammar.txt and the parse table (e.g. "struct",
 * "<identifier>", "{", "$").  This bridges the gap between the lexer and the
 * grammar/parser layers.
 */

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
// Token types
// =============================================================================
enum class TokenType {
    // Keywords
    KW_STRUCT,
    KW_INT,
    KW_DOUBLE,
    KW_FLOAT,
    KW_CHAR,
    KW_BOOL,
    KW_STRING,

    // Generic tokens whose exact text matters
    IDENTIFIER,   // grammar terminal: <identifier>
    INTEGER,      // grammar terminal: <integer>

    // Single-character punctuation
    LBRACE,       // {
    RBRACE,       // }
    SEMICOLON,    // ;
    LBRACKET,     // [
    RBRACKET,     // ]

    // Sentinel
    EOF_TOKEN     // grammar terminal: $
};

// =============================================================================
// Token — one lexeme with its location
// =============================================================================
struct Token {
    TokenType   type;   // what kind of token
    std::string value;  // the raw text  (e.g. "Point", "42", "{")
    int         line;   // 1-based source line
    int         col;    // 1-based source column (start of lexeme)

    Token(TokenType t, std::string v, int l, int c)
        : type(t), value(std::move(v)), line(l), col(c) {}

    /*
     * grammarSymbol() — convert this token to the string key used in the
     * grammar and parse table.
     *
     *   KW_INT      → "int"
     *   IDENTIFIER  → "<identifier>"
     *   LBRACE      → "{"
     *   EOF_TOKEN   → "$"
     *   ...
     */
    std::string grammarSymbol() const;

    // Human-readable type name for error messages
    std::string typeName() const;
};

// =============================================================================
// LexerError — thrown when an unexpected character is encountered
// =============================================================================
class LexerError : public std::runtime_error {
public:
    int line, col;
    LexerError(const std::string& msg, int l, int c)
        : std::runtime_error(msg), line(l), col(c) {}
};

// =============================================================================
// Lexer — iterates over source text and produces tokens
// =============================================================================
class Lexer {
public:
    /*
     * Constructor.
     * @param source  Complete source text to tokenize (may be multi-line).
     */
    explicit Lexer(const std::string& source);

    /*
     * tokenize() — scan the entire source and return all tokens.
     * The last token is always EOF_TOKEN.
     * Throws LexerError on invalid characters.
     */
    std::vector<Token> tokenize();

private:
    std::string src_;   // source text
    size_t      pos_;   // current character index into src_
    int         line_;  // current line (1-based)
    int         col_;   // current column (1-based)

    // Peek at current character (or '\0' at end)
    char cur() const;

    // Peek at src_[pos_ + offset] (or '\0')
    char peek(size_t offset = 1) const;

    // Consume current character, update line/col counters, return it
    char advance();

    // Skip whitespace and C++ line comments (//)
    void skipWhitespace();

    // Read identifier or keyword starting at current position
    Token readIdentifierOrKeyword();

    // Read integer literal starting at current position
    Token readInteger();

    // True when pos_ >= src_.size()
    bool atEnd() const;
};
