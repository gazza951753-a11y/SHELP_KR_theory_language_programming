/*
 * grammar.cpp — загрузка грамматики, вычисление множеств FIRST/FOLLOW/направляющих,
 *               проверка свойства LL(1) и построение таблицы разбора.
 *
 * Все алгоритмы разобраны подробно в комментариях к каждой функции.
 */

#include "grammar.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

// =============================================================================
// Вспомогательные строковые функции
// =============================================================================

/*
 * trim(s) — убирает пробельные символы с обоих концов строки.
 * Используется при парсинге файла грамматики.
 */
std::string Grammar::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";  // строка состоит только из пробелов
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/*
 * splitBy(s, delim) — разбивает строку s по разделителю delim (не регулярное
 * выражение, не набор символов, а буквальная подстрока).
 * Используется для разбивки правой части по символу '|'.
 */
std::vector<std::string> Grammar::splitBy(const std::string& s,
                                          const std::string& delim) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        parts.push_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    parts.push_back(s.substr(start));  // добавляем последний фрагмент
    return parts;
}

/*
 * splitByWhitespace(s) — разбивает строку по любым пробельным символам.
 * Используется для разбивки символов в правой части правила.
 */
std::vector<std::string> Grammar::splitByWhitespace(const std::string& s) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        result.push_back(token);
    }
    return result;
}

// =============================================================================
// Загрузка грамматики из файла
// =============================================================================

/*
 * parseRuleLine(line)
 * -------------------
 * Разбирает одну строку вида "FIELDLIST -> FIELDDECL FIELDLIST | eps".
 *
 * Шаги:
 *   1. Ищем "->", делим на ЛЧС и строку правых частей.
 *   2. Строку правых частей разбиваем по "|" на отдельные альтернативы.
 *   3. Для каждой альтернативы:
 *        - Если текст равен "eps" — создаём правило с пустой правой частью (ε)
 *        - Иначе разбиваем по пробелам и получаем список символов правой части
 *      Добавляем объект Rule в список rules_.
 *   4. Регистрируем ЛЧС как нетерминал.
 */
void Grammar::parseRuleLine(const std::string& line) {
    const std::string arrow = "->";
    auto arrowPos = line.find(arrow);
    if (arrowPos == std::string::npos) return;  // нет стрелки — пропускаем строку

    std::string lhs    = trim(line.substr(0, arrowPos));
    std::string rhsStr = trim(line.substr(arrowPos + arrow.size()));

    if (lhs.empty() || rhsStr.empty()) return;  // пустая строка — пропускаем

    nonterminals_.insert(lhs);  // ЛЧС всегда нетерминал

    // Разбиваем альтернативы по вертикальной черте
    auto alts = splitBy(rhsStr, "|");
    for (auto& alt : alts) {
        alt = trim(alt);
        Rule rule;
        rule.lhs = lhs;

        if (alt == "eps") {
            // ε-правило: правая часть остаётся пустой
        } else {
            rule.rhs = splitByWhitespace(alt);
        }
        rules_.push_back(rule);
    }
}

/*
 * loadFromFile(filename)
 * ----------------------
 * Загружает грамматику из текстового файла построчно.
 * Пустые строки и строки-комментарии (#) игнорируются.
 * После загрузки правил последовательно выполняет три прохода:
 *   computeFirstSets(), computeFollowSets(), buildParseTable().
 */
bool Grammar::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    bool firstRule = true;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        // Пропускаем пустые строки и комментарии
        if (line.empty() || line[0] == '#') continue;

        parseRuleLine(line);

        // Стартовый символ — ЛЧС самого первого правила
        if (firstRule && !rules_.empty()) {
            startSymbol_ = rules_.front().lhs;
            firstRule = false;
        }
    }

    if (rules_.empty()) return false;  // грамматика пустая — ошибка

    // --------------------------------------------------------------------------
    // Определяем терминалы: любой символ правой части, который не является
    // нетерминалом и не является "eps", считается терминалом.
    // --------------------------------------------------------------------------
    for (const auto& rule : rules_) {
        for (const auto& sym : rule.rhs) {
            if (nonterminals_.find(sym) == nonterminals_.end()) {
                terminals_.insert(sym);
            }
        }
    }
    // Маркер конца ввода $ — всегда терминал
    terminals_.insert("$");

    // --------------------------------------------------------------------------
    // Три прохода вычисления: FIRST → FOLLOW → таблица разбора
    // --------------------------------------------------------------------------
    computeFirstSets();
    computeFollowSets();
    ll1_ = buildParseTable();

    return true;
}

// =============================================================================
// Вычисление множеств FIRST
// =============================================================================

