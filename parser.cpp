/*
 * parser.cpp — реализация рекурсивного нисходящего парсера.
 *
 * Каждый метод parseXxx() соответствует одному правилу грамматики.
 * Парсер читает токены из лексера (один за раз) и строит дерево (AST).
 *
 * Чтобы разобрать  a + b + c  правильно (левоассоциативно), используем
 * приём «левого накопления» (left-folding):
 *   - parseExprPrime(left) принимает уже разобранный левый операнд
 *   - читает '+' и правый, строит узел BinaryOp
 *   - передаёт этот узел рекурсивно как новый «левый»
 *
 * Итоговое дерево для a + b + c:
 *   BinaryOp(+, BinaryOp(+, a, b), c)   — т.е. сначала (a+b), потом +(c)
 */

#include "parser.h"

#include <cstdlib>   // std::stod — преобразует строку в double
#include <sstream>   // ostringstream — для составления строк ошибок

// Конструктор: запоминаем ссылку на лексер
Parser::Parser(Lexer& lexer) : lexer_(lexer) {}

// =============================================================================
// Главный метод — разобрать всё выражение целиком
// =============================================================================

ASTNodePtr Parser::parse() {
    // Разбираем единственное выражение вида  ID = EXPR
    ASTNodePtr root = parseAssign();

    // После разбора должен идти конец ввода (EOF).
    // Если ещё что-то осталось — это лишнее, синтаксическая ошибка.
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
// Методы разбора по правилам грамматики
// =============================================================================

// Правило:  ASSIGN -> ID '=' EXPR
// Читаем имя переменной, потом '=', потом выражение справа.
ASTNodePtr Parser::parseAssign() {
    Token id  = expect(TokenType::TK_ID);     // обязательно должно быть имя
    int   ln  = id.line;
    int   col = id.col;
    expect(TokenType::TK_ASSIGN);             // потом обязательно '='
    ASTNodePtr expr = parseExpr();            // потом разбираем правую часть
    return std::make_unique<AssignNode>(id.value, std::move(expr), ln, col);
}

// Правило:  EXPR -> TERM EXPR'
// Выражение — это терм (умножения), за которым идут слагаемые
ASTNodePtr Parser::parseExpr() {
    ASTNodePtr left = parseTerm();            // разбираем первый терм
    return parseExprPrime(std::move(left));   // передаём его дальше для накопления
}

// Правило:  EXPR' -> '+' TERM EXPR'  |  ε
// Если видим '+' — читаем следующий терм и строим узел сложения.
// Потом снова смотрим: вдруг ещё один '+' идёт.
// Если '+' нет — просто возвращаем то что накопили (ε-продукция).
ASTNodePtr Parser::parseExprPrime(ASTNodePtr left) {
    Token tok = lexer_.peekToken();  // смотрим вперёд, не «съедая» токен
    if (tok.type == TokenType::TK_PLUS) {
        lexer_.nextToken();              // теперь «съедаем» '+'
        int opLine = tok.line;
        int opCol  = tok.col;
        ASTNodePtr right = parseTerm(); // разбираем правый операнд
        // Создаём узел BinaryOp(+, left, right) и он становится новым «левым»
        auto node = std::make_unique<BinaryOpNode>(
            '+', std::move(left), std::move(right), opLine, opCol);
        return parseExprPrime(std::move(node));  // рекурсия: вдруг ещё '+' есть
    }
    // '+' нет — возвращаем накопленное, ничего больше не читаем
    return left;
}

// Правило:  TERM -> FACTOR TERM'
// Терм — это факторы (атомы), соединённые умножением
ASTNodePtr Parser::parseTerm() {
    ASTNodePtr left = parseFactor();
    return parseTermPrime(std::move(left));
}

// Правило:  TERM' -> '*' FACTOR TERM'  |  ε
// Аналогично EXPR', но для умножения
ASTNodePtr Parser::parseTermPrime(ASTNodePtr left) {
    Token tok = lexer_.peekToken();
    if (tok.type == TokenType::TK_STAR) {
        lexer_.nextToken();               // «съедаем» '*'
        int opLine = tok.line;
        int opCol  = tok.col;
        ASTNodePtr right = parseFactor();
        auto node = std::make_unique<BinaryOpNode>(
            '*', std::move(left), std::move(right), opLine, opCol);
        return parseTermPrime(std::move(node));  // рекурсия: вдруг ещё '*' есть
    }
    return left;
}

// Правило:  FACTOR -> '(' EXPR ')' | NUMBER | ID
// Фактор — самый маленький атом: скобки с выражением, число или имя
ASTNodePtr Parser::parseFactor() {
    Token tok = lexer_.peekToken();  // смотрим что там

    // Вариант 1: скобочное выражение  ( EXPR )
    if (tok.type == TokenType::TK_LPAREN) {
        lexer_.nextToken();               // «съедаем» '('
        ASTNodePtr expr = parseExpr();   // рекурсивно разбираем то что внутри
        expect(TokenType::TK_RPAREN);    // требуем закрывающую ')' — иначе ошибка
        return expr;
    }

    // Вариант 2: число  (42, 3.14, 1e+18, ...)
    if (tok.type == TokenType::TK_NUMBER) {
        lexer_.nextToken();
        // std::stod — преобразует строку "3.14" в число 3.14
        double value = std::stod(tok.value);
        return std::make_unique<NumberNode>(tok.value, value, tok.line, tok.col);
    }

    // Вариант 3: имя переменной  (a, b, result, ...)
    if (tok.type == TokenType::TK_ID) {
        lexer_.nextToken();
        return std::make_unique<IdentNode>(tok.value, tok.line, tok.col);
    }

    // Ни один вариант не подошёл — синтаксическая ошибка
    std::ostringstream oss;
    oss << "Ожидалось '(', число или идентификатор"
        << ", но встречен " << tokenTypeName(tok.type)
        << " (\"" << tok.value << "\")"
        << " — строка " << tok.line << ", столбец " << tok.col;
    throw ParseError(oss.str(), tok.line, tok.col);
}

// =============================================================================
// Вспомогательный метод: «потребовать» токен нужного типа
// =============================================================================

// Читаем следующий токен. Если он нужного типа — хорошо, возвращаем его.
// Если нет — кидаем ParseError с объяснением что ожидалось и что пришло.
Token Parser::expect(TokenType type) {
    Token tok = lexer_.nextToken();  // читаем (и «съедаем») следующий токен
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
