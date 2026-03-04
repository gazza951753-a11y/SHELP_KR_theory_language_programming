/**
 * lexer.cpp — реализация лексического анализатора.
 *
 * Стратегия:
 *   • Пробельные символы игнорируются между токенами.
 *   • Числа: целые → [0-9]+
 *            вещественные → [0-9]+ '.' [0-9]*
 *            научная нотация → [0-9]+['.'[0-9]*] [eE][+-]? [0-9]+
 *   • Идентификаторы: [a-zA-Z][a-zA-Z0-9]*
 *   • Одиночные операторы: '+', '*', '=', '(', ')'
 *   • При встрече неизвестного символа выбрасывается LexerError.
 */

#include "lexer.h"

#include <cctype>
#include <sstream>

// -----------------------------------------------------------------------------
// Вспомогательная функция: читаемое название типа токена
// -----------------------------------------------------------------------------
std::string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::TK_ID:     return "IDENTIFIER";
        case TokenType::TK_NUMBER: return "NUMBER";
        case TokenType::TK_PLUS:   return "'+'";
        case TokenType::TK_STAR:   return "'*'";
        case TokenType::TK_ASSIGN: return "'='";
        case TokenType::TK_LPAREN: return "'('";
        case TokenType::TK_RPAREN: return "')'";
        case TokenType::TK_EOF:    return "EOF";
        default:                   return "UNKNOWN";
    }
}

// -----------------------------------------------------------------------------
// Конструктор
// -----------------------------------------------------------------------------
Lexer::Lexer(const std::string& source)
    : src_(source)
    , pos_(0)
    , line_(1)
    , col_(1)
    , hasPeeked_(false)
    , peeked_(TokenType::TK_EOF, "", 0, 0)
{}

// -----------------------------------------------------------------------------
// Внутренние вспомогательные методы
// -----------------------------------------------------------------------------

bool Lexer::atEnd() const {
    return pos_ >= src_.size();
}

char Lexer::current() const {
    return atEnd() ? '\0' : src_[pos_];
}

/// Продвигает позицию на 1 символ, учитывая переводы строк.
char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') {
        ++line_;
        col_ = 1;
    } else {
        ++col_;
    }
    return c;
}

/// Пропускает все пробельные символы.
void Lexer::skipWhitespace() {
    while (!atEnd() && std::isspace(static_cast<unsigned char>(current()))) {
        advance();
    }
}

// -----------------------------------------------------------------------------
// Считывание числового литерала.
// Формат: [0-9]+ ('.' [0-9]*)? ([eE] [+-]? [0-9]+)?
// -----------------------------------------------------------------------------
Token Lexer::readNumber(int startLine, int startCol) {
    std::string num;

    // Целая часть
    while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
        num += advance();
    }

    // Необязательная дробная часть
    if (!atEnd() && current() == '.') {
        num += advance();  // '.'
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
            num += advance();
        }
    }

    // Необязательная экспоненциальная часть: [eE][+-]?[0-9]+
    if (!atEnd() && (current() == 'e' || current() == 'E')) {
        num += advance();  // 'e' или 'E'

        if (!atEnd() && (current() == '+' || current() == '-')) {
            num += advance();  // знак экспоненты
        }

        if (atEnd() || !std::isdigit(static_cast<unsigned char>(current()))) {
            std::ostringstream oss;
            oss << "Ожидаются цифры после показателя экспоненты в числе \""
                << num << "\" (строка " << startLine << ", столбец " << startCol << ")";
            throw LexerError(oss.str(), startLine, startCol);
        }

        while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
            num += advance();
        }
    }

    return Token(TokenType::TK_NUMBER, num, startLine, startCol);
}

// -----------------------------------------------------------------------------
// Считывание идентификатора: [a-zA-Z][a-zA-Z0-9]*
// -----------------------------------------------------------------------------
Token Lexer::readIdent(int startLine, int startCol) {
    std::string id;
    while (!atEnd() && (std::isalpha(static_cast<unsigned char>(current()))
                     || std::isdigit(static_cast<unsigned char>(current())))) {
        id += advance();
    }
    return Token(TokenType::TK_ID, id, startLine, startCol);
}

// -----------------------------------------------------------------------------
// Чтение очередного токена из исходного текста
// -----------------------------------------------------------------------------
Token Lexer::readToken() {
    skipWhitespace();

    if (atEnd()) {
        return Token(TokenType::TK_EOF, "", line_, col_);
    }

    int  startLine = line_;
    int  startCol  = col_;
    char c         = current();

    // Одиночные операторы
    if (c == '+') { advance(); return Token(TokenType::TK_PLUS,   "+", startLine, startCol); }
    if (c == '*') { advance(); return Token(TokenType::TK_STAR,   "*", startLine, startCol); }
    if (c == '=') { advance(); return Token(TokenType::TK_ASSIGN, "=", startLine, startCol); }
    if (c == '(') { advance(); return Token(TokenType::TK_LPAREN, "(", startLine, startCol); }
    if (c == ')') { advance(); return Token(TokenType::TK_RPAREN, ")", startLine, startCol); }

    // Числовой литерал начинается с цифры
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return readNumber(startLine, startCol);
    }

    // Идентификатор начинается с буквы
    if (std::isalpha(static_cast<unsigned char>(c))) {
        return readIdent(startLine, startCol);
    }

    // Неизвестный символ — ошибка
    std::ostringstream oss;
    oss << "Неожиданный символ '" << c
        << "' (строка " << line_ << ", столбец " << col_ << ")";
    throw LexerError(oss.str(), line_, col_);
}

// -----------------------------------------------------------------------------
// Публичный API
// -----------------------------------------------------------------------------

/// Вернуть следующий токен и продвинуть позицию.
Token Lexer::nextToken() {
    if (hasPeeked_) {
        hasPeeked_ = false;
        return peeked_;
    }
    return readToken();
}

/// Подсмотреть следующий токен без продвижения (lookahead = 1).
Token Lexer::peekToken() {
    if (!hasPeeked_) {
        peeked_    = readToken();
        hasPeeked_ = true;
    }
    return peeked_;
}
