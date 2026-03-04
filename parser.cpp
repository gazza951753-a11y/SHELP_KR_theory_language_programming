/*
 * parser.cpp — LL(1) table-driven parser implementation.
 *
 * The core idea is simple: we simulate a pushdown automaton by managing an
 * explicit stack of grammar symbols and consulting the pre-built parse table
 * to decide which rule to apply at each step.
 *
 * Detailed walkthrough of the algorithm
 * ======================================
 *
 *  Initialise:
 *    stack  ← [ "$", startSymbol ]        ($ is the bottom sentinel)
 *    pos    ← 0                            (index into tokens[])
 *
 *  Loop (while stack is not empty):
 *
 *    top  ← stack.top()
 *    tok  ← tokens[pos]                   (current lookahead)
 *    sym  ← tok.grammarSymbol()           (grammar terminal string)
 *
 *    ┌──────────────────────────────────────────────────────────────────┐
 *    │ A. top == "$"                                                    │
 *    │    ├─ sym == "$"  →  ACCEPT (return success)                    │
 *    │    └─ sym != "$"  →  ERROR  (extra input)                       │
 *    │                                                                  │
 *    │ B. top is TERMINAL                                               │
 *    │    ├─ top == sym  →  MATCH: pop top, advance pos                │
 *    │    └─ top != sym  →  ERROR (expected top, got sym)              │
 *    │                                                                  │
 *    │ C. top is NONTERMINAL                                            │
 *    │    table = parseTable[top]                                       │
 *    │    ├─ table has entry for sym:                                   │
 *    │    │    rule ← rules[table[sym]]                                 │
 *    │    │    pop top                                                  │
 *    │    │    push rule.rhs in REVERSE order                           │
 *    │    │      (leftmost symbol of rhs ends up on top of stack)       │
 *    │    │    If rhs is empty (ε rule) → nothing is pushed            │
 *    │    └─ no entry for sym  →  ERROR (unexpected token)             │
 *    └──────────────────────────────────────────────────────────────────┘
 *
 *  The loop terminates because:
 *    • Every MATCH advances pos by 1 (strictly progresses through input).
 *    • Every rule expansion reduces the nonterminal count on the stack
 *      (there are no infinitely-growing grammars in LL(1) grammars without
 *       ε cycles, and our grammar is ε-free or has acyclic ε derivations).
 *
 *  Error reporting:
 *    On any error path we record the line and column of the current
 *    lookahead token — that is the position in the source that caused the
 *    failure.
 */

#include "parser.h"

#include <sstream>
#include <stack>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Parser::Parser(const Grammar& grammar)
    : grammar_(grammar)
{}

// ---------------------------------------------------------------------------
// parse()
// ---------------------------------------------------------------------------

ParseResult Parser::parse(const std::vector<Token>& tokens) const {
    // Safety: tokens must contain at least the EOF sentinel.
    if (tokens.empty()) {
        return ParseResult::error(1, 1, "Empty token stream");
    }

    const auto& table = grammar_.getParseTable();
    const auto& rules = grammar_.getRules();

    // -------------------------------------------------------------------------
    // Initialise the parse stack.
    // We push "$" first (bottom sentinel), then the start symbol on top.
    // -------------------------------------------------------------------------
    std::stack<std::string> stk;
    stk.push("$");
    stk.push(grammar_.getStartSymbol());

    size_t pos = 0;  // current position in the token vector

    // Accessor for the current lookahead token
    auto currentToken = [&]() -> const Token& {
        return tokens[pos];
    };

    // -------------------------------------------------------------------------
    // Main parsing loop
    // -------------------------------------------------------------------------
    while (!stk.empty()) {
        const std::string& top = stk.top();
        const Token&       tok = currentToken();
        const std::string& sym = tok.grammarSymbol();

        // ------------------------------------------------------------------
        // Case A: bottom-of-stack sentinel
        // ------------------------------------------------------------------
        if (top == "$") {
            if (sym == "$") {
                // Both the stack and the input are exhausted → success!
                return ParseResult::success();
            } else {
                // Stack is done but input still has tokens → extra tokens
                std::ostringstream msg;
                msg << "Unexpected token '" << tok.value
                    << "' (" << tok.typeName() << ") after end of input";
                return ParseResult::error(tok.line, tok.col, msg.str());
            }
        }

        // ------------------------------------------------------------------
        // Case B: top of stack is a TERMINAL
        // ------------------------------------------------------------------
        if (grammar_.isTerminal(top)) {
            if (top == sym) {
                // The expected terminal matches the current token → consume both
                stk.pop();
                ++pos;
            } else {
                // Mismatch between expected terminal and actual token
                std::ostringstream msg;
                msg << "Expected '" << top
                    << "' but got '" << tok.value
                    << "' (" << tok.typeName() << ")";
                return ParseResult::error(tok.line, tok.col, msg.str());
            }
            continue;
        }

        // ------------------------------------------------------------------
        // Case C: top of stack is a NONTERMINAL
        // ------------------------------------------------------------------

        // Look up the parse table entry for (top, sym)
        auto ntIt = table.find(top);
        if (ntIt == table.end()) {
            // No entries at all for this nonterminal (shouldn't happen with
            // a valid grammar but guard against it).
            std::ostringstream msg;
            msg << "Internal error: no parse table entry for nonterminal '"
                << top << "'";
            return ParseResult::error(tok.line, tok.col, msg.str());
        }

        auto termIt = ntIt->second.find(sym);
        if (termIt == ntIt->second.end()) {
            // No rule for this (nonterminal, terminal) pair → syntax error.
            //
            // Build a helpful error message listing what was expected.
            std::ostringstream msg;
            msg << "Unexpected token '" << tok.value
                << "' (" << tok.typeName() << ")";

            // Optionally list the expected terminals from the table row
            if (!ntIt->second.empty()) {
                msg << "; expected one of: ";
                bool first = true;
                for (const auto& kv : ntIt->second) {
                    if (!first) msg << ", ";
                    msg << "'" << kv.first << "'";
                    first = false;
                }
            }

            return ParseResult::error(tok.line, tok.col, msg.str());
        }

        // Found a matching rule
        int        ruleIdx = termIt->second;
        const Rule& rule   = rules[ruleIdx];

        // Pop the nonterminal from the stack
        stk.pop();

        // Push the RHS symbols in REVERSE order so that the leftmost symbol
        // ends up on top of the stack (to be processed first).
        //
        // Example: rule  STRUCTDECL → struct <identifier> { FIELDLIST } ;
        //   Push in order:  ;  }  FIELDLIST  {  <identifier>  struct
        //   After push, top of stack = "struct"  ✓
        for (int j = static_cast<int>(rule.rhs.size()) - 1; j >= 0; --j) {
            stk.push(rule.rhs[j]);
        }
        // If rule.rhs is empty (ε production) → nothing is pushed, effectively
        // erasing the nonterminal from the stack.
    }

    // Stack exhausted — we should have returned success inside the loop,
    // but handle the edge case where the EOF token might not have been
    // consumed yet.
    if (currentToken().grammarSymbol() == "$") {
        return ParseResult::success();
    }

    return ParseResult::error(
        currentToken().line, currentToken().col,
        "Unexpected tokens remaining after parse");
}
