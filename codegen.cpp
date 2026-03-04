/*
 * codegen.cpp — генерация промежуточного трёхадресного кода (3AC)
 *               и его оптимизация (свёртка констант).
 *
 * Работаем в несколько проходов по дереву:
 *   Проход 0: collectSymbols — собираем все имена и числа в таблицу символов
 *   Проход 1: genRaw         — генерируем «сырой» неоптимизированный код
 *   Проход 2: genOpt         — генерируем оптимизированный код
 *
 * Трёхадресная инструкция выглядит так:
 *   t1 = a + 3      (результат = левый  оператор правый)
 *   result = t1     (копирование: op пустое)
 */

#include "codegen.h"

#include <algorithm>  // std::max
#include <cmath>      // std::floor, std::abs
#include <iomanip>    // std::setw, std::left, std::setprecision
#include <sstream>    // ostringstream
#include <stdexcept>  // runtime_error

// =============================================================================
// Главный метод — запускает все проходы
// =============================================================================

void CodeGen::generate(const ASTNode* root) {
    // Обнуляем состояние (на случай если generate() вдруг вызовут второй раз)
    tempCount_      = 0;
    foldingApplied_ = false;

    // Проход 0: обходим дерево и собираем все символы (переменные и числа)
    collectSymbols(root);

    // Проход 1: генерируем неоптимизированный 3AC
    genRaw(root);

    // Проход 2: генерируем оптимизированный 3AC.
    // Сбрасываем счётчик — временные переменные снова начнутся с t1
    tempCount_ = 0;
    genOpt(root);
}

// =============================================================================
// Вывод таблицы символов в красивом виде (с рамочкой из + и -)
// =============================================================================

void CodeGen::printSymbolTable(std::ostream& os) const {
    // Заголовки столбцов — всё ASCII, чтобы ширина считалась правильно
    const std::vector<std::string> hdrs = {"Name", "Kind", "Value", "Line", "Col"};

    // Считаем нужную ширину каждого столбца (берём максимум среди всех строк)
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

    // Лямбда sep() — рисует горизонтальную линию: +--------+----------+...
    auto sep = [&]() {
        os << "+";
        for (size_t i = 0; i < w.size(); ++i)
            os << std::string(w[i] + 2, '-') << "+";
        os << "\n";
    };

    // Лямбда row() — рисует одну строку таблицы с данными
    auto row = [&](const std::vector<std::string>& cells) {
        os << "|";
        for (size_t i = 0; i < cells.size(); ++i)
            // setw + left — выровнять текст по левому краю в поле нужной ширины
            os << " " << std::left << std::setw(static_cast<int>(w[i])) << cells[i] << " |";
        os << "\n";
    };

    sep();
    row(hdrs);  // шапка
    sep();

    // Строки с данными
    for (const auto& s : symbols_) {
        std::string val = s.hasValue ? formatNumber(s.value) : "-";
        row({s.name, s.kind, val, std::to_string(s.line), std::to_string(s.col)});
    }

    sep();
}

// =============================================================================
// Форматирование и вывод трёхадресных инструкций
// =============================================================================

// Превращает одну TAC-инструкцию в строку вида  "t1 = a + 3"
static std::string formatTAC(const TAC& tac) {
    if (tac.op.empty()) {
        // op пустое — это копирование:  result = arg1
        return tac.result + " = " + tac.arg1;
    }
    // Бинарная операция:  result = arg1 op arg2
    return tac.result + " = " + tac.arg1 + " " + tac.op + " " + tac.arg2;
}

void CodeGen::printRawCode(std::ostream& os) const {
    // Просто выводим каждую инструкцию с отступом в 4 пробела
    for (const auto& tac : rawCode_)
        os << "    " << formatTAC(tac) << "\n";
}

void CodeGen::printOptCode(std::ostream& os) const {
    for (const auto& tac : optCode_)
        os << "    " << formatTAC(tac) << "\n";
}

// =============================================================================
// Проход 0: сбор таблицы символов
// Обходим дерево рекурсивно и добавляем в таблицу каждое имя и число
// =============================================================================

