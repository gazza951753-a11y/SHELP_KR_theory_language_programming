/*
 * parser.h — LL(1) table-driven parser for struct declarations.
 *
 * The parser uses the parse table built by the Grammar module and processes
 * the token stream produced by the Lexer.
 *
 * Algorithm overview (table-driven LL(1))
 * ----------------------------------------
 * We maintain an explicit stack of grammar symbols.
 * Initially:  stack = [ "$",  startSymbol ]  ($ on the bottom)
 *
 * At each step we look at:
 *   • top  = top of the parse stack
 *   • tok  = current lookahead token
 *
 * Case 1 — top is a TERMINAL:
 *   If top == tok.grammarSymbol()  →  match: pop top, advance input.
 *   Else  →  syntax error.
 *
 * Case 2 — top is a NONTERMINAL:
 *   Look up table[top][tok.grammarSymbol()]:
 *     Found  →  pop top, push rhs symbols in REVERSE order (so the
 *               leftmost symbol ends up on top of the stack).
 *     Not found  →  syntax error.
 *
 * Case 3 — top == "$" and tok == "$":
 *   Accept (successful parse).
 *
 * Case 4 — top == "$" but tok != "$":
 *   The input has extra tokens after a complete parse → syntax error.
 *
 * Result
 * ------
 * ParseResult holds either a success flag or a (line, col) error location.
 */

#pragma once

#include "grammar.h"
#include "lexer.h"

#include <string>
#include <vector>

// =============================================================================
// ParseResult — outcome of one parse attempt
// =============================================================================
struct ParseResult {
    bool        ok;     // true = syntactically correct
    int         line;   // error line   (meaningful only when ok == false)
    int         col;    // error column (meaningful only when ok == false)
    std::string msg;    // human-readable error description

    // Factory helpers
    static ParseResult success() {
        return {true, 0, 0, ""};
    }
    static ParseResult error(int l, int c, const std::string& m) {
        return {false, l, c, m};
    }
};

// =============================================================================
// Parser
// =============================================================================
class Parser {
public:
    /*
     * Constructor.
     * @param grammar  A fully loaded Grammar instance (must be LL(1)).
     *                 The caller is responsible for checking isLL1() first.
     */
    explicit Parser(const Grammar& grammar);

    /*
     * parse(tokens)
     * -------------
     * Run the LL(1) table-driven algorithm on the given token sequence.
     * @param tokens  Output of Lexer::tokenize(); must end with EOF_TOKEN.
     * @return ParseResult::success() on a valid input,
     *         ParseResult::error()   on the first syntax error.
     */
    ParseResult parse(const std::vector<Token>& tokens) const;

private:
    const Grammar& grammar_;
};
