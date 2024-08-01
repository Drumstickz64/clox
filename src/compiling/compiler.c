#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

typedef struct Parser {
    Token prev_token;
    Token curr_token;
    bool had_error;
    bool is_panicing;
} Parser;

typedef enum Precedence {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

Parser parser = {0};
Chunk* compiling_chunk = {0};

static Chunk* curr_chunk() {
    return compiling_chunk;
}

static void error_at(Token* token, const char* message) {
    if (parser.is_panicing)
        return;

    parser.is_panicing = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type != TOKEN_ERROR) {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error_at_curr(const char* msg) {
    error_at(&parser.curr_token, msg);
}

static void error(const char* msg) {
    error_at(&parser.prev_token, msg);
}

#pragma region compiling
static void emit_byte(uint8_t byte) {
    chunk_write(curr_chunk(), byte, parser.prev_token.line);
}

static void emit_byte2(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static uint8_t make_constant(Value value) {
    int constant = chunk_add_constant(curr_chunk(), value);
    if (constant > UINT8_MAX) {
        error(
            "too many constants in one chunk, can only have a maximum of 255 "
            "constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void emit_constant(Value value) {
    emit_byte2(OP_CONSTANT, make_constant(value));
}

static void emit_return(void) {
    emit_byte(OP_RETURN);
}

static void end_compiler(void) {
    emit_return();
}
#pragma endregion

#pragma region parsing
static void advance(void) {
    parser.prev_token = parser.curr_token;

    for (;;) {
        parser.curr_token = scanner_next_token();
        if (parser.curr_token.type != TOKEN_ERROR) {
            break;
        } else {
            error_at_curr(parser.curr_token.start);
            continue;
        }
    }
}

static void expression(void);

static void consume(TokenType type, const char* msg) {
    if (parser.curr_token.type == type) {
        advance();
    } else {
        error_at_curr(msg);
    }
}

static void number(void) {
    Value value = strtod(parser.curr_token.start, NULL);
    emit_constant(value);
}

static void grouping(void) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "expected ')' after expression");
}

static void unary(void) {
    TokenType operator= parser.prev_token.type;

    parse_precedence(PREC_UNARY);

    switch (operator) {
        case TOKEN_MINUS:
            emit_byte(OP_NEGATE);
            break;
        default:
            return;
    }
}

static void parse_precedence(Precedence precedence) {}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}
#pragma endregion

bool compile(const char* source, Chunk* chunk) {
    scanner_init(source);
    compiling_chunk = chunk;

    advance();
    expression();

    consume(TOKEN_EOF, "expected end of expression");
    end_compiler();

    return !parser.had_error;
}
