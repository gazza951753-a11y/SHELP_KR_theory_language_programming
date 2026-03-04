/**
 * main.cpp -- entry point for the arithmetic expression parser (DPDA).
 *
 * Input:  "input.txt"  -- one line:  VARIABLE = EXPRESSION
 * Output: "output.txt" -- four sections:
 *   1. Parse tree (AST dump, indented)
 *   2. Symbol table (names, kinds, values, positions)
 *   3. Non-optimized intermediate code (3-address code)
 *   4. Optimized code (constant folding + copy elimination)
 *
 * On lexical or syntax error the message is written to output.txt and stderr.
 */

#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Print a numbered section header
static void printSection(std::ostream& os, int num, const std::string& title) {
    const std::string line(72, '=');
    os << "\n" << line << "\n";
    os << "  SECTION " << num << ": " << title << "\n";
    os << line << "\n\n";
}

int main() {
    // -------------------------------------------------------------------------
    // 1. Read input.txt
    // -------------------------------------------------------------------------
    std::ifstream inFile("input.txt");
    if (!inFile.is_open()) {
        std::cerr << "Error: cannot open input.txt\n";
        return 1;
    }

    std::string inputLine;
    if (!std::getline(inFile, inputLine)) {
        std::cerr << "Error: input.txt is empty\n";
        return 1;
    }
    inFile.close();

    // -------------------------------------------------------------------------
    // 2. Open output.txt
    // -------------------------------------------------------------------------
    std::ofstream outFile("output.txt");
    if (!outFile.is_open()) {
        std::cerr << "Error: cannot open output.txt for writing\n";
        return 1;
    }

    outFile << "========================================================================\n";
    outFile << "  Arithmetic Expression Parser  --  Recursive Descent (DPDA)\n";
    outFile << "  Theory of Programming Languages and Translation Methods\n";
    outFile << "========================================================================\n";
    outFile << "\n";
    outFile << "Input:\n";
    outFile << "  " << inputLine << "\n";

    // -------------------------------------------------------------------------
    // 3. Lex + parse
    // -------------------------------------------------------------------------
    ASTNodePtr ast;
    try {
        Lexer  lexer(inputLine);
        Parser parser(lexer);
        ast = parser.parse();
    }
    catch (const LexerError& e) {
        std::string msg = std::string("LEXER ERROR: ") + e.what();
        outFile << "\n" << msg << "\n";
        std::cerr << msg << "\n";
        return 1;
    }
    catch (const ParseError& e) {
        std::string msg = std::string("SYNTAX ERROR: ") + e.what();
        outFile << "\n" << msg << "\n";
        std::cerr << msg << "\n";
        return 1;
    }

    // -------------------------------------------------------------------------
    // 4. Code generation and optimization
    // -------------------------------------------------------------------------
    CodeGen cg;
    cg.generate(ast.get());

    // =========================================================================
    // SECTION 1: PARSE TREE
    // =========================================================================
    printSection(outFile, 1, "PARSE TREE (AST)");
    outFile << "Node types:\n";
    outFile << "  AssignNode [x =]      -- assignment to variable x\n";
    outFile << "  BinaryOpNode ['+','*']-- binary arithmetic operation\n";
    outFile << "  IdentNode  [name]     -- identifier (variable reference)\n";
    outFile << "  NumberNode [value]    -- numeric literal\n";
    outFile << "  Each indentation level = 2 spaces.\n\n";
    ast->print(outFile, 0);

    // =========================================================================
    // SECTION 2: SYMBOL TABLE
    // =========================================================================
    printSection(outFile, 2, "SYMBOL TABLE (Table of Names)");
    outFile << "Kind:\n";
    outFile << "  variable -- identifier (variable name)\n";
    outFile << "  integer  -- integer numeric constant\n";
    outFile << "  float    -- floating-point or scientific-notation constant\n\n";
    cg.printSymbolTable(outFile);

    // =========================================================================
    // SECTION 3: NON-OPTIMIZED INTERMEDIATE CODE (3AC)
    // =========================================================================
    printSection(outFile, 3, "NON-OPTIMIZED INTERMEDIATE CODE (3-Address Code)");
    outFile << "Each binary operation produces a new temporary variable ti.\n";
    outFile << "The assignment ends with an explicit copy instruction.\n\n";
    cg.printRawCode(outFile);

    // =========================================================================
    // SECTION 4: OPTIMIZED CODE
    // =========================================================================
    printSection(outFile, 4, "OPTIMIZED CODE (Constant Folding)");
    outFile << "Optimizations applied:\n";
    outFile << "  1. Constant folding: a sub-expression with two constant operands\n";
    outFile << "     is evaluated at compile time (e.g. 2 + 5 => 7).\n";
    outFile << "  2. Copy elimination: the last temporary is renamed to the target\n";
    outFile << "     variable, removing the redundant 'result = t_N' copy.\n";
    if (cg.wasFoldingApplied()) {
        outFile << "\n  [+] Constant folding was applied!\n\n";
    } else {
        outFile << "\n  [-] No constant folding applied\n";
        outFile << "      (no sub-expression consists entirely of constants).\n\n";
    }
    cg.printOptCode(outFile);

    // =========================================================================
    // Done
    // =========================================================================
    outFile << "\n";
    const std::string footer(72, '-');
    outFile << footer << "\n";
    outFile << "  Parsing completed successfully.\n";
    outFile << footer << "\n";

    outFile.close();

    std::cout << "Done! Result written to output.txt\n";
    return 0;
}
