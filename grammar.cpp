/*
 * grammar.cpp — Grammar loading, FIRST/FOLLOW/directing-set computation,
 *               LL(1) verification, and parse table construction.
 *
 * All algorithms are explained in detail in the function-level comments.
 */

#include "grammar.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

// =============================================================================
// String utilities
// =============================================================================

std::string Grammar::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Split s by the literal delimiter delim (not a regex, not a char class)
std::vector<std::string> Grammar::splitBy(const std::string& s,
                                          const std::string& delim) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        parts.push_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    parts.push_back(s.substr(start));
    return parts;
}

// Split s by any run of whitespace
std::vector<std::string> Grammar::splitByWhitespace(const std::string& s) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        result.push_back(token);
    }
    return result;
}

// =============================================================================
// Grammar file parser
// =============================================================================

/*
 * parseRuleLine
 * -------------
 * Input: a single line like  "FIELDLIST -> FIELDDECL FIELDLIST | eps"
 *
 * Steps:
 *   1. Find "->", split into LHS and RHS-string.
 *   2. Split RHS-string on "|" to get individual alternatives.
 *   3. For each alternative:
 *        - If trimmed text == "eps" → Rule with empty rhs (ε production)
 *        - Otherwise split by whitespace → list of symbols in rhs
 *      Create a Rule object and push it to rules_.
 *   4. Register LHS as a nonterminal.
 */
void Grammar::parseRuleLine(const std::string& line) {
    const std::string arrow = "->";
    auto arrowPos = line.find(arrow);
    if (arrowPos == std::string::npos) return;  // no arrow → skip

    std::string lhs    = trim(line.substr(0, arrowPos));
    std::string rhsStr = trim(line.substr(arrowPos + arrow.size()));

    if (lhs.empty() || rhsStr.empty()) return;

    nonterminals_.insert(lhs);

    // Split alternatives
    auto alts = splitBy(rhsStr, "|");
    for (auto& alt : alts) {
        alt = trim(alt);
        Rule rule;
        rule.lhs = lhs;

        if (alt == "eps") {
            // Epsilon production: rhs stays empty
        } else {
            rule.rhs = splitByWhitespace(alt);
        }
        rules_.push_back(rule);
    }
}

bool Grammar::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    bool firstRule = true;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        // Skip comment lines and blank lines
        if (line.empty() || line[0] == '#') continue;

        parseRuleLine(line);

        // The start symbol is the LHS of the very first production rule
        if (firstRule && !rules_.empty()) {
            startSymbol_ = rules_.front().lhs;
            firstRule = false;
        }
    }

    if (rules_.empty()) return false;

    // --------------------------------------------------------------------------
    // Identify terminals: every RHS symbol that is NOT a nonterminal
    // and is NOT "eps" is a terminal.
    // --------------------------------------------------------------------------
    for (const auto& rule : rules_) {
        for (const auto& sym : rule.rhs) {
            if (nonterminals_.find(sym) == nonterminals_.end()) {
                terminals_.insert(sym);
            }
        }
    }
    // The end-of-input sentinel $ is always a terminal
    terminals_.insert("$");

    // --------------------------------------------------------------------------
    // Run the three computation passes
    // --------------------------------------------------------------------------
    computeFirstSets();
    computeFollowSets();
    ll1_ = buildParseTable();

    return true;
}

// =============================================================================
// FIRST set computation
// =============================================================================

/*
 * firstOfSequence([X1, X2, …, Xn])
 * ----------------------------------
 * Returns FIRST(X1 X2 … Xn).
 *
 * Algorithm:
 *   Iterate through the symbols X1, X2, … from left to right.
 *   At each Xi:
 *     (a) Add FIRST(Xi) \ {eps} to the result.
 *     (b) If eps ∉ FIRST(Xi) → stop: Xi cannot be empty, so nothing
 *         to the right of Xi is reachable as the "first" symbol.
 *     (c) If eps ∈ FIRST(Xi) → continue to Xi+1 (Xi can be empty).
 *   After the loop: if every Xi could derive eps, add eps to result.
 *
 * The empty sequence always has eps in its FIRST set.
 */
