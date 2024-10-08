#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "common.h"
#include "compiler.h"
#include "config.h"
#include "memory.h"
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

typedef struct Local {
    Token name;
    // the scope depth of the local, -1 means the variable has not defined yet
    int depth;
    bool is_captured;
} Local;

typedef struct {
    uint8_t index;
    bool is_local;
} Upvalue;

typedef enum FunctionType {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int local_count;
    Upvalue upvalues[UINT8_COUNT];
    int scope_depth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool has_superclass;
} ClassCompiler;

static void declaration(void);
static void class_declaration(void);
static void method(void);
static void fun_declaration(void);
static void var_declaration(void);
static void statement(void);
static void print_statement(void);
static void block(void);
static void expression_statement(void);
static void if_statement(void);
static void return_statement(void);
static void while_statement(void);
static void for_statement(void);

static void parse_precedence(Precedence precedence);

static void expression(void);
static void call(bool can_assign);
static void dot(bool can_assign);
static void binary(bool can_assign);
static void and_(bool can_assign);
static void or_(bool can_assign);
static void unary(bool can_assign);
static void grouping(bool can_assign);
static void variable(bool can_assign);
static void this_(bool can_assign);
static void super_(bool can_assign);
static void literal(bool can_assign);
static void number(bool can_assign);
static void string(bool can_assign);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
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
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {super_, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

Parser parser = {0};
Compiler* current = NULL;
ClassCompiler* current_class = NULL;

static void compiler_init(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = function_new();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        // we call init compiler right parsing the function name, so we can grab
        // the name from the previous token
        current->function->name =
            copy_string(parser.prev_token.start, parser.prev_token.length);
    }

    Local* local = &compiler->locals[compiler->local_count++];
    local->depth = 0;
    local->is_captured = false;

    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static Chunk* curr_chunk(void) {
    return &current->function->chunk;
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
            "too many constants in one chunk, can only have a maximum of "
            "255 constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void emit_constant(Value value) {
    emit_byte2(OP_CONSTANT, make_constant(value));
}

static void emit_return(void) {
    if (current->type == TYPE_INITIALIZER) {
        emit_byte2(OP_GET_LOCAL, 0);  // get instance
    } else {
        emit_byte(OP_NIL);
    }

    emit_byte(OP_RETURN);
}

static ObjFunction* end_compiler(void) {
    emit_return();
    ObjFunction* function = current->function;

#ifdef DEBUG_TRACE_CODE
    if (!parser.had_error) {
        disassemble_chunk(curr_chunk(),
                          function->name ? function->name->chars : "<script>");
        printf("\n");
    }
#endif

    current = current->enclosing;
    return function;
}

static void begin_scope(void) {
    current->scope_depth++;
}

static void end_scope(void) {
    current->scope_depth--;

    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth >
               current->scope_depth) {
        if (current->locals[current->local_count - 1].is_captured) {
            emit_byte(OP_CLOSE_UPVALUE);
        } else {
            emit_byte(OP_POP);
        }
        current->local_count--;
    }
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

static uint8_t identifier_constant(Token* token) {
    return make_constant(OBJ_VAL(copy_string(token->start, token->length)));
}

static void add_local(Token name) {
    if (current->local_count == UINT8_COUNT) {
        error("too many local variables in function");
        return;
    }

    Local* local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
}

static bool identifiers_equal(Token* a, Token* b) {
    if (a->length != b->length) {
        return false;
    }

    return memcmp(a->start, b->start, a->length) == 0;
}

static void declare_variable(void) {
    if (current->scope_depth == 0) {
        return;
    }

    Token* name = &parser.prev_token;

    for (int i = current->local_count - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        bool reached_different_scope = local->depth < current->scope_depth;
        if (local->depth != -1 && reached_different_scope) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error("there is already a variable with this name in this scope");
        }
    }

    add_local(*name);
}

