/*
 * lexer.cpp — реализация лексического анализатора.
 *
 * Здесь написана логика «нарезки» строки на токены.
 * Принцип работы: идём по строке символ за символом,
 * смотрим что за символ и решаем — это начало числа, имени или оператора.
 *
 * Числа распознаём в трёх форматах:
 *   целые:      просто цифры             (42)
 *   дробные:    цифры, точка, цифры      (3.14)
 *   научная:    цифры [.цифры] e [+-] цифры  (1e+18, 8.41E-10)
 *
 * Имена:  буква, потом буквы или цифры  (result, a, x1)
 * Операторы: каждый один символ  (+  *  =  (  ))
 */

#include "lexer.h"

#include <cctype>   // isdigit, isalpha, isspace — проверка типа символа
#include <sstream>  // ostringstream — для формирования строк ошибок

// Возвращает читаемое название типа токена.
// Нужно только для красивых сообщений об ошибках.
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

// Конструктор: сохраняем строку, ставим позицию на начало
Lexer::Lexer(const std::string& source)
    : src_(source)
    , pos_(0)
    , line_(1)
    , col_(1)
    , hasPeeked_(false)
    , peeked_(TokenType::TK_EOF, "", 0, 0)  // пустышка, просто чтобы поле было инициализировано
{}

// =============================================================================
// Внутренние вспомогательные методы
// =============================================================================

// Вернуть символ на текущей позиции, не двигаясь вперёд.
// Если дошли до конца — вернуть нулевой символ '\0'.
bool Lexer::atEnd() const {
    return pos_ >= src_.size();
}

char Lexer::current() const {
    return atEnd() ? '\0' : src_[pos_];
}

// Вернуть текущий символ и сдвинуться на следующий.
// Если прошли перенос строки — увеличиваем счётчик строк, сбрасываем столбец.
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

// Пропустить все пробельные символы (пробел, \t, \r, \n).
// Пробелы между токенами нас не интересуют.
void Lexer::skipWhitespace() {
    while (!atEnd() && std::isspace(static_cast<unsigned char>(current()))) {
        advance();
    }
}

// Прочитать числовой литерал.
// К моменту вызова мы уже знаем что текущий символ — цифра.
// Читаем: целую часть, потом (если есть) дробную, потом (если есть) экспоненту.
Token Lexer::readNumber(int startLine, int startCol) {
    std::string num;

    // Целая часть: читаем все цифры подряд
    while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
        num += advance();
    }

    // Дробная часть: если следующий символ — точка, читаем её и цифры после
    if (!atEnd() && current() == '.') {
        num += advance();  // забираем '.'
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
            num += advance();
        }
    }

    // Экспоненциальная часть: e или E, потом опциональный знак, потом цифры
    // Пример: 1e+18  или  8.41E-10  или  5e3
    if (!atEnd() && (current() == 'e' || current() == 'E')) {
        num += advance();  // забираем 'e' или 'E'

        // Опциональный знак экспоненты
        if (!atEnd() && (current() == '+' || current() == '-')) {
            num += advance();
        }

        // После e/E ОБЯЗАТЕЛЬНО должны быть цифры — иначе это ошибка
        if (atEnd() || !std::isdigit(static_cast<unsigned char>(current()))) {
            std::ostringstream oss;
            oss << "Ожидаются цифры после показателя экспоненты в числе \""
                << num << "\" (строка " << startLine << ", столбец " << startCol << ")";
            throw LexerError(oss.str(), startLine, startCol);
        }

        // Читаем цифры степени
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
            num += advance();
        }
    }

    return Token(TokenType::TK_NUMBER, num, startLine, startCol);
}

// Прочитать идентификатор (имя переменной).
// К моменту вызова текущий символ — буква.
// Читаем буквы и цифры пока они идут подряд.
Token Lexer::readIdent(int startLine, int startCol) {
    std::string id;
    while (!atEnd() && (std::isalpha(static_cast<unsigned char>(current()))
                     || std::isdigit(static_cast<unsigned char>(current())))) {
        id += advance();
    }
    return Token(TokenType::TK_ID, id, startLine, startCol);
}

// Прочитать очередной токен из строки.
// Это основная функция лексера: пропускаем пробелы, смотрим на символ, решаем.
Token Lexer::readToken() {
    skipWhitespace();  // пробелы нам не нужны

    // Дошли до конца — возвращаем EOF-токен
    if (atEnd()) {
        return Token(TokenType::TK_EOF, "", line_, col_);
    }

    int  startLine = line_;
    int  startCol  = col_;
    char c         = current();

    // Одиночные символы-операторы: просто создаём токен нужного типа
    if (c == '+') { advance(); return Token(TokenType::TK_PLUS,   "+", startLine, startCol); }
    if (c == '*') { advance(); return Token(TokenType::TK_STAR,   "*", startLine, startCol); }
    if (c == '=') { advance(); return Token(TokenType::TK_ASSIGN, "=", startLine, startCol); }
    if (c == '(') { advance(); return Token(TokenType::TK_LPAREN, "(", startLine, startCol); }
    if (c == ')') { advance(); return Token(TokenType::TK_RPAREN, ")", startLine, startCol); }

    // Цифра — начало числа
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return readNumber(startLine, startCol);
    }

    // Буква — начало имени переменной
    if (std::isalpha(static_cast<unsigned char>(c))) {
        return readIdent(startLine, startCol);
    }

    // Ни то ни другое — непонятный символ, кидаем ошибку
    std::ostringstream oss;
    oss << "Неожиданный символ '" << c
        << "' (строка " << line_ << ", столбец " << col_ << ")";
    throw LexerError(oss.str(), line_, col_);
}

// =============================================================================
// Публичный API — именно эти методы вызывает парсер
// =============================================================================

// Взять следующий токен и сдвинуться вперёд.
// Если до этого мы «подсматривали» (peek), берём из буфера — не читаем заново.
Token Lexer::nextToken() {
    if (hasPeeked_) {
        hasPeeked_ = false;  // буфер опустел
        return peeked_;
    }
    return readToken();
}

// Посмотреть следующий токен без сдвига.
// Парсер использует это чтобы «посмотреть вперёд» и решить что делать.
// При повторном вызове возвращает тот же токен (не двигается).
Token Lexer::peekToken() {
    if (!hasPeeked_) {
        peeked_    = readToken();  // читаем и кладём в буфер
        hasPeeked_ = true;
    }
    return peeked_;  // возвращаем из буфера, не сдвигаясь
}
