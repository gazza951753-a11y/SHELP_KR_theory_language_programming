/*
 * main.cpp — Entry point for the LL(1) struct syntax analyzer.
 *
 * Processing pipeline
 * -------------------
 *
 *  ┌─────────────┐     ┌──────────────┐     ┌────────────┐     ┌──────────────┐
 *  │ grammar.txt │────►│   Grammar    │────►│   Parser   │     │  Semantic    │
 *  │             │     │  (LL1 check, │     │ (LL1 table │     │  Checker     │
 *  │  input.txt  │────►│ parse table) │     │  driven)   │────►│ (dup names)  │
 *  └─────────────┘     └──────────────┘     └────────────┘     └──────┬───────┘
 *                                                                      │
 *                                                               output.txt
 *
 * Output rules (checked in order):
 *   1. "Grammar is not LL(1)"
 *        — if the grammar has conflicts in any directing set.
 *   2. "Syntax error at line L, position P"
 *        — first syntax error found during LL(1) parsing.
 *   3. "Name conflict: '<name>' redeclared at line L, position P"
 *        — first duplicate field name within any struct.
 *   4. "OK"
 *        — everything is syntactically and semantically correct.
 */

#include "grammar.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    // =========================================================================
    // Step 1: Load and validate the grammar
    // =========================================================================
    Grammar grammar;
    if (!grammar.loadFromFile("grammar.txt")) {
        std::cerr << "Error: cannot load grammar.txt\n";
        return 1;
    }

    // Open output file early so we can write the grammar-error message too
    std::ofstream out("output.txt");
    if (!out.is_open()) {
        std::cerr << "Error: cannot open output.txt for writing\n";
        return 1;
    }

    // Check LL(1) property — must be done before constructing the parser
    if (!grammar.isLL1()) {
        out << "Grammar is not LL(1)\n";
        return 0;
    }

    // =========================================================================
    // Step 2: Read and tokenize input.txt
    // =========================================================================
    std::ifstream inputFile("input.txt");
    if (!inputFile.is_open()) {
        std::cerr << "Error: cannot open input.txt\n";
        return 1;
    }
    std::ostringstream ss;
    ss << inputFile.rdbuf();
    std::string source = ss.str();

    std::vector<Token> tokens;
    try {
        Lexer lexer(source);
        tokens = lexer.tokenize();
    } catch (const LexerError& e) {
        // A lexical error is reported as a syntax error
        out << "Syntax error at line " << e.line
            << ", position " << e.col << "\n";
        return 0;
    }

    // =========================================================================
    // Step 3: LL(1) syntax analysis
    // =========================================================================
    Parser     parser(grammar);
    ParseResult pr = parser.parse(tokens);

    if (!pr.ok) {
        out << "Syntax error at line " << pr.line
            << ", position " << pr.col << "\n";
        return 0;
    }

    // =========================================================================
    // Step 4: Semantic analysis — duplicate field names
    // =========================================================================
    SemanticChecker checker;
    SemanticResult  sr = checker.check(tokens);

    if (!sr.ok) {
        out << "Name conflict: '" << sr.name
            << "' redeclared at line " << sr.line
            << ", position " << sr.col << "\n";
        return 0;
    }

    // =========================================================================
    // Step 5: All checks passed
    // =========================================================================
    out << "OK\n";
    return 0;
}
