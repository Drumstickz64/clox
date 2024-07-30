#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "common.h"
#include "scanner.h"

typedef struct Scanner {
    const char* start;
    const char* curr;
    int32_t line;
} Scanner;

Scanner scanner;

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_at_end() {
    return *scanner.curr == '\0';
}

static Token make_token(TokenType type) {
    Token token = {
        .type = type,
        .line = scanner.line,
        .start = scanner.start,
        .length = (int32_t)(scanner.curr - scanner.start),
    };

    return token;
}

static Token token_error(const char* msg) {
    Token token = {
        .type = TOKEN_ERROR,
        .line = scanner.line,
        .start = msg,
        .length = (int32_t)strlen(msg),
    };

    return token;
}

static char advance() {
    scanner.curr++;
    return scanner.curr[-1];
}

static bool match(char target) {
    if (is_at_end())
        return false;

    if (*scanner.curr != target)
        return false;

    scanner.curr++;
    return true;
}

static char peek() {
    return *scanner.curr;
}

static char peek_next() {
    if (is_at_end())
        return '\0';

    return scanner.curr[1];
}

static void drop_line() {
    while (!is_at_end() && peek() != '\n')
        advance();
}

static void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    drop_line();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n')
            scanner.line++;
        advance();
    }

    if (is_at_end())
        return token_error("unterminated string literal");

    advance();  // consume closing "
    return make_token(TOKEN_STRING);
}

static Token number() {
    while (is_digit(peek()))
        advance();

    if (peek() == '.' && is_digit(peek_next())) {
        advance();  // consume the .

        while (is_digit(peek()))
            advance();
    }

    return make_token(TOKEN_NUMBER);
}

static TokenType check_keyword(size_t span_start,
                               size_t span_len,
                               const char* rest,
                               TokenType type) {
    size_t word_len = scanner.curr - scanner.start;
    if (span_start + span_len == word_len &&
        memcmp(scanner.start + span_start, rest, span_len) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifier_type() {
    switch (scanner.start[0]) {
        case 'a':
            return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c':
            return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e':
            return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'i':
            return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n':
            return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o':
            return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p':
            return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 'v':
            return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w':
            return check_keyword(1, 4, "hile", TOKEN_WHILE);
        case 'f':
            if (scanner.curr - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return check_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 't':
            if (scanner.curr - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h':
                        return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
    }

    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while (is_alpha(peek()) || is_digit(peek()))
        advance();

    return make_token(identifier_type());
}

void scanner_init(const char* source) {
    scanner.start = source;
    scanner.curr = source;
    scanner.line = 1;
}

Token scanner_next_token() {
    skip_whitespace();
    scanner.start = scanner.curr;

    if (is_at_end())
        return make_token(TOKEN_EOF);

    char c = advance();

    if (is_digit(c))
        return number();

    if (is_alpha(c))
        return identifier();

    switch (c) {
        case '(':
            return make_token(TOKEN_LEFT_PAREN);
        case ')':
            return make_token(TOKEN_RIGHT_PAREN);
        case '{':
            return make_token(TOKEN_LEFT_BRACE);
        case '}':
            return make_token(TOKEN_RIGHT_BRACE);
        case ';':
            return make_token(TOKEN_SEMICOLON);
        case ',':
            return make_token(TOKEN_COMMA);
        case '.':
            return make_token(TOKEN_DOT);
        case '-':
            return make_token(TOKEN_MINUS);
        case '+':
            return make_token(TOKEN_PLUS);
        case '/':
            return make_token(TOKEN_SLASH);
        case '*':
            return make_token(TOKEN_STAR);
        case '!':
            return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return string();
    }

    return token_error("unexpected character");
}
