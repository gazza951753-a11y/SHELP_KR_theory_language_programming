/*
 * ast.h — здесь описаны все «кирпичики» дерева разбора (AST).
 *
 * АСД (абстрактное синтаксическое дерево) — это такая штука, которую
 * строит парсер, когда читает выражение типа  result = (a + 3) * b.
 * Вместо того чтобы хранить строку как есть, мы строим дерево, где
 * каждый узел — это либо число, либо переменная, либо операция.
 *
 * Наша грамматика (что вообще можно написать в input.txt):
 *   ASSIGN  -> ID '=' EXPR          // например:  x = ...
 *   EXPR    -> TERM EXPR'           // выражение — это термы, соединённые '+'
 *   EXPR'   -> '+' TERM EXPR' | ε  // ε значит «ничего» (можно закончить)
 *   TERM    -> FACTOR TERM'         // терм — это факторы, соединённые '*'
 *   TERM'   -> '*' FACTOR TERM' | ε
 *   FACTOR  -> '(' EXPR ')' | NUMBER | ID   // атом: скобки, число или имя
 *
 * Итого у нас 4 вида узлов в дереве:
 *   AssignNode   — узел присваивания (x = что-то)
 *   BinaryOpNode — узел операции (левое + правое  или  левое * правое)
 *   IdentNode    — узел-переменная (просто имя, типа  a  или  result)
 *   NumberNode   — узел-число (42, 3.14, 1e+18 — любая числовая константа)
 */

#pragma once

#include <memory>
#include <ostream>
#include <string>

// unique_ptr — умный указатель, сам удаляет память когда больше не нужна.
// Пишем псевдоним чтобы каждый раз не писать std::unique_ptr<ASTNode>.
struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

// =============================================================================
// Базовый класс — от него наследуются все виды узлов
// =============================================================================
struct ASTNode {
    int line = 0;  // номер строки в исходном тексте (считаем с 1)
    int col  = 0;  // номер столбца в исходном тексте (считаем с 1)

    // virtual деструктор нужен чтобы C++ правильно удалял объекты-наследники
    virtual ~ASTNode() = default;

    // Метод print() — рисует узел (и всех его детей) в файл.
    // indent — это сколько отступов поставить слева (каждый уровень = 2 пробела).
    // = 0 означает «по умолчанию indent равен нулю».
    virtual void print(std::ostream& os, int indent = 0) const = 0;
};

// =============================================================================
// NumberNode — узел для числа (42, 3.14, 1e+18 и т.д.)
// =============================================================================
struct NumberNode : ASTNode {
    std::string raw;    // как число написано в тексте, например "1e+18"
    double      value;  // его числовое значение, например 1000000000000000000.0

    NumberNode(const std::string& raw, double value, int line, int col)
        : raw(raw), value(value)
    { this->line = line; this->col = col; }

    void print(std::ostream& os, int indent = 0) const override;
};

// =============================================================================
// IdentNode — узел для имени переменной (a, b, result, ...)
// =============================================================================
struct IdentNode : ASTNode {
    std::string name;  // само имя переменной

    IdentNode(const std::string& name, int line, int col)
        : name(name)
    { this->line = line; this->col = col; }

    void print(std::ostream& os, int indent = 0) const override;
};

// =============================================================================
// BinaryOpNode — узел для операции '+' или '*'
// У него два ребёнка: левый и правый операнд
// =============================================================================
struct BinaryOpNode : ASTNode {
    char       op;     // сам оператор: '+' или '*'
    ASTNodePtr left;   // левый операнд (тоже узел дерева)
    ASTNodePtr right;  // правый операнд (тоже узел дерева)

    BinaryOpNode(char op, ASTNodePtr left, ASTNodePtr right, int line, int col)
        : op(op), left(std::move(left)), right(std::move(right))
    { this->line = line; this->col = col; }

    void print(std::ostream& os, int indent = 0) const override;
};

// =============================================================================
// AssignNode — узел присваивания, самый верхний в дереве
// Содержит имя переменной слева и выражение справа
// =============================================================================
struct AssignNode : ASTNode {
    std::string varName;  // переменная в левой части (то, куда пишем результат)
    ASTNodePtr  expr;     // выражение в правой части

    AssignNode(const std::string& varName, ASTNodePtr expr, int line, int col)
        : varName(varName), expr(std::move(expr))
    { this->line = line; this->col = col; }

    void print(std::ostream& os, int indent = 0) const override;
};

// =============================================================================
// Реализации print() — написаны прямо тут (inline), чтобы не создавать
// отдельный ast.cpp. Просто выводят имя узла и позицию, потом рекурсивно
// рисуют дочерние узлы с большим отступом.
// =============================================================================

inline void NumberNode::print(std::ostream& os, int indent) const {
    // std::string(N, ' ') — создаёт строку из N пробелов (для отступа)
    os << std::string(static_cast<size_t>(indent * 2), ' ')
       << "NumberNode [" << raw << "]"
       << "  (line=" << line << ", col=" << col << ")\n";
}

inline void IdentNode::print(std::ostream& os, int indent) const {
    os << std::string(static_cast<size_t>(indent * 2), ' ')
       << "IdentNode  [" << name << "]"
       << "  (line=" << line << ", col=" << col << ")\n";
}

inline void BinaryOpNode::print(std::ostream& os, int indent) const {
    // Сначала рисуем сам узел операции...
    os << std::string(static_cast<size_t>(indent * 2), ' ')
       << "BinaryOpNode ['" << op << "']"
       << "  (line=" << line << ", col=" << col << ")\n";
    // ...потом с отступом +1 рисуем левого и правого ребёнка
    left->print(os, indent + 1);
    right->print(os, indent + 1);
}

inline void AssignNode::print(std::ostream& os, int indent) const {
    // Рисуем узел присваивания, потом дерево выражения справа от '='
    os << std::string(static_cast<size_t>(indent * 2), ' ')
       << "AssignNode [" << varName << " =]"
       << "  (line=" << line << ", col=" << col << ")\n";
    expr->print(os, indent + 1);
}
