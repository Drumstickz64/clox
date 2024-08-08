#include <stdio.h>

#include "assert.h"
#include "common.h"
#include "compiler.h"
#include "config.h"
#include "object.h"
#include "scanner.h"

#ifdef DEBUG_TRACE_CODE
#include "debug.h"
#endif

typedef struct Parser {
    Token prev_token;
    Token curr_token;
    bool had_error;
    bool is_panicking;
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

typedef void (*ParseFn)(bool can_assign);

typedef struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static void declaration(void);
static void var_declaration(void);
static void statement(void);
static void print_statement(void);
static void expression_statement(void);

static void parse_precedence(Precedence precedence);

static void expression(void);
static void binary(bool can_assign);
static void unary(bool can_assign);
static void grouping(bool can_assign);
static void variable(bool can_assign);
static void literal(bool can_assign);
static void number(bool can_assign);
static void string(bool can_assign);

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
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
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
    if (parser.is_panicking)
        return;

    parser.is_panicking = true;
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

static bool check(TokenType type) {
    return type == parser.curr_token.type;
}

static bool match(TokenType type) {
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

static void synchronize(void) {
    parser.is_panicking = false;

    while (parser.curr_token.type != TOKEN_EOF) {
        if (parser.prev_token.type == TOKEN_SEMICOLON) {
            return;
        }

        switch (parser.curr_token.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:;  // Do nothing.
        }

        advance();
    }
}

static void consume(TokenType type, const char* msg) {
    if (parser.curr_token.type == type) {
        advance();
    } else {
        error_at_curr(msg);
    }
}

static void declaration(void) {
    if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }

    if (parser.is_panicking) {
        synchronize();
    }
}

static uint8_t identifier_constant(Token* token) {
    return make_constant(OBJ_VAL(copy_string(token->start, token->length)));
}

static uint8_t parse_variable(const char* err_msg) {
    consume(TOKEN_IDENTIFIER, err_msg);
    return identifier_constant(&parser.prev_token);
}

static void define_variable(uint8_t global) {
    emit_byte2(OP_DEFINE_GLOBAL, global);
}

static void var_declaration(void) {
    uint8_t global = parse_variable("expected variable name");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "expected ';' at end of variable declaration");

    define_variable(global);
}

static void statement(void) {
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else {
        expression_statement();
    }
}

static void print_statement(void) {
    expression();
    consume(TOKEN_SEMICOLON, "expected ';' after value");
    emit_byte(OP_PRINT);
}

static void expression_statement(void) {
    expression();
    consume(TOKEN_SEMICOLON, "expected ';' after expression");
    emit_byte(OP_POP);
}

static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = rules[parser.prev_token.type].prefix;
    if (!prefix_rule) {
        error("expected expression");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= rules[parser.curr_token.type].precedence) {
        advance();
        ParseFn infix_rule = rules[parser.prev_token.type].infix;
        ASSERT(infix_rule != NULL, "");
        infix_rule(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        error("invalid assignment target");
    }
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void binary(bool) {
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
        case TOKEN_EQUAL_EQUAL:
            emit_byte(OP_EQUAL);
            break;
        case TOKEN_BANG_EQUAL:
            emit_byte2(OP_EQUAL, OP_NOT);
            break;
        case TOKEN_GREATER:
            emit_byte(OP_GREATER);
            break;
        case TOKEN_LESS_EQUAL:
            emit_byte2(OP_GREATER, OP_NOT);
            break;
        case TOKEN_LESS:
            emit_byte(OP_LESS);
            break;
        case TOKEN_GREATER_EQUAL:
            emit_byte2(OP_LESS, OP_NOT);
            break;
        default: {
            UNREACHABLE("encountered invalid binary operator");
        }
    }
}

static void unary(bool) {
    TokenType operatorType = parser.prev_token.type;

    parse_precedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS:
            emit_byte(OP_NEGATE);
            break;
        case TOKEN_BANG:
            emit_byte(OP_NOT);
            break;
        default:
            UNREACHABLE("encountered invalid unary operator")
    }
}

static void grouping(bool) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "expected ')' after expression");
}

static void named_variable(Token* token, bool can_assign) {
    uint8_t arg = identifier_constant(token);

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_byte2(OP_SET_GLOBAL, arg);
    } else {
        emit_byte2(OP_GET_GLOBAL, arg);
    }
}

static void variable(bool can_assign) {
    named_variable(&parser.prev_token, can_assign);
}

static void literal(bool) {
    switch (parser.prev_token.type) {
        case TOKEN_NIL:
            emit_byte(OP_NIL);
            break;
        case TOKEN_TRUE:
            emit_byte(OP_TRUE);
            break;
        case TOKEN_FALSE:
            emit_byte(OP_FALSE);
            break;
        default:
            UNREACHABLE("encountered an invalid character in literal function");
    }
}

static void number(bool) {
    double value = strtod(parser.prev_token.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string(bool) {
    emit_constant(OBJ_VAL(copy_string(parser.prev_token.start + 1,
                                      parser.prev_token.length - 2)));
}
#pragma endregion

bool compile(const char* source, Chunk* chunk) {
    scanner_init(source);
    compiling_chunk = chunk;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    end_compiler();

    return !parser.had_error;
}