void CodeGen::collectSymbols(const ASTNode* node) {
    if (auto* n = dynamic_cast<const AssignNode*>(node)) {
        // Переменная из левой части — она первая в таблице
        addSymbol(n->varName, "variable", false, 0.0, n->line, n->col);
        // Рекурсивно обходим правую часть
        collectSymbols(n->expr.get());
    }
    else if (auto* n = dynamic_cast<const BinaryOpNode*>(node)) {
        // Узел операции — обходим оба поддерева
        collectSymbols(n->left.get());
        collectSymbols(n->right.get());
    }
    else if (auto* n = dynamic_cast<const IdentNode*>(node)) {
        // Имя переменной — добавляем в таблицу
        addSymbol(n->name, "variable", false, 0.0, n->line, n->col);
    }
    else if (auto* n = dynamic_cast<const NumberNode*>(node)) {
        // Числовая константа — добавляем с её значением
        addSymbol(n->raw, numKind(n->raw), true, n->value, n->line, n->col);
    }
}

// Добавить символ в таблицу. Если имя уже есть — пропускаем (дубли не нужны).
void CodeGen::addSymbol(const std::string& name, const std::string& kind,
                        bool hasValue, double value, int line, int col) {
    // count() возвращает 0 или 1 — есть ли такой ключ в map
    if (symIndex_.count(name)) return;
    int idx = static_cast<int>(symbols_.size());
    symIndex_[name] = idx;  // запоминаем индекс для быстрого поиска
    symbols_.push_back({name, kind, hasValue, value, line, col});
}

// Определить тип числа по его строковому виду.
// Если есть точка или e/E — это вещественное (float), иначе — целое (integer).
std::string CodeGen::numKind(const std::string& raw) {
    for (char c : raw) {
        if (c == '.' || c == 'e' || c == 'E') return "float";
    }
    return "integer";
}

// =============================================================================
// Проход 1: генерация неоптимизированного трёхадресного кода
// =============================================================================
// Принцип: обходим дерево снизу вверх.
// Для каждой операции создаём новую временную переменную t1, t2, ...
// Возвращаем имя переменной где лежит результат данного поддерева.
// =============================================================================

std::string CodeGen::genRaw(const ASTNode* node) {
    if (auto* n = dynamic_cast<const AssignNode*>(node)) {
        // Генерируем код для правой части
        std::string rhs = genRaw(n->expr.get());
        // Добавляем инструкцию копирования:  result = t_last
        rawCode_.push_back({n->varName, rhs, "", ""});
        return n->varName;
    }
    else if (auto* n = dynamic_cast<const BinaryOpNode*>(node)) {
        std::string lv = genRaw(n->left.get());   // код для левого поддерева
        std::string rv = genRaw(n->right.get());  // код для правого поддерева
        std::string t  = newTemp();               // новая временная: t1, t2, ...
        // Добавляем инструкцию:  t = lv op rv
        rawCode_.push_back({t, lv, std::string(1, n->op), rv});
        return t;
    }
    else if (auto* n = dynamic_cast<const IdentNode*>(node)) {
        // Переменная — просто возвращаем её имя, ничего не генерируем
        return n->name;
    }
    else if (auto* n = dynamic_cast<const NumberNode*>(node)) {
        // Число — возвращаем его как строку (например "3" или "3.14")
        return n->raw;
    }
    // Сюда попасть нельзя — все виды узлов обработаны выше
    throw std::runtime_error("genRaw: неизвестный тип узла АСД");
}

// =============================================================================
// Проход 2: генерация оптимизированного кода (со свёрткой констант)
// =============================================================================

