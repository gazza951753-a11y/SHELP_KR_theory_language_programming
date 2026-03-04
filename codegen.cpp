/**
 * codegen.cpp — реализация генератора промежуточного кода и оптимизатора.
 */

#include "codegen.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// =============================================================================
// Публичный API
// =============================================================================

void CodeGen::generate(const ASTNode* root) {
    tempCount_      = 0;
    foldingApplied_ = false;

    // Проход 1: собрать таблицу символов
    collectSymbols(root);

    // Проход 2a: сгенерировать неоптимизированный 3AC
    genRaw(root);

    // Проход 2b: сгенерировать оптимизированный 3AC
    // Сбрасываем счётчик временных переменных, чтобы нумерация начиналась с t1
    tempCount_ = 0;
    genOpt(root);
}

// =============================================================================
// Вывод таблицы символов в виде отформатированной ASCII-таблицы
// =============================================================================
void CodeGen::printSymbolTable(std::ostream& os) const {
    // Заголовки столбцов (ASCII для корректного расчёта ширины)
    const std::vector<std::string> hdrs = {"Name", "Kind", "Value", "Line", "Col"};

    // Вычислить ширины столбцов
    std::vector<size_t> w(hdrs.size());
    for (size_t i = 0; i < hdrs.size(); ++i) w[i] = hdrs[i].size();

    for (const auto& s : symbols_) {
        w[0] = std::max(w[0], s.name.size());
        w[1] = std::max(w[1], s.kind.size());
        std::string val = s.hasValue ? formatNumber(s.value) : "-";
        w[2] = std::max(w[2], val.size());
        w[3] = std::max(w[3], std::to_string(s.line).size());
        w[4] = std::max(w[4], std::to_string(s.col).size());
    }

    // Разделитель строки
    auto sep = [&]() {
        os << "+";
        for (size_t i = 0; i < w.size(); ++i)
            os << std::string(w[i] + 2, '-') << "+";
        os << "\n";
    };

    // Вывод строки данных
    auto row = [&](const std::vector<std::string>& cells) {
        os << "|";
        for (size_t i = 0; i < cells.size(); ++i)
            os << " " << std::left << std::setw(static_cast<int>(w[i])) << cells[i] << " |";
        os << "\n";
    };

    sep();
    row(hdrs);
    sep();

    for (const auto& s : symbols_) {
        std::string val = s.hasValue ? formatNumber(s.value) : "-";
        row({s.name, s.kind, val, std::to_string(s.line), std::to_string(s.col)});
    }

    sep();
}

// =============================================================================
// Вывод трёхадресного кода
// =============================================================================

/// Форматировать одну инструкцию TAC в строку
static std::string formatTAC(const TAC& tac) {
    if (tac.op.empty()) {
        // Инструкция копирования: result = arg1
        return tac.result + " = " + tac.arg1;
    }
    return tac.result + " = " + tac.arg1 + " " + tac.op + " " + tac.arg2;
}

void CodeGen::printRawCode(std::ostream& os) const {
    for (const auto& tac : rawCode_)
        os << "    " << formatTAC(tac) << "\n";
}

void CodeGen::printOptCode(std::ostream& os) const {
    for (const auto& tac : optCode_)
        os << "    " << formatTAC(tac) << "\n";
}

// =============================================================================
// Сбор таблицы символов (обход АСД)
// =============================================================================

void CodeGen::collectSymbols(const ASTNode* node) {
    if (auto* n = dynamic_cast<const AssignNode*>(node)) {
        // Переменная в левой части — первая в таблице
        addSymbol(n->varName, "variable", false, 0.0, n->line, n->col);
        collectSymbols(n->expr.get());
    }
    else if (auto* n = dynamic_cast<const BinaryOpNode*>(node)) {
        collectSymbols(n->left.get());
        collectSymbols(n->right.get());
    }
    else if (auto* n = dynamic_cast<const IdentNode*>(node)) {
        addSymbol(n->name, "variable", false, 0.0, n->line, n->col);
    }
    else if (auto* n = dynamic_cast<const NumberNode*>(node)) {
        addSymbol(n->raw, numKind(n->raw), true, n->value, n->line, n->col);
    }
}

void CodeGen::addSymbol(const std::string& name, const std::string& kind,
                        bool hasValue, double value, int line, int col) {
    // Дубликаты не добавляем — оставляем первое вхождение
    if (symIndex_.count(name)) return;
    int idx = static_cast<int>(symbols_.size());
    symIndex_[name] = idx;
    symbols_.push_back({name, kind, hasValue, value, line, col});
}

/// Определить вид числовой константы по её строковому представлению
std::string CodeGen::numKind(const std::string& raw) {
    for (char c : raw) {
        if (c == '.' || c == 'e' || c == 'E') return "float";
    }
    return "integer";
}

// =============================================================================
// Генерация неоптимизированного 3AC
// =============================================================================
// Правило:
//   • AssignNode  → сгенерировать RHS, затем emit: varName = RHS
//   • BinaryOpNode → сгенерировать left и right, emit: t_i = left op right
//   • IdentNode   → вернуть имя
//   • NumberNode  → вернуть исходный текст
// =============================================================================