static uint8_t parse_variable(const char* err_msg) {
    consume(TOKEN_IDENTIFIER, err_msg);

    declare_variable();
    if (current->scope_depth > 0) {
        return 0;
    }

    return identifier_constant(&parser.prev_token);
}
static void mark_initialized(void) {
    if (current->scope_depth == 0) {
        return;
    }

    current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(uint8_t global) {
    if (current->scope_depth > 0) {
        mark_initialized();
        // no need to do anything else because the initializer expression
        // ends up with a single value on the stack and that ends up being the
        // value of the local variable
        return;
    }

    emit_byte2(OP_DEFINE_GLOBAL, global);
}

static int resolve_local(Compiler* compiler, Token* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error("can't read local variable in its own initializer");
            }
            return i;
        }
    }

    return -1;
}

static int add_upvalue(Compiler* compiler, uint8_t index, bool is_local) {
    int upvalue_index = compiler->function->upvalue_count;

    for (int i = 0; i < upvalue_index; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (compiler->function->upvalue_count == UINT8_COUNT) {
        error("too many closure variables in function");
        return 0;
    }

    compiler->upvalues[upvalue_index].is_local = is_local;
    compiler->upvalues[upvalue_index].index = index;
    compiler->function->upvalue_count++;

    return upvalue_index;
}

static int resolve_upvalue(Compiler* compiler, Token* name) {
    if (!compiler->enclosing) {
        return -1;
    }

    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void named_variable(Token name, bool can_assign) {
    uint8_t get_op, set_op;
    int arg = resolve_local(current, &name);

    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else if ((arg = resolve_upvalue(current, &name)) != -1) {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    } else {
        arg = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_byte2(set_op, (uint8_t)arg);
    } else {
        emit_byte2(get_op, (uint8_t)arg);
    }
}

static void declaration(void) {
    if (match(TOKEN_CLASS)) {
        class_declaration();
    } else if (match(TOKEN_FUN)) {
        fun_declaration();
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }

    if (parser.is_panicking) {
        synchronize();
    }
}

static void function(FunctionType type) {
    Compiler compiler;
    compiler_init(&compiler, type);
    begin_scope();  // end_scope() not needed because we end the whole compiler

    consume(TOKEN_LEFT_PAREN, "expected '(' after function name");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                error_at_curr(
                    "can't have more than 255 parameters in a function");
            }

            uint8_t constant = parse_variable("expected parameter name");
            define_variable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "expected ')' after function parameters");
    consume(TOKEN_LEFT_BRACE, "expected '{' before function body");
    block();

    ObjFunction* function = end_compiler();

    emit_byte2(OP_CLOSURE, make_constant(OBJ_VAL(function)));

    for (int i = 0; i < compiler.function->upvalue_count; i++) {
        Upvalue* upvalue = &compiler.upvalues[i];
        emit_byte(upvalue->is_local ? 1 : 0);
        emit_byte(upvalue->index);
    }
}

static Token synthetic_token(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void class_declaration(void) {
    consume(TOKEN_IDENTIFIER, "expected class name");
    Token class_name = parser.prev_token;
    uint8_t name_constant = identifier_constant(&parser.prev_token);

    declare_variable();
    emit_byte2(OP_CLASS, name_constant);
    define_variable(name_constant);

    ClassCompiler class_compiler = {
        .enclosing = current_class,
        .has_superclass = false,
    };
    current_class = &class_compiler;

    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "expected superclass name");
        variable(false);

        if (identifiers_equal(&class_name, &parser.prev_token)) {
            error("a class can't inherit from itself");
        }

        begin_scope();
        add_local(synthetic_token("super"));
        define_variable(0);

        named_variable(class_name, false);
        emit_byte(OP_INHERIT);
        class_compiler.has_superclass = true;
    }

    named_variable(class_name, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emit_byte(OP_POP);

    if (class_compiler.has_superclass) {
        end_scope();
    }
    current_class = current_class->enclosing;
}