/*
 * firstOfSequence([X1, X2, …, Xn])
 * ----------------------------------
 * Возвращает FIRST(X1 X2 … Xn) — множество терминалов, которые могут
 * стоять первыми в строках, выводимых из данной последовательности.
 *
 * Алгоритм (разбираем символы слева направо):
 *   Для каждого Xi:
 *     (а) Добавляем FIRST(Xi) \ {eps} в результат.
 *     (б) Если eps ∉ FIRST(Xi) → стоп: этот Xi не может быть пустым,
 *         значит за ним ничего не «просвечивает».
 *     (в) Если eps ∈ FIRST(Xi) → продолжаем к Xi+1 (Xi может исчезнуть).
 *   Если все Xi могут давать eps → добавляем eps в результат.
 *
 * Для пустой последовательности FIRST = {eps}.
 */
std::set<std::string> Grammar::firstOfSequence(
        const std::vector<std::string>& seq) const {

    std::set<std::string> result;

    if (seq.empty()) {
        // Пустая последовательность всегда даёт eps
        result.insert("eps");
        return result;
    }

    for (const auto& sym : seq) {
        // Берём FIRST(sym). Если символ не найден — считаем его терминалом
        std::set<std::string> firstSym;
        auto it = firstSets_.find(sym);
        if (it != firstSets_.end()) {
            firstSym = it->second;
        } else {
            firstSym.insert(sym);  // терминал: FIRST = {сам символ}
        }

        // Шаг (а): добавляем FIRST(sym) \ {eps}
        for (const auto& s : firstSym) {
            if (s != "eps") result.insert(s);
        }

        // Шаг (б): если sym не может давать eps — прекращаем просмотр
        if (firstSym.find("eps") == firstSym.end()) {
            return result;
        }
        // Шаг (в): sym может быть пустым — идём дальше по последовательности
    }

    // Все символы последовательности могут давать eps — вся цепочка тоже
    result.insert("eps");
    return result;
}

/*
 * computeFirstSets()
 * ------------------
 * Итеративно вычисляет FIRST(A) для всех символов грамматики.
 *
 * Инициализация:
 *   • FIRST(терминал) = {терминал}  (терминал выводит только сам себя)
 *   • FIRST(нетерминал) = {}        (пополняется в процессе итерации)
 *
 * Итерация:
 *   Для каждого правила A → X1 X2 … Xn:
 *     Вычисляем FIRST(X1 X2 … Xn) и добавляем всё в FIRST(A).
 *   Повторяем до тех пор, пока хоть одно множество изменилось.
 *
 * Завершение гарантировано: мы только добавляем элементы (не убираем),
 * а алфавит конечен — значит рано или поздно ничего не изменится.
 */
void Grammar::computeFirstSets() {
    // Инициализация для терминалов: FIRST(t) = {t}
    for (const auto& t : terminals_) {
        firstSets_[t].insert(t);
    }
    // Инициализация для нетерминалов: пустое множество
    for (const auto& nt : nonterminals_) {
        firstSets_[nt];  // создаёт пустое множество, если его нет
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& rule : rules_) {
            auto&  firstA   = firstSets_[rule.lhs];
            size_t oldSize  = firstA.size();

            // Вычисляем FIRST правой части и сливаем с FIRST левой части
            auto firstRhs = firstOfSequence(rule.rhs);
            firstA.insert(firstRhs.begin(), firstRhs.end());

            if (firstA.size() != oldSize) changed = true;  // было добавлено что-то новое
        }
    }
}

// =============================================================================
// Вычисление множеств FOLLOW
// =============================================================================

/*
 * computeFollowSets()
 * -------------------
 * Итеративно вычисляет FOLLOW(A) для каждого нетерминала A.
 *
 * Инициализация:
 *   • FOLLOW(стартовый символ) = {"$"}
 *   • FOLLOW(все остальные нетерминалы) = {}
 *
 * Итерация (для каждого правила A → α B β):
 *
 *   Правило 1: Добавляем FIRST(β) \ {eps} в FOLLOW(B).
 *              (Что может идти первым после β — то может следовать за B.)
 *
 *   Правило 2: Если eps ∈ FIRST(β), добавляем FOLLOW(A) в FOLLOW(B).
 *              (Если β может быть пустой, то за B может идти то же, что за A.)
 *
 * Повторяем до стабилизации (ни одно множество не изменилось).
 *
 * Пример: для правила STRUCTDECL → struct <identifier> { FIELDLIST } ;
 *   β после FIELDLIST = ["}", ";"]
 *   FIRST(["}", ";"]) = {"}"}  (нет eps)
 *   → добавляем "}" в FOLLOW(FIELDLIST)
 */
