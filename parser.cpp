/**
 * parser.cpp — реализация рекурсивного нисходящего парсера.
 *
 * Схема «левого накопления» (left-folding) для левоассоциативных операторов:
 *   parseExprPrime(left) и parseTermPrime(left) принимают накопленный левый
 *   операнд и продолжают читать '+' или '*', пока они встречаются.
 *   Это позволяет правильно строить левоассоциативное дерево без левой рекурсии.
 *
 * Пример: a + b + c  →  BinaryOp(+, BinaryOp(+, a, b), c)
 */

#include "parser.h"

#include <cstdlib>   // std::stod
#include <sstream>

// -----------------------------------------------------------------------------
// Конструктор
// -----------------------------------------------------------------------------
Parser::Parser(Lexer& lexer) : lexer_(lexer) {}

// =============================================================================
// Публичный API
// =============================================================================

/// Разобрать весь ввод; убедиться, что после разбора — EOF.
ASTNodePtr Parser::parse() {
    ASTNodePtr root = parseAssign();

    // Проверяем, что весь ввод потреблён
    Token tok = lexer_.peekToken();
    if (tok.type != TokenType::TK_EOF) {
        std::ostringstream oss;
        oss << "Неожиданный токен " << tokenTypeName(tok.type)
            << " (\"" << tok.value << "\")"
            << " — строка " << tok.line << ", столбец " << tok.col
            << "; ожидался конец ввода";
        throw ParseError(oss.str(), tok.line, tok.col);
    }

    return root;
}

// =============================================================================
// Правила грамматики
// =============================================================================

// ASSIGN -> ID '=' EXPR
ASTNodePtr Parser::parseAssign() {
    Token id  = expect(TokenType::TK_ID);
    int   ln  = id.line;
    int   col = id.col;
    expect(TokenType::TK_ASSIGN);
    ASTNodePtr expr = parseExpr();
    return std::make_unique<AssignNode>(id.value, std::move(expr), ln, col);
}

// EXPR -> TERM EXPR'
ASTNodePtr Parser::parseExpr() {
    ASTNodePtr left = parseTerm();
    return parseExprPrime(std::move(left));
}

// EXPR' -> '+' TERM EXPR' | ε
// Аккумулирует левоассоциативную цепочку сложений.
ASTNodePtr Parser::parseExprPrime(ASTNodePtr left) {
    Token tok = lexer_.peekToken();
    if (tok.type == TokenType::TK_PLUS) {
        lexer_.nextToken();               // потребляем '+'
        int opLine = tok.line;
        int opCol  = tok.col;
        ASTNodePtr right = parseTerm();
        // Создаём узел BinaryOp и передаём его в рекурсивный вызов
        // (left-folding: новый узел становится следующим «левым»)
        auto node = std::make_unique<BinaryOpNode>(
            '+', std::move(left), std::move(right), opLine, opCol);
        return parseExprPrime(std::move(node));
    }
    // ε-продукция: возвращаем накопленное левое поддерево
    return left;
}

// TERM -> FACTOR TERM'
ASTNodePtr Parser::parseTerm() {
    ASTNodePtr left = parseFactor();
    return parseTermPrime(std::move(left));
}

// TERM' -> '*' FACTOR TERM' | ε
// Аккумулирует левоассоциативную цепочку умножений.
ASTNodePtr Parser::parseTermPrime(ASTNodePtr left) {
    Token tok = lexer_.peekToken();
    if (tok.type == TokenType::TK_STAR) {
        lexer_.nextToken();               // потребляем '*'
        int opLine = tok.line;
        int opCol  = tok.col;
        ASTNodePtr right = parseFactor();
        auto node = std::make_unique<BinaryOpNode>(
            '*', std::move(left), std::move(right), opLine, opCol);
        return parseTermPrime(std::move(node));
    }
    // ε-продукция
    return left;
}

// FACTOR -> '(' EXPR ')' | NUMBER | ID
ASTNodePtr Parser::parseFactor() {
    Token tok = lexer_.peekToken();

    // Вариант 1: скобочное выражение
    if (tok.type == TokenType::TK_LPAREN) {
        lexer_.nextToken();               // потребляем '('
        ASTNodePtr expr = parseExpr();
        expect(TokenType::TK_RPAREN);     // потребляем ')'
        return expr;
    }

    // Вариант 2: числовая константа
    if (tok.type == TokenType::TK_NUMBER) {
        lexer_.nextToken();
        double value = std::stod(tok.value);
        return std::make_unique<NumberNode>(tok.value, value, tok.line, tok.col);
    }

    // Вариант 3: идентификатор
    if (tok.type == TokenType::TK_ID) {
        lexer_.nextToken();
        return std::make_unique<IdentNode>(tok.value, tok.line, tok.col);
    }

    // Неожиданный токен — синтаксическая ошибка
    std::ostringstream oss;
    oss << "Ожидалось '(', число или идентификатор"
        << ", но встречен " << tokenTypeName(tok.type)
        << " (\"" << tok.value << "\")"
        << " — строка " << tok.line << ", столбец " << tok.col;
    throw ParseError(oss.str(), tok.line, tok.col);
}

// =============================================================================
// Вспомогательный метод: потребить токен заданного типа или выбросить ошибку
// =============================================================================
Token Parser::expect(TokenType type) {
    Token tok = lexer_.nextToken();
    if (tok.type != type) {
        std::ostringstream oss;
        oss << "Ожидался " << tokenTypeName(type)
            << ", но встречен " << tokenTypeName(tok.type)
            << " (\"" << tok.value << "\")"
            << " — строка " << tok.line << ", столбец " << tok.col;
        throw ParseError(oss.str(), tok.line, tok.col);
    }
    return tok;
}