// genOptExpr — генерирует код для выражения (не для узла присваивания).
// Возвращает имя где лежит результат.
std::string CodeGen::genOptExpr(const ASTNode* node) {
    if (auto* n = dynamic_cast<const BinaryOpNode*>(node)) {
        std::string lv = genOptExpr(n->left.get());
        std::string rv = genOptExpr(n->right.get());

        // --- Свёртка констант ---
        // Если оба операнда — известные числовые константы, считаем результат
        // прямо сейчас, во время компиляции. Никакую инструкцию не генерируем!
        if (isConstant(lv) && isConstant(rv)) {
            double lVal   = getConstValue(lv);
            double rVal   = getConstValue(rv);
            // Вычисляем: либо сложение, либо умножение
            double result = (n->op == '+') ? lVal + rVal : lVal * rVal;
            std::string rs = formatNumber(result);

            // Регистрируем полученное число как константу в таблице символов
            // (оно может понадобиться для следующих свёрток)
            addSymbol(rs, numKind(rs), true, result, n->line, n->col);

            foldingApplied_ = true;  // помечаем что свёртка была
            return rs;               // возвращаем уже готовое число
        }

        // Оба не константы — создаём временную переменную как обычно
        std::string t = newTemp();
        optCode_.push_back({t, lv, std::string(1, n->op), rv});
        return t;
    }
    else if (auto* n = dynamic_cast<const IdentNode*>(node)) {
        return n->name;  // переменная — просто имя
    }
    else if (auto* n = dynamic_cast<const NumberNode*>(node)) {
        return n->raw;   // число — просто его текст
    }
    throw std::runtime_error("genOptExpr: неизвестный тип узла АСД");
}

// genOpt — обрабатывает корневой узел (AssignNode) с оптимизацией.
std::string CodeGen::genOpt(const ASTNode* node) {
    auto* n = dynamic_cast<const AssignNode*>(node);
    if (!n) {
        // Если вдруг передали не AssignNode — делегируем genOptExpr
        return genOptExpr(node);
    }

    // Запоминаем сколько инструкций было до генерации правой части
    size_t beforeCount = optCode_.size();

    std::string rhs = genOptExpr(n->expr.get());

    // Тривиальный случай: x = x — такое не генерируем
    if (rhs == n->varName) {
        return n->varName;
    }

    // Оптимизация устранения копирования:
    // Если правая часть породила хотя бы одну инструкцию, и последняя
    // инструкция записывает во временную переменную rhs — просто
    // переименовываем её результат в целевую переменную.
    // Так вместо:
    //   t3 = t1 * t2
    //   result = t3
    // Получаем одну инструкцию:
    //   result = t1 * t2
    if (optCode_.size() > beforeCount && isTempName(rhs)
        && optCode_.back().result == rhs)
    {
        optCode_.back().result = n->varName;  // переименовываем t_N -> result
        return n->varName;
    }

    // Обычный случай — генерируем явное копирование:  result = rhs
    optCode_.push_back({n->varName, rhs, "", ""});
    return n->varName;
}

// =============================================================================
// Служебные вспомогательные методы
// =============================================================================

// Создать имя следующей временной переменной: t1, t2, t3, ...
std::string CodeGen::newTemp() {
    ++tempCount_;
    return "t" + std::to_string(tempCount_);
}

// Проверить, является ли имя числовой константой с известным значением
bool CodeGen::isConstant(const std::string& name) const {
    auto it = symIndex_.find(name);
    if (it == symIndex_.end()) return false;
    return symbols_[it->second].hasValue;
}

// Получить числовое значение константы по её имени.
// Если такого имени нет — это ошибка в логике программы (не пользователя).
double CodeGen::getConstValue(const std::string& name) const {
    auto it = symIndex_.find(name);
    if (it == symIndex_.end())
        throw std::runtime_error("getConstValue: символ не найден: " + name);
    return symbols_[it->second].value;
}

// Проверить, выглядит ли имя как временная переменная.
// Временные переменные: t1, t2, t3, ... — буква t, потом только цифры.
bool CodeGen::isTempName(const std::string& name) {
    if (name.size() < 2 || name[0] != 't') return false;
    for (size_t i = 1; i < name.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) return false;
    return true;
}

// Превратить double в строку для вывода.
// Если число «целое» (например 7.0) — пишем просто "7", без дробной части.
// Иначе — пишем с нужной точностью.
std::string CodeGen::formatNumber(double v) {
    if (v == std::floor(v) && std::abs(v) < 1e15) {
        // Целое значение: приводим к long long чтобы не было "7.00000"
        std::ostringstream oss;
        oss << static_cast<long long>(v);
        return oss.str();
    }
    // Вещественное значение — используем «общий» формат (g-format)
    std::ostringstream oss;
    oss << std::setprecision(10) << std::noshowpoint << v;
    return oss.str();
}
