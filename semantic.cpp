/*
 * semantic.cpp — Implementation of the name-conflict checker.
 *
 * The checker runs a finite state machine over the token stream.
 * Since the parser already guarantees syntactic correctness, we can
 * make firm assumptions about which tokens will appear in which order.
 *
 * State machine transitions are documented in semantic.h.
 */

#include "semantic.h"

// =============================================================================
// Helper
// =============================================================================

/*
 * isTypeToken
 * -----------
 * Returns true for any token that can appear as the TYPE of a field
 * declaration according to the grammar:
 *
 *   TYPE → int | double | float | char | bool | string | <identifier>
 *
 * All six keyword types plus bare identifiers (user-defined struct types
 * such as  Point, MyVec, …) qualify.
 */
bool SemanticChecker::isTypeToken(const Token& tok) {
    return tok.type == TokenType::KW_INT    ||
           tok.type == TokenType::KW_DOUBLE ||
           tok.type == TokenType::KW_FLOAT  ||
           tok.type == TokenType::KW_CHAR   ||
           tok.type == TokenType::KW_BOOL   ||
           tok.type == TokenType::KW_STRING ||
           tok.type == TokenType::IDENTIFIER;
}

// =============================================================================
// check()
// =============================================================================

/*
 * check(tokens)
 * -------------
 * Walk the token stream once, driven by the FSM described in semantic.h.
 *
 * Key data:
 *   currentFields — map from field name → (line, col) of its FIRST declaration
 *                   within the current struct.  Cleared at each new struct body.
 *
 * Duplicate detection:
 *   When we record a field name in AFTER_TYPE → FIELD_NAMED:
 *     If name already in currentFields → conflict! Report the CURRENT token's
 *     position (second declaration).
 *     Else → insert name with current token's position.
 *
 * We return on the FIRST conflict found (per the spec).
 */
SemanticResult SemanticChecker::check(const std::vector<Token>& tokens) {

    // -------------------------------------------------------------------------
    // FSM states
    // -------------------------------------------------------------------------
    enum class State {
        INITIAL,       // between struct declarations
        AFTER_STRUCT,  // just saw 'struct' keyword
        STRUCT_NAMED,  // saw struct name (identifier), expecting '{'
        IN_BODY,       // inside struct body, start of a new statement
        AFTER_TYPE,    // just consumed the type token, next is field name
        FIELD_NAMED,   // field name recorded, expecting ';' or '['
        IN_ARRAY,      // inside [N], expecting the integer
        ARRAY_INT,     // saw integer inside [...], expecting ']'
        AFTER_CLOSE,   // saw closing '}', expecting ';'
    };

    State state = State::INITIAL;

    // Field table for the current struct: name → (line, col) of first decl.
    std::map<std::string, std::pair<int,int>> currentFields;

    // -------------------------------------------------------------------------
    // Token walk
    // -------------------------------------------------------------------------
    for (const auto& tok : tokens) {

        if (tok.type == TokenType::EOF_TOKEN) break;

        switch (state) {

            // ----------------------------------------------------------------
            // INITIAL: looking for the start of a struct declaration
            // ----------------------------------------------------------------
            case State::INITIAL:
                if (tok.type == TokenType::KW_STRUCT) {
                    state = State::AFTER_STRUCT;
                }
                // Any other token between structs is ignored (should not
                // occur after successful parsing, but be defensive).
                break;

            // ----------------------------------------------------------------
            // AFTER_STRUCT: the token right after 'struct' must be the name
            // ----------------------------------------------------------------
            case State::AFTER_STRUCT:
                if (tok.type == TokenType::IDENTIFIER) {
                    // Begin a new struct scope: clear the field table
                    currentFields.clear();
                    state = State::STRUCT_NAMED;
                }
                break;

            // ----------------------------------------------------------------
            // STRUCT_NAMED: waiting for the opening brace '{'
            // ----------------------------------------------------------------
            case State::STRUCT_NAMED:
                if (tok.type == TokenType::LBRACE) {
                    state = State::IN_BODY;
                }
                break;

            // ----------------------------------------------------------------
            // IN_BODY: start of a statement inside the struct body.
            //   • A type token begins a field declaration.
            //   • '}' ends the struct body.
            // ----------------------------------------------------------------
            case State::IN_BODY:
                if (isTypeToken(tok)) {
                    // The type of the next field: transition to AFTER_TYPE.
                    // We don't record the type name itself — only the field
                    // name (next identifier) matters for semantic checking.
                    state = State::AFTER_TYPE;
                } else if (tok.type == TokenType::RBRACE) {
                    // End of the struct body
                    state = State::AFTER_CLOSE;
                }
                break;

            // ----------------------------------------------------------------
            // AFTER_TYPE: the next identifier is the field name.
            //   Check for duplicates HERE.
            // ----------------------------------------------------------------
            case State::AFTER_TYPE:
                if (tok.type == TokenType::IDENTIFIER) {
                    // Duplicate check
                    auto it = currentFields.find(tok.value);
                    if (it != currentFields.end()) {
                        // This name was already declared in this struct →
                        // report the SECOND occurrence (current token).
                        return SemanticResult::conflict(tok.value, tok.line, tok.col);
                    }
                    // First occurrence: record it
                    currentFields[tok.value] = {tok.line, tok.col};
                    state = State::FIELD_NAMED;
                }
                break;

            // ----------------------------------------------------------------
            // FIELD_NAMED: after the field name, expect ';' or '[' for array.
            // ----------------------------------------------------------------
            case State::FIELD_NAMED:
                if (tok.type == TokenType::SEMICOLON) {
                    // Simple field declaration ends
                    state = State::IN_BODY;
                } else if (tok.type == TokenType::LBRACKET) {
                    // Array declarator begins: expect integer next
                    state = State::IN_ARRAY;
                }
                break;

            // ----------------------------------------------------------------
            // IN_ARRAY: inside [...], expecting the array size integer.
            // ----------------------------------------------------------------
            case State::IN_ARRAY:
                if (tok.type == TokenType::INTEGER) {
                    state = State::ARRAY_INT;
                }
                break;

            // ----------------------------------------------------------------
            // ARRAY_INT: saw the array size, now expect the closing ']'.
            // ----------------------------------------------------------------
            case State::ARRAY_INT:
                if (tok.type == TokenType::RBRACKET) {
                    // Back to FIELD_NAMED: still expecting ';'
                    state = State::FIELD_NAMED;
                }
                break;

            // ----------------------------------------------------------------
            // AFTER_CLOSE: saw '}', now expect the trailing ';'.
            // ----------------------------------------------------------------
            case State::AFTER_CLOSE:
                if (tok.type == TokenType::SEMICOLON) {
                    state = State::INITIAL;
                }
                break;
        }
    }

    return SemanticResult::success();
}