static void method(void) {
    consume(TOKEN_IDENTIFIER, "expected method name");
    uint8_t name = identifier_constant(&parser.prev_token);

    FunctionType type = TYPE_METHOD;
    if (parser.prev_token.length == 4 &&
        memcmp(parser.prev_token.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);
    emit_byte2(OP_METHOD, name);
}

static void fun_declaration(void) {
    uint8_t global = parse_variable("expected function name");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
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
    } else if (match(TOKEN_IF)) {
        if_statement();
    } else if (match(TOKEN_RETURN)) {
        return_statement();
    } else if (match(TOKEN_WHILE)) {
        while_statement();
    } else if (match(TOKEN_FOR)) {
        for_statement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else {
        expression_statement();
    }
}

static void print_statement(void) {
    expression();
    consume(TOKEN_SEMICOLON, "expected ';' after value");
    emit_byte(OP_PRINT);
}

static void block(void) {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "expected '}' after block");
}

static void expression_statement(void) {
    expression();
    consume(TOKEN_SEMICOLON, "expected ';' after expression");
    emit_byte(OP_POP);
}

static int emit_jump(uint8_t jump_op) {
    emit_byte(jump_op);
    emit_byte(0xFF);
    emit_byte(0xFF);
    return curr_chunk()->count - 2;
}

static void patch_jump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    int jump = curr_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("too much code to jump over");
    }

    uint8_t upper_byte = (jump >> 8) & 0xff;
    uint8_t lower_byte = jump & 0xFF;
    curr_chunk()->code[offset] = upper_byte;
    curr_chunk()->code[offset + 1] = lower_byte;
}

static void if_statement(void) {
    consume(TOKEN_LEFT_PAREN, "expected '(' after 'if'");
    expression();  //> expr
    consume(TOKEN_RIGHT_PAREN, "expected ')' after expression in if statement");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);  //> expr jumpf off1 off2

    // pop the value of the condition expression if the then branch executes
    emit_byte(OP_POP);  //> expr jumpf off1 off2 pop
    //> expr jumpf off1 off2 pop stmt
    statement();

    //> expr jumpf off1 off2 pop then_stmt jump off1 off2
    int else_jump = emit_jump(OP_JUMP);

    //> expr jumpf 00 05 pop then_stmt jump off1 off2
    patch_jump(then_jump);

    // pop the value of the condition expression if the else branch executes
    // this is not included with the rest of the else branch because it needs to
    // execute even if there is no explicit else
    //> expr jumpf 00 05 pop then_stmt jump off1 off2 pop
    emit_byte(OP_POP);
    if (match(TOKEN_ELSE)) {
        //> expr jumpf 00 05 pop then_stmt jump off1 off2 pop else_statement
        statement();
    }

    //> expr jumpf 00 05 pop then_stmt jump 00 02 pop else_statement ...rest
    patch_jump(else_jump);
}

static void return_statement(void) {
    if (current->type == TYPE_SCRIPT) {
        error("can't return from top-level code");
    }

    if (match(TOKEN_SEMICOLON)) {
        emit_return();
        return;
    }

    if (current->type == TYPE_INITIALIZER) {
        error("can't return a value from an initializer");
    }

    expression();
    consume(TOKEN_SEMICOLON, "expected ';' after expression");
    emit_byte(OP_RETURN);
}

static void emit_loop(int loop_start) {
    emit_byte(OP_LOOP);

    // +2 because we need to jump after the offset bytes
    int offset = curr_chunk()->count + 2 - loop_start;
    if (offset > UINT16_MAX) {
        error("loop body is too large");
    }

    emit_byte((offset >> 8) & 0xFF);
    emit_byte(offset & 0xFF);
}

