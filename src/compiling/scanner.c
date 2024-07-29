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

static bool isAtEnd() {
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
    if (isAtEnd())
        return false;

    if (*scanner.curr != target)
        return false;

    scanner.curr++;
    return true;
}

static char peek() {
    ASSERT(!isAtEnd(), "peek() will not be called at the end of file");

    return *scanner.curr;
}

static char peek_next() {
    if (isAtEnd())
        return '\0';

    return scanner.curr[1];
}

static void drop_line() {
    while (!isAtEnd() && peek() != '\n')
        advance();
}

static void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/')
                    drop_line();
                break;
            default:
                return;
        }
    }
}

static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n')
            scanner.line++;
        advance();
    }

    if (isAtEnd())
        return token_error("unterminated string literal");

    advance();  // consume closing "
    return make_token(TOKEN_STRING);
}

void scanner_init(const char* source) {
    scanner.start = source;
    scanner.curr = source;
    scanner.line = 1;
}

Token scanner_next_token() {
    skip_whitespace();
    scanner.start = scanner.curr;

    if (isAtEnd())
        return make_token(TOKEN_EOF);

    char c = advance();

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
