/**
 * ast.h — определения узлов абстрактного синтаксического дерева (АСД / AST).
 *
 * Грамматика, которую реализует парсер:
 *   ASSIGN  -> ID '=' EXPR
 *   EXPR    -> TERM EXPR'
 *   EXPR'   -> '+' TERM EXPR'  |  ε
 *   TERM    -> FACTOR TERM'
 *   TERM'   -> '*' FACTOR TERM'  |  ε
 *   FACTOR  -> '(' EXPR ')'  |  NUMBER  |  ID
 *
 * Типы узлов:
 *   AssignNode    — оператор присваивания  (VARIABLE = EXPRESSION)
 *   BinaryOpNode  — бинарная операция       (left OP right, OP ∈ {'+','*'})
 *   IdentNode     — идентификатор (переменная)
 *   NumberNode    — числовая константа (целая, вещественная, научная нотация)
 */

#pragma once

#include <memory>
#include <ostream>
#include <string>

// Удобный псевдоним для умного указателя на узел АСД
struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

// =============================================================================
// Базовый класс всех узлов АСД
// =============================================================================
struct ASTNode {
    int line = 0;  ///< Строка в исходном тексте (нумерация с 1)
    int col  = 0;  ///< Столбец в исходном тексте (нумерация с 1)

    virtual ~ASTNode() = default;

    /**
     * Рекурсивный вывод поддерева в поток os с отступом indent уровней.
     * Каждый уровень — 2 пробела.
     */
    virtual void print(std::ostream& os, int indent = 0) const = 0;
};

// =============================================================================
// NumberNode — числовой литерал
// Примеры: 42,  3.14,  1e+18,  8.41E-10
// =============================================================================
struct NumberNode : ASTNode {
    std::string raw;    ///< Исходный текст токена (например "1e+18")
    double      value;  ///< Вычисленное числовое значение

    NumberNode(const std::string& raw, double value, int line, int col)
        : raw(raw), value(value)
    { this->line = line; this->col = col; }

    void print(std::ostream& os, int indent = 0) const override;
};

// =============================================================================
// IdentNode — идентификатор (имя переменной)
// =============================================================================
struct IdentNode : ASTNode {
    std::string name;  ///< Имя идентификатора

    IdentNode(const std::string& name, int line, int col)
        : name(name)
    { this->line = line; this->col = col; }

    void print(std::ostream& os, int indent = 0) const override;
};

// =============================================================================
// BinaryOpNode — бинарная арифметическая операция ('+' или '*')
// =============================================================================
struct BinaryOpNode : ASTNode {
    char       op;     ///< Оператор: '+' или '*'
    ASTNodePtr left;   ///< Левый операнд
    ASTNodePtr right;  ///< Правый операнд

    BinaryOpNode(char op, ASTNodePtr left, ASTNodePtr right, int line, int col)
        : op(op), left(std::move(left)), right(std::move(right))
    { this->line = line; this->col = col; }

    void print(std::ostream& os, int indent = 0) const override;
};

// =============================================================================
// AssignNode — оператор присваивания:  ID = EXPR
// =============================================================================
struct AssignNode : ASTNode {
    std::string varName;  ///< Имя переменной в левой части
    ASTNodePtr  expr;     ///< Выражение в правой части

    AssignNode(const std::string& varName, ASTNodePtr expr, int line, int col)
        : varName(varName), expr(std::move(expr))
    { this->line = line; this->col = col; }

    void print(std::ostream& os, int indent = 0) const override;
};

// =============================================================================
// Реализации print() — определены здесь, чтобы не плодить лишний .cpp файл
// =============================================================================

inline void NumberNode::print(std::ostream& os, int indent) const {
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
    os << std::string(static_cast<size_t>(indent * 2), ' ')
       << "BinaryOpNode ['" << op << "']"
       << "  (line=" << line << ", col=" << col << ")\n";
    left->print(os, indent + 1);
    right->print(os, indent + 1);
}

inline void AssignNode::print(std::ostream& os, int indent) const {
    os << std::string(static_cast<size_t>(indent * 2), ' ')
       << "AssignNode [" << varName << " =]"
       << "  (line=" << line << ", col=" << col << ")\n";
    expr->print(os, indent + 1);
}
