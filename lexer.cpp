/*
 * lexer.cpp — реализация лексического анализатора для объявлений структур.
 *
 * Основные особенности реализации:
 *  • Отслеживание строки и столбца: col_ увеличивается на 1 для каждого символа;
 *    при встрече '\n' увеличиваем line_ и сбрасываем col_ в 1.
 *  • Комментарии: однострочные комментарии C++ (//) полностью пропускаются.
 *  • Ключевые слова: идентификаторы, совпадающие с зарезервированными словами,
 *    переклассифицируются в соответствующий тип токена.
 *  • Ошибка: любой нераспознанный символ вызывает исключение LexerError.
 */

#include "lexer.h"

#include <cctype>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Вспомогательные методы структуры Token
// ---------------------------------------------------------------------------

/*
 * grammarSymbol() — переводит тип токена в строку, используемую в грамматике
 * и таблице LL(1)-разбора. Нужен для связи лексера с парсером.
 */
std::string Token::grammarSymbol() const {
    switch (type) {
        case TokenType::KW_STRUCT:    return "struct";
        case TokenType::KW_INT:       return "int";
        case TokenType::KW_DOUBLE:    return "double";
        case TokenType::KW_FLOAT:     return "float";
        case TokenType::KW_CHAR:      return "char";
        case TokenType::KW_BOOL:      return "bool";
        case TokenType::KW_STRING:    return "string";
        case TokenType::IDENTIFIER:   return "<identifier>";
        case TokenType::INTEGER:      return "<integer>";
        case TokenType::LBRACE:       return "{";
        case TokenType::RBRACE:       return "}";
        case TokenType::SEMICOLON:    return ";";
        case TokenType::LBRACKET:     return "[";
        case TokenType::RBRACKET:     return "]";
        case TokenType::EOF_TOKEN:    return "$";
    }
    return "?";  // сюда никогда не попадём, но компилятор требует return
}

/*
 * typeName() — возвращает читаемое название типа токена для сообщений об ошибках.
 */
std::string Token::typeName() const {
    switch (type) {
        case TokenType::KW_STRUCT:    return "keyword 'struct'";
        case TokenType::KW_INT:       return "keyword 'int'";
        case TokenType::KW_DOUBLE:    return "keyword 'double'";
        case TokenType::KW_FLOAT:     return "keyword 'float'";
        case TokenType::KW_CHAR:      return "keyword 'char'";
        case TokenType::KW_BOOL:      return "keyword 'bool'";
        case TokenType::KW_STRING:    return "keyword 'string'";
        case TokenType::IDENTIFIER:   return "identifier";
        case TokenType::INTEGER:      return "integer";
        case TokenType::LBRACE:       return "'{'";
        case TokenType::RBRACE:       return "'}'";
        case TokenType::SEMICOLON:    return "';'";
        case TokenType::LBRACKET:     return "'['";
        case TokenType::RBRACKET:     return "']'";
        case TokenType::EOF_TOKEN:    return "end-of-file";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Конструктор лексера
// ---------------------------------------------------------------------------

Lexer::Lexer(const std::string& source)
    : src_(source), pos_(0), line_(1), col_(1)
{}

// ---------------------------------------------------------------------------
// Вспомогательные методы для работы с символами
// ---------------------------------------------------------------------------

/*
 * atEnd() — возвращает true, если достигнут конец входного текста.
 */
bool Lexer::atEnd() const {
    return pos_ >= src_.size();
}

/*
 * cur() — возвращает текущий символ (не двигает позицию).
 * Если конец строки — возвращает '\0'.
 */
char Lexer::cur() const {
    if (atEnd()) return '\0';
    return src_[pos_];
}

/*
 * peek(offset) — смотрит вперёд на offset символов без сдвига позиции.
 * Используется, например, чтобы проверить "//" (два слэша подряд).
 */
char Lexer::peek(size_t offset) const {
    size_t idx = pos_ + offset;
    if (idx >= src_.size()) return '\0';
    return src_[idx];
}

/*
 * advance() — читает текущий символ, продвигает позицию вперёд,
 * обновляет счётчики строки и столбца, возвращает считанный символ.
 */
char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') {
        // Встретили перевод строки — переходим на новую строку
        ++line_;
        col_ = 1;
    } else {
        ++col_;
    }
    return c;
}

