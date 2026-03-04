/*
 * semantic.h — Semantic analysis: field-name duplicate detection.
 *
 * After the LL(1) parser confirms that the token stream is syntactically
 * correct, the semantic checker makes a second pass over the same tokens
 * and checks the single semantic rule required by this assignment:
 *
 *   RULE: Within one struct, every field name must be unique.
 *         If the same name is declared twice, report the SECOND declaration.
 *
 * Approach: finite state machine over the token stream
 * -----------------------------------------------------
 * Because the syntax has already been validated, we can rely on the fact
 * that the token sequence is well-formed.  A small FSM is sufficient:
 *
 *   State         Meaning                      Next transition
 *   ─────────     ────────────────────────     ──────────────────────────────
 *   INITIAL       Between structs              see 'struct'  → AFTER_STRUCT
 *   AFTER_STRUCT  Just saw 'struct'            see identifier→ STRUCT_NAMED
 *   STRUCT_NAMED  Have struct name             see '{'       → IN_BODY
 *   IN_BODY       Inside struct, at stmt start see type tok  → AFTER_TYPE
 *                                              see '}'       → AFTER_CLOSE
 *   AFTER_TYPE    Just consumed type token     see identifier→ FIELD_NAMED
 *   FIELD_NAMED   Field name recorded          see ';'       → IN_BODY
 *                                              see '['       → IN_ARRAY
 *   IN_ARRAY      Inside array subscript [N]   see integer   → ARRAY_INT
 *   ARRAY_INT     Saw the integer              see ']'       → FIELD_NAMED
 *   AFTER_CLOSE   Saw closing '}'              see ';'       → INITIAL
 *
 * "type tok" means: KW_INT | KW_DOUBLE | KW_FLOAT | KW_CHAR |
 *                   KW_BOOL | KW_STRING | IDENTIFIER
 *
 * The field-name scope is cleared when entering a new struct body.
 * Duplicate detection happens in the AFTER_TYPE → FIELD_NAMED transition.
 */

#pragma once

#include "lexer.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// SemanticResult — outcome of one semantic-check pass
// =============================================================================
struct SemanticResult {
    bool        ok;    // true = no name conflicts found
    std::string name;  // conflicting name (when ok == false)
    int         line;  // location of the SECOND (duplicate) declaration
    int         col;

    static SemanticResult success() {
        return {true, "", 0, 0};
    }
    static SemanticResult conflict(const std::string& n, int l, int c) {
        return {false, n, l, c};
    }
};

// =============================================================================
// SemanticChecker
// =============================================================================
class SemanticChecker {
public:
    /*
     * check(tokens)
     * -------------
     * Walk through the already-validated token sequence and check for
     * duplicate field names within each struct.
     *
     * @param tokens  The same token vector that was passed to the parser.
     *                Must end with EOF_TOKEN.
     * @return SemanticResult::success() if no conflict is found,
     *         SemanticResult::conflict() on the first duplicate encountered.
     */
    SemanticResult check(const std::vector<Token>& tokens);

private:
    // Helper: true for any token that can serve as a field type
    static bool isTypeToken(const Token& tok);
};
