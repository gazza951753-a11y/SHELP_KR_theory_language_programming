/*
 * grammar.h — Grammar representation, FIRST/FOLLOW/T sets, and LL(1) parse table.
 *
 * This module handles everything between reading grammar.txt and handing a
 * ready-to-use parse table to the parser.
 *
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │  Pipeline:                                                           │
 * │    grammar.txt                                                       │
 * │       │  loadFromFile()                                              │
 * │       ▼                                                              │
 * │    Rules + Nonterminals + Terminals                                  │
 * │       │  computeFirstSets()                                          │
 * │       ▼                                                              │
 * │    FIRST(A) for every symbol A                                       │
 * │       │  computeFollowSets()                                         │
 * │       ▼                                                              │
 * │    FOLLOW(A) for every nonterminal A                                 │
 * │       │  buildParseTable()                                           │
 * │       ▼                                                              │
 * │    T(A → α) directing sets  →  parse table[A][terminal] = rule idx  │
 * │       │  isLL1()                                                     │
 * │       ▼                                                              │
 * │    true / false                                                      │
 * └──────────────────────────────────────────────────────────────────────┘
 *
 * Grammar file format (grammar.txt)
 * -----------------------------------
 *  • One production group per line: "LHS -> alt1 | alt2 | ..."
 *  • Symbols separated by spaces
 *  • Epsilon written as "eps"
 *  • Special terminals:  <identifier>  <integer>
 *  • The first line's LHS is the start symbol
 *  • Lines starting with '#' are comments and are ignored
 */

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

// =============================================================================
// Rule — one production rule  A → X1 X2 … Xn
// =============================================================================
struct Rule {
    std::string              lhs; // left-hand side (always a nonterminal)
    std::vector<std::string> rhs; // right-hand side symbols; empty means ε
};

// =============================================================================
// Grammar — the full grammar with precomputed sets and parse table
// =============================================================================
class Grammar {
public:
    /*
     * loadFromFile(filename)
     * ----------------------
     * Parse the grammar description in filename.
     * Returns true on success.  On file-read failure returns false.
     * After a successful call all sets and the parse table are ready.
     */
    bool loadFromFile(const std::string& filename);

    // True iff the grammar is LL(1) (no conflicts in any directing set)
    bool isLL1() const { return ll1_; }

    // All production rules in order of appearance in the file
    const std::vector<Rule>& getRules() const { return rules_; }

    // The start symbol (LHS of the first rule)
    const std::string& getStartSymbol() const { return startSymbol_; }

    // Symbol classification
    bool isTerminal(const std::string& sym)    const { return terminals_.count(sym)    != 0; }
    bool isNonterminal(const std::string& sym) const { return nonterminals_.count(sym) != 0; }

    // LL(1) parse table: table[nonterminal][terminal] = index into getRules()
    const std::map<std::string, std::map<std::string, int>>& getParseTable() const {
        return parseTable_;
    }

    // FIRST/FOLLOW sets (useful for debugging / printing)
    const std::map<std::string, std::set<std::string>>& getFirstSets()  const { return firstSets_;  }
    const std::map<std::string, std::set<std::string>>& getFollowSets() const { return followSets_; }

private:
    // -------------------------------------------------------------------------
    // Grammar data
    // -------------------------------------------------------------------------
    std::vector<Rule>        rules_;
    std::string              startSymbol_;
    std::set<std::string>    nonterminals_;
    std::set<std::string>    terminals_;

    // Computed sets
    std::map<std::string, std::set<std::string>> firstSets_;
    std::map<std::string, std::set<std::string>> followSets_;

    // Parse table: [nonterminal][terminal] → rule index
    std::map<std::string, std::map<std::string, int>> parseTable_;

    bool ll1_ = false;

    // -------------------------------------------------------------------------
    // Internal computation steps
    // -------------------------------------------------------------------------

    /*
     * parseRuleLine(line)
     * -------------------
     * Parse one grammar file line such as
     *     TYPE -> int | double | float | <identifier>
     * Splits alternatives on '|', trims whitespace, creates Rule objects.
     * "eps" in an alternative maps to an empty rhs vector.
     */
    void parseRuleLine(const std::string& line);

    /*
     * computeFirstSets()
     * ------------------
     * Compute FIRST(A) for every grammar symbol A.
     *
     * FIRST(A) is the set of terminals that can appear as the first symbol
     * of any string derived from A (plus "eps" if A can derive the empty string).
     *
     * Algorithm (fixed-point iteration):
     *   1. Initialise FIRST(terminal) = {terminal} for every terminal.
     *      Initialise FIRST(nonterminal) = {} for every nonterminal.
     *   2. For each rule  A → X1 X2 … Xn:
     *        Add FIRST(X1 X2 … Xn) to FIRST(A).
     *   3. Repeat step 2 until no FIRST set changes.
     *
     * Helper: firstOfSequence([X1, …, Xn]) returns FIRST(X1 … Xn):
     *   - Add FIRST(Xi) \ {eps} to result.
     *   - If eps ∉ FIRST(Xi), stop.
     *   - If all Xi can derive eps, add eps to result.
     */
    void computeFirstSets();

    // Return FIRST(X1 X2 … Xn)  (does not modify firstSets_; reads it)
    std::set<std::string> firstOfSequence(const std::vector<std::string>& seq) const;

    /*
     * computeFollowSets()
     * -------------------
     * Compute FOLLOW(A) for every nonterminal A.
     *
     * FOLLOW(A) is the set of terminals that can appear IMMEDIATELY to the
     * right of A in any sentential form.
     *
     * Algorithm (fixed-point iteration):
     *   1. FOLLOW(S) = {$}  where S is the start symbol.
     *   2. For each rule  A → α B β:
     *        Add FIRST(β) \ {eps} to FOLLOW(B).
     *        If eps ∈ FIRST(β), add FOLLOW(A) to FOLLOW(B).
     *   3. Repeat step 2 until no FOLLOW set changes.
     */
    void computeFollowSets();

    /*
     * buildParseTable()
     * -----------------
     * Compute directing sets T(A → α) and fill the parse table.
     *
     * T(A → α) = FIRST(α) \ {eps}
     *          ∪ FOLLOW(A)   if eps ∈ FIRST(α)
     *
     * Parse table entry:  table[A][t] = rule index   for each t ∈ T(A → α).
     *
     * If two different rules for the same nonterminal produce the same
     * terminal in their directing sets, the grammar is NOT LL(1).
     *
     * Returns true iff the grammar is LL(1).
     */
    bool buildParseTable();

    // -------------------------------------------------------------------------
    // String utilities
    // -------------------------------------------------------------------------
    static std::vector<std::string> splitBy(const std::string& s,
                                            const std::string& delim);
    static std::vector<std::string> splitByWhitespace(const std::string& s);
    static std::string              trim(const std::string& s);
};