std::string CodeGen::genRaw(const ASTNode* node) {
    if (auto* n = dynamic_cast<const AssignNode*>(node)) {
        std::string rhs = genRaw(n->expr.get());
        // Явное копирование: result = t_last
        rawCode_.push_back({n->varName, rhs, "", ""});
        return n->varName;
    }
    else if (auto* n = dynamic_cast<const BinaryOpNode*>(node)) {
        std::string lv = genRaw(n->left.get());
        std::string rv = genRaw(n->right.get());
        std::string t  = newTemp();
        rawCode_.push_back({t, lv, std::string(1, n->op), rv});
        return t;
    }
    else if (auto* n = dynamic_cast<const IdentNode*>(node)) {
        return n->name;
    }
    else if (auto* n = dynamic_cast<const NumberNode*>(node)) {
        return n->raw;
    }
    throw std::runtime_error("genRaw: неизвестный тип узла АСД");
}

// =============================================================================
// Генерация оптимизированного 3AC (свёртка констант)
// =============================================================================
// Дополнительно: последнюю временную переменную переименовываем в имя
// целевой переменной (устраняем лишнее копирование).
// =============================================================================

/// Рекурсивная генерация выражения (не верхний AssignNode).
/// Возвращает имя операнда с результатом.
std::string CodeGen::genOptExpr(const ASTNode* node) {
    if (auto* n = dynamic_cast<const BinaryOpNode*>(node)) {
        std::string lv = genOptExpr(n->left.get());
        std::string rv = genOptExpr(n->right.get());

        // --- Свёртка констант ---
        // Если оба операнда — числовые константы с известными значениями,
        // вычисляем результат в compile-time и возвращаем его напрямую.
        if (isConstant(lv) && isConstant(rv)) {
            double lVal   = getConstValue(lv);
            double rVal   = getConstValue(rv);
            double result = (n->op == '+') ? lVal + rVal : lVal * rVal;
            std::string rs = formatNumber(result);

            // Регистрируем свёрнутую константу в таблице символов
            addSymbol(rs, numKind(rs), true, result, n->line, n->col);

            foldingApplied_ = true;
            return rs;  // возвращаем константу без эмиссии инструкции
        }

        // Иначе — стандартная временная переменная
        std::string t = newTemp();
        optCode_.push_back({t, lv, std::string(1, n->op), rv});
        return t;
    }
    else if (auto* n = dynamic_cast<const IdentNode*>(node)) {
        return n->name;
    }
    else if (auto* n = dynamic_cast<const NumberNode*>(node)) {
        return n->raw;
    }
    throw std::runtime_error("genOptExpr: unknown AST node type");
}

/// Верхний уровень: обработка AssignNode.
std::string CodeGen::genOpt(const ASTNode* node) {
    auto* n = dynamic_cast<const AssignNode*>(node);
    if (!n) {
        // Если по какой-то причине передан не AssignNode — делегируем
        return genOptExpr(node);
    }

    // Запоминаем количество инструкций до генерации выражения
    size_t beforeCount = optCode_.size();

    std::string rhs = genOptExpr(n->expr.get());

    if (rhs == n->varName) {
        // Тривиальный случай: x = x — ничего не делаем
        return n->varName;
    }

    // Оптимизация: если генерация выражения породила хотя бы одну инструкцию
    // и последняя инструкция пишет во временную переменную rhs —
    // переименовываем её в целевую переменную (устраняем копирование).
    if (optCode_.size() > beforeCount && isTempName(rhs)
        && optCode_.back().result == rhs)
    {
        // Переименовываем назначение последней инструкции
        optCode_.back().result = n->varName;
        return n->varName;
    }

    // Обычный случай: генерируем инструкцию копирования  varName = rhs
    optCode_.push_back({n->varName, rhs, "", ""});
    return n->varName;
}

// =============================================================================
// Вспомогательные методы
// =============================================================================

std::string CodeGen::newTemp() {
    ++tempCount_;
    return "t" + std::to_string(tempCount_);
}

bool CodeGen::isConstant(const std::string& name) const {
    auto it = symIndex_.find(name);
    if (it == symIndex_.end()) return false;
    return symbols_[it->second].hasValue;
}

double CodeGen::getConstValue(const std::string& name) const {
    auto it = symIndex_.find(name);
    if (it == symIndex_.end())
        throw std::runtime_error("getConstValue: symbol not found: " + name);
    return symbols_[it->second].value;
}

bool CodeGen::isTempName(const std::string& name) {
    if (name.size() < 2 || name[0] != 't') return false;
    for (size_t i = 1; i < name.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) return false;
    return true;
}

/// Форматировать число: если это целое значение — без дробной части,
/// иначе — с полной точностью.
std::string CodeGen::formatNumber(double v) {
    if (v == std::floor(v) && std::abs(v) < 1e15) {
        // Целое значение
        std::ostringstream oss;
        oss << static_cast<long long>(v);
        return oss.str();
    }
    // Вещественное значение — используем g-формат
    std::ostringstream oss;
    oss << std::setprecision(10) << std::noshowpoint << v;
    return oss.str();
}