std::set<std::string> Grammar::firstOfSequence(
        const std::vector<std::string>& seq) const {

    std::set<std::string> result;

    if (seq.empty()) {
        result.insert("eps");
        return result;
    }

    for (const auto& sym : seq) {
        // Retrieve FIRST(sym).  If sym is unknown (shouldn't happen after
        // loading), treat it as a terminal whose FIRST is {sym}.
        std::set<std::string> firstSym;
        auto it = firstSets_.find(sym);
        if (it != firstSets_.end()) {
            firstSym = it->second;
        } else {
            firstSym.insert(sym);
        }

        // Add FIRST(sym) \ {eps}
        for (const auto& s : firstSym) {
            if (s != "eps") result.insert(s);
        }

        // If sym cannot derive eps, the first symbol of the whole sequence
        // must come from this position → stop.
        if (firstSym.find("eps") == firstSym.end()) {
            return result;
        }
        // Otherwise sym can be empty → continue to the next symbol.
    }

    // Every symbol in the sequence can derive eps → the whole sequence can too.
    result.insert("eps");
    return result;
}

/*
 * computeFirstSets()
 * ------------------
 * Iteratively compute FIRST(A) for every symbol (terminal & nonterminal).
 *
 * Initialisation:
 *   • FIRST(terminal) = {terminal}   (a terminal can only derive itself)
 *   • FIRST(nonterminal) = {}        (filled in by the iteration below)
 *
 * Iteration:
 *   For each rule  A → X1 X2 … Xn:
 *     Compute FIRST(X1 X2 … Xn) and add all its elements to FIRST(A).
 *   Repeat until no set changes (fixed point).
 *
 * Termination is guaranteed because we only add elements, never remove them,
 * and the alphabet is finite.
 */
void Grammar::computeFirstSets() {
    // Initialise terminals: FIRST(t) = {t}
    for (const auto& t : terminals_) {
        firstSets_[t].insert(t);
    }
    // Initialise nonterminals: FIRST(A) = {} (empty, filled by iteration)
    for (const auto& nt : nonterminals_) {
        firstSets_[nt];  // inserts an empty set if not already present
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& rule : rules_) {
            auto&  firstA   = firstSets_[rule.lhs];
            size_t oldSize  = firstA.size();

            // Compute FIRST(rhs) and merge into FIRST(lhs)
            auto firstRhs = firstOfSequence(rule.rhs);
            firstA.insert(firstRhs.begin(), firstRhs.end());

            if (firstA.size() != oldSize) changed = true;
        }
    }
}

// =============================================================================
// FOLLOW set computation
// =============================================================================

/*
 * computeFollowSets()
 * -------------------
 * Iteratively compute FOLLOW(A) for every nonterminal A.
 *
 * Initialisation:
 *   • FOLLOW(startSymbol) contains "$" (the end-of-input marker).
 *   • FOLLOW(A) = {} for all other nonterminals.
 *
 * Iteration:
 *   For each rule  A → α B β   (B is a nonterminal, α and β are sequences):
 *
 *   Rule 1: Add FIRST(β) \ {eps} to FOLLOW(B).
 *           (Any terminal that can start β can appear after B.)
 *
 *   Rule 2: If eps ∈ FIRST(β), add FOLLOW(A) to FOLLOW(B).
 *           (If β can be empty, whatever follows A can also follow B.)
 *
 * Repeat until no set changes (fixed point).
 *
 * Example for rule  STRUCTDECL → struct <identifier> { FIELDLIST } ;
 *   β after FIELDLIST = ["}",";"]
 *   FIRST(["}", ";"]) = {"}"}  (no eps)
 *   → Add "}" to FOLLOW(FIELDLIST).
 */
void Grammar::computeFollowSets() {
    // Initialise all FOLLOW sets to empty
    for (const auto& nt : nonterminals_) {
        followSets_[nt];  // empty set
    }
    // Start symbol: FOLLOW(S) ∋ $
    followSets_[startSymbol_].insert("$");

    bool changed = true;
    while (changed) {
        changed = false;

        for (const auto& rule : rules_) {
            const std::string& A = rule.lhs;

            for (size_t i = 0; i < rule.rhs.size(); ++i) {
                const std::string& B = rule.rhs[i];

                // Only nonterminals have FOLLOW sets
                if (!isNonterminal(B)) continue;

                // β = everything to the right of B in this rule
                std::vector<std::string> beta(rule.rhs.begin() + i + 1,
                                              rule.rhs.end());
                auto firstBeta = firstOfSequence(beta);

                // Rule 1: Add FIRST(β) \ {eps} to FOLLOW(B)
                for (const auto& s : firstBeta) {
                    if (s != "eps") {
                        if (followSets_[B].insert(s).second) changed = true;
                    }
                }

                // Rule 2: If eps ∈ FIRST(β), add FOLLOW(A) to FOLLOW(B)
                if (firstBeta.count("eps")) {
                    for (const auto& s : followSets_[A]) {
                        if (followSets_[B].insert(s).second) changed = true;
                    }
                }
            }
        }
    }
}

