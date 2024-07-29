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

void scanner_init(const char* source) {
    scanner.start = source;
    scanner.curr = source;
    scanner.line = 1;
}

Token scanner_next_token() {
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
    }

    return token_error("unexpected character");
}
