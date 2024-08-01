#include <stdio.h>

#include "assert.h"
#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_TRACE_CODE
#include "debug.h"
#endif

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

typedef void (*ParseFn)();

typedef struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static void parse_precedence(Precedence precedence);

static void expression(void);
static void binary(void);
static void unary(void);
static void grouping(void);
static void number(void);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {NULL, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

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

#ifdef DEBUG_TRACE_CODE
    if (!parser.had_error)
        disassemble_chunk(curr_chunk(), "code");
#endif
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

static void consume(TokenType type, const char* msg) {
    if (parser.curr_token.type == type) {
        advance();
    } else {
        error_at_curr(msg);
    }
}

static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = rules[parser.prev_token.type].prefix;
    if (!prefix_rule) {
        error("expected expression");
        return;
    }

    prefix_rule();

    while (precedence <= rules[parser.curr_token.type].precedence) {
        advance();
        ParseFn infix_rule = rules[parser.prev_token.type].infix;
        ASSERT(infix_rule != NULL, "");
        infix_rule();
    }
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void binary(void) {
    TokenType operatorType = parser.prev_token.type;
    ParseRule* rule = &rules[operatorType];
    parse_precedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS:
            emit_byte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emit_byte(OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emit_byte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emit_byte(OP_DIVIDE);
            break;
        default: {
            UNREACHABLE(
                "encountered invalid binary operator in binary parsing "
                "function");
        }
    }
}

static void unary(void) {
    TokenType operatorType = parser.prev_token.type;

    parse_precedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS:
            emit_byte(OP_NEGATE);
            break;
        default:
            return;
    }
}

static void grouping(void) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "expected ')' after expression");
}

static void number(void) {
    Value value = strtod(parser.prev_token.start, NULL);
    emit_constant(value);
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