static void while_statement(void) {
    int loop_start = curr_chunk()->count;

    consume(TOKEN_LEFT_PAREN, "expected '(' after 'while'");
    expression();
    consume(TOKEN_RIGHT_PAREN,
            "expected ')' after expression in while statement");

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void for_statement(void) {
    // for ((var_decl | expr_stmt); expr; stmt) stmt
    // diagram in img/for-diagram.png

    begin_scope();

    consume(TOKEN_LEFT_PAREN, "expected '(' after 'for'");

    // initializer
    if (match(TOKEN_SEMICOLON)) {
        // no initializer
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        expression_statement();
    }

    // condition
    int loop_start = curr_chunk()->count;
    int exit_jump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "expected ';' after 'for' condition clause");

        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(OP_JUMP);

        int increment_start = curr_chunk()->count;
        expression();
        consume(TOKEN_RIGHT_PAREN, "expected ')' after for clauses");
        emit_byte(OP_POP);

        emit_loop(loop_start);
        loop_start = increment_start;

        patch_jump(body_jump);
    }

    statement();

    emit_loop(loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }

    end_scope();
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

static void expression(void) {
    parse_precedence(PREC_ASSIGNMENT);
}

static uint8_t argument_list(void) {
    uint8_t arg_count = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (arg_count == 255) {
                error("can't have more than 255 arguments");
            }
            arg_count++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "expected ')' after arguments");
    return arg_count;
}

static void call(bool can_assign) {
    UNUSED(can_assign);
    uint8_t arg_count = argument_list();
    emit_byte2(OP_CALL, arg_count);
}

static void dot(bool can_assign) {
    consume(TOKEN_IDENTIFIER, "expected property name after '.'");
    uint8_t name_constant = identifier_constant(&parser.prev_token);

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_byte2(OP_SET_PROPERTY, name_constant);
    } else if (match(TOKEN_LEFT_PAREN)) {
        uint8_t arg_count = argument_list();
        emit_byte2(OP_INVOKE, name_constant);
        emit_byte(arg_count);
    } else {
        emit_byte2(OP_GET_PROPERTY, name_constant);
    }
}

static void binary(bool can_assign) {
    UNUSED(can_assign);

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

static void and_(bool can_assign) {
    UNUSED(can_assign);
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static void or_(bool can_assign) {
    UNUSED(can_assign);
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(OP_JUMP);
    patch_jump(else_jump);

    emit_byte(OP_POP);
    parse_precedence(PREC_OR);

    patch_jump(end_jump);
}

static void unary(bool can_assign) {
    UNUSED(can_assign);
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

static void grouping(bool can_assign) {
    UNUSED(can_assign);
    expression();
    consume(TOKEN_RIGHT_PAREN, "expected ')' after expression");
}

static void variable(bool can_assign) {
    named_variable(parser.prev_token, can_assign);
}

static void this_(bool can_assign) {
    UNUSED(can_assign);

    if (current_class == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }

    variable(false);
}

void super_(bool can_assign) {
    UNUSED(can_assign);

    if (current_class == NULL) {
        error("can't use 'super' outside of a class");
    } else if (!current_class->has_superclass) {
        error("can't use 'super' in a class with no superclass");
    }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifier_constant(&parser.prev_token);

    named_variable(synthetic_token("this"), false);

    if (match(TOKEN_LEFT_PAREN)) {
        uint8_t arg_count = argument_list();
        named_variable(synthetic_token("super"), false);
        emit_byte2(OP_SUPER_INVOKE, name);
        emit_byte(arg_count);
    } else {
        named_variable(synthetic_token("super"), false);
        emit_byte2(OP_GET_SUPER, name);
    }
}

static void literal(bool can_assign) {
    UNUSED(can_assign);
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

static void number(bool can_assign) {
    UNUSED(can_assign);
    double value = strtod(parser.prev_token.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string(bool can_assign) {
    UNUSED(can_assign);
    emit_constant(OBJ_VAL(copy_string(parser.prev_token.start + 1,
                                      parser.prev_token.length - 2)));
}
#pragma endregion

ObjFunction* compile(const char* source) {
    scanner_init(source);

    Compiler compiler;
    compiler_init(&compiler, TYPE_SCRIPT);

    parser.had_error = false;
    parser.is_panicking = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = end_compiler();

    return parser.had_error ? NULL : function;
}

void mark_compiler_roots(void) {
    Compiler* compiler = current;
    while (compiler != NULL) {
        mark_object((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