// =============================================================================
// Parse table construction  (and LL(1) check)
// =============================================================================

/*
 * buildParseTable()
 * -----------------
 * For each rule  (index i)  A → α:
 *
 *   Compute the DIRECTING (GUIDE) SET T(A → α):
 *
 *     T(A → α) = FIRST(α) \ {eps}
 *              ∪ FOLLOW(A)   if eps ∈ FIRST(α)
 *
 *   Explanation:
 *     • FIRST(α) \ {eps}: any terminal that can appear first in α tells the
 *       parser "use this rule when you see this terminal".
 *     • If α can derive the empty string (eps ∈ FIRST(α)), then the rule
 *       can also be used when the lookahead is in FOLLOW(A) — the parser
 *       will expand A to ε and let the parent rule consume the current token.
 *
 *   Fill the parse table:
 *     For each t ∈ T(A → α):
 *       If table[A][t] is already set → CONFLICT → grammar is NOT LL(1).
 *       Otherwise set table[A][t] = i.
 *
 * Returns true iff no conflict was found (grammar is LL(1)).
 */
bool Grammar::buildParseTable() {
    bool ok = true;

    for (size_t i = 0; i < rules_.size(); ++i) {
        const Rule& rule = rules_[i];

        // Compute FIRST(rhs)
        auto firstAlpha = firstOfSequence(rule.rhs);

        // Build directing set T(A → α)
        std::set<std::string> directing;

        // Part 1: FIRST(α) \ {eps}
        for (const auto& s : firstAlpha) {
            if (s != "eps") directing.insert(s);
        }

        // Part 2: If eps ∈ FIRST(α), include FOLLOW(A)
        if (firstAlpha.count("eps")) {
            auto it = followSets_.find(rule.lhs);
            if (it != followSets_.end()) {
                directing.insert(it->second.begin(), it->second.end());
            }
        }

        // Fill parse table entries
        for (const auto& t : directing) {
            auto& cell = parseTable_[rule.lhs][t];
            if (cell != 0 && cell != (int)i) {
                // Two rules map to the same (nonterminal, terminal) cell → conflict
                ok = false;
                // Keep the first entry so we can still build a (potentially
                // partial) table, but flag it.
            } else {
                // 0 is our "unset" sentinel; rule 0 will overwrite it too —
                // that's fine because we check for double-entry explicitly:
                if (parseTable_[rule.lhs].find(t) == parseTable_[rule.lhs].end()) {
                    parseTable_[rule.lhs][t] = (int)i;
                } else if (parseTable_[rule.lhs][t] != (int)i) {
                    ok = false;
                } else {
                    parseTable_[rule.lhs][t] = (int)i;
                }
            }
        }
    }

    // Fix: do a clean pass using a separate conflict-tracking structure
    // (The above logic has a subtle issue with rule index 0 vs "unset".)
    parseTable_.clear();
    std::map<std::string, std::map<std::string, int>> table;
    std::map<std::string, std::map<std::string, bool>> seen;
    ok = true;

    for (size_t i = 0; i < rules_.size(); ++i) {
        const Rule& rule = rules_[i];

        auto firstAlpha = firstOfSequence(rule.rhs);

        std::set<std::string> directing;
        for (const auto& s : firstAlpha) {
            if (s != "eps") directing.insert(s);
        }
        if (firstAlpha.count("eps")) {
            auto it = followSets_.find(rule.lhs);
            if (it != followSets_.end()) {
                directing.insert(it->second.begin(), it->second.end());
            }
        }

        for (const auto& t : directing) {
            if (seen[rule.lhs][t]) {
                ok = false;
            } else {
                seen[rule.lhs][t] = true;
                table[rule.lhs][t] = (int)i;
            }
        }
    }

    parseTable_ = std::move(table);
    return ok;
}