// ---------------------------------------------------------------------------
// Пропуск пробелов и комментариев
// ---------------------------------------------------------------------------

/*
 * skipWhitespace() — пропускает все пробельные символы и однострочные
 * комментарии вида "// ...". Вызывается перед каждым новым токеном.
 */
void Lexer::skipWhitespace() {
    while (!atEnd()) {
        char c = cur();

        if (std::isspace(static_cast<unsigned char>(c))) {
            // Пробельный символ — просто пропускаем
            advance();
        } else if (c == '/' && peek() == '/') {
            // Однострочный комментарий C++ — пропускаем до конца строки
            while (!atEnd() && cur() != '\n') {
                advance();
            }
        } else {
            // Нашли значимый символ — выходим
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Чтение составных токенов
// ---------------------------------------------------------------------------

/*
 * readIdentifierOrKeyword()
 * --------------------------
 * Предусловие: cur() — буква или символ подчёркивания.
 * Читает последовательность [A-Za-z_][A-Za-z0-9_]*, затем проверяет,
 * является ли считанная строка ключевым словом.
 * Возвращает токен нужного типа (ключевое слово или идентификатор).
 */
Token Lexer::readIdentifierOrKeyword() {
    // Таблица всех ключевых слов нашей грамматики
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"struct", TokenType::KW_STRUCT},
        {"int",    TokenType::KW_INT},
        {"double", TokenType::KW_DOUBLE},
        {"float",  TokenType::KW_FLOAT},
        {"char",   TokenType::KW_CHAR},
        {"bool",   TokenType::KW_BOOL},
        {"string", TokenType::KW_STRING},
    };

    int startLine = line_;
    int startCol  = col_;
    std::string lexeme;

    // Читаем символы пока они буквенно-цифровые или '_'
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(cur())) || cur() == '_')) {
        lexeme += advance();
    }

    // Проверяем — не ключевое ли это слово?
    auto it = keywords.find(lexeme);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;

    return Token(type, lexeme, startLine, startCol);
}

/*
 * readInteger()
 * -------------
 * Предусловие: cur() — цифра.
 * Читает одну или более десятичных цифр подряд.
 * Возвращает токен типа INTEGER.
 */
Token Lexer::readInteger() {
    int startLine = line_;
    int startCol  = col_;
    std::string lexeme;

    while (!atEnd() && std::isdigit(static_cast<unsigned char>(cur()))) {
        lexeme += advance();
    }

    return Token(TokenType::INTEGER, lexeme, startLine, startCol);
}

// ---------------------------------------------------------------------------
// Главный публичный метод: токенизация всего исходного текста
// ---------------------------------------------------------------------------

/*
 * tokenize()
 * ----------
 * Сканирует весь исходный текст и возвращает вектор токенов.
 * Последний элемент вектора всегда EOF_TOKEN ($).
 *
 * Алгоритм:
 *   1. Пропускаем пробелы и комментарии.
 *   2. Определяем тип следующего токена по первому символу.
 *   3. Повторяем до конца входа.
 *   4. Добавляем маркер конца потока EOF_TOKEN.
 */
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();

        if (atEnd()) break;  // достигли конца исходного текста

        char c    = cur();
        int  line = line_;
        int  col  = col_;

        // ---- Идентификатор или ключевое слово ----
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // ---- Целочисленный литерал ----
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readInteger());
            continue;
        }

        // ---- Односимвольные знаки пунктуации ----
        advance();  // потребляем символ
        switch (c) {
            case '{':  tokens.emplace_back(TokenType::LBRACE,    "{", line, col); break;
            case '}':  tokens.emplace_back(TokenType::RBRACE,    "}", line, col); break;
            case ';':  tokens.emplace_back(TokenType::SEMICOLON, ";", line, col); break;
            case '[':  tokens.emplace_back(TokenType::LBRACKET,  "[", line, col); break;
            case ']':  tokens.emplace_back(TokenType::RBRACKET,  "]", line, col); break;
            default:
                // Неизвестный символ — это лексическая ошибка
                throw LexerError(
                    std::string("Неожиданный символ '") + c + "'",
                    line, col);
        }
    }

    // В конце всегда добавляем маркер конца потока
    tokens.emplace_back(TokenType::EOF_TOKEN, "$", line_, col_);
    return tokens;
}