void Grammar::computeFollowSets() {
    // Инициализируем все FOLLOW-множества пустыми
    for (const auto& nt : nonterminals_) {
        followSets_[nt];  // пустое множество
    }
    // Стартовый символ: FOLLOW(S) ∋ $
    followSets_[startSymbol_].insert("$");

    bool changed = true;
    while (changed) {
        changed = false;

        for (const auto& rule : rules_) {
            const std::string& A = rule.lhs;  // левая часть правила

            for (size_t i = 0; i < rule.rhs.size(); ++i) {
                const std::string& B = rule.rhs[i];

                // FOLLOW есть только у нетерминалов
                if (!isNonterminal(B)) continue;

                // β — всё, что стоит правее B в данном правиле
                std::vector<std::string> beta(rule.rhs.begin() + i + 1,
                                              rule.rhs.end());
                auto firstBeta = firstOfSequence(beta);

                // Правило 1: FIRST(β) \ {eps} → FOLLOW(B)
                for (const auto& s : firstBeta) {
                    if (s != "eps") {
                        if (followSets_[B].insert(s).second) changed = true;
                    }
                }

                // Правило 2: если eps ∈ FIRST(β), то FOLLOW(A) → FOLLOW(B)
                if (firstBeta.count("eps")) {
                    for (const auto& s : followSets_[A]) {
                        if (followSets_[B].insert(s).second) changed = true;
                    }
                }
            }
        }
    }
}

// =============================================================================
// Построение таблицы разбора и проверка LL(1)
// =============================================================================

/*
 * buildParseTable()
 * -----------------
 * Для каждого правила (с индексом i) A → α вычисляем НАПРАВЛЯЮЩЕЕ МНОЖЕСТВО:
 *
 *   T(A → α) = FIRST(α) \ {eps}
 *            ∪ FOLLOW(A),  если eps ∈ FIRST(α)
 *
 * Смысл:
 *   • FIRST(α) \ {eps}: терминалы, с которых может начинаться α, говорят
 *     парсеру "применяй это правило, когда видишь этот терминал в lookahead".
 *   • Если α может давать пустую строку (eps ∈ FIRST(α)), то правило также
 *     применяется, когда lookahead ∈ FOLLOW(A) — A "исчезает", и управление
 *     переходит к следующему символу в родительском правиле.
 *
 * Заполнение таблицы:
 *   Для каждого t ∈ T(A → α):
 *     Если table[A][t] уже занят другим правилом → КОНФЛИКТ → не LL(1).
 *     Иначе: table[A][t] = i.
 *
 * Возвращает true, если конфликтов нет (грамматика LL(1)).
 */
bool Grammar::buildParseTable() {
    bool ok = true;

    for (size_t i = 0; i < rules_.size(); ++i) {
        const Rule& rule = rules_[i];

        // Вычисляем FIRST правой части
        auto firstAlpha = firstOfSequence(rule.rhs);

        // Строим направляющее множество T(A → α)
        std::set<std::string> directing;

        // Часть 1: FIRST(α) \ {eps}
        for (const auto& s : firstAlpha) {
            if (s != "eps") directing.insert(s);
        }

        // Часть 2: если eps ∈ FIRST(α), добавляем FOLLOW(A)
        if (firstAlpha.count("eps")) {
            auto it = followSets_.find(rule.lhs);
            if (it != followSets_.end()) {
                directing.insert(it->second.begin(), it->second.end());
            }
        }

        // Заполняем таблицу разбора
        for (const auto& t : directing) {
            auto& cell = parseTable_[rule.lhs][t];
            if (cell != 0 && cell != (int)i) {
                // Уже занято другим правилом — конфликт
                ok = false;
            } else {
                if (parseTable_[rule.lhs].find(t) == parseTable_[rule.lhs].end()) {
                    parseTable_[rule.lhs][t] = (int)i;
                } else if (parseTable_[rule.lhs][t] != (int)i) {
                    ok = false;
                } else {
                    parseTable_[rule.lhs][t] = (int)i;
                }
            }
        }
    }

    // Выполняем чистый второй проход с отдельной структурой отслеживания конфликтов
    // (первый проход имеет тонкую проблему: индекс правила 0 совпадает с "не задано")
    parseTable_.clear();
    std::map<std::string, std::map<std::string, int>> table;
    std::map<std::string, std::map<std::string, bool>> seen;  // отмечаем, было ли уже занято
    ok = true;

    for (size_t i = 0; i < rules_.size(); ++i) {
        const Rule& rule = rules_[i];

        auto firstAlpha = firstOfSequence(rule.rhs);

        // Снова строим направляющее множество
        std::set<std::string> directing;
        for (const auto& s : firstAlpha) {
            if (s != "eps") directing.insert(s);
        }
        if (firstAlpha.count("eps")) {
            auto it = followSets_.find(rule.lhs);
            if (it != followSets_.end()) {
                directing.insert(it->second.begin(), it->second.end());
            }
        }

        // Заполняем таблицу с явной проверкой конфликтов
        for (const auto& t : directing) {
            if (seen[rule.lhs][t]) {
                // Эта ячейка уже занята — грамматика не LL(1)
                ok = false;
            } else {
                seen[rule.lhs][t] = true;
                table[rule.lhs][t] = (int)i;
            }
        }
    }

    parseTable_ = std::move(table);
    return ok;
}
