#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct Scanner {
    const char* start;
    const char* curr;
    int32_t line;
} Scanner;

Scanner scanner;

void scanner_init(const char* source) {
    scanner.start = source;
    scanner.curr = source;
    scanner.line = 1;
}

static bool isAtEnd() {
    return *scanner.curr == '\0';
}

static Token scanner_make_token(TokenType type) {
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

Token scanner_next_token() {
    scanner.start = scanner.curr;

    if (isAtEnd())
        return scanner_make_token(TOKEN_EOF);

    return token_error("unexpected character");
}
