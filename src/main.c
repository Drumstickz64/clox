#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "chunk.h"
#include "common.h"
#include "compiling/scanner.h"
#include "debug.h"
#include "vm.h"

static void repl(void) {
    char line[1024] = {0};
    for (;;) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "could not open file '%s'\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "not enough memory to read '%s'\n", path);
        exit(74);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "could not read from file '%s'\n", path);
        exit(74);
    }

    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

static void run_file(const char* path) {
    char* source = read_file(path);
    InterpretResult result = interpret(source);
    free(source);

    switch (result) {
        case INTERPRET_COMPILE_ERROR:
            exit(65);
        case INTERPRET_RUNTIME_ERROR:
            exit(70);
        case INTERPRET_OK:
            break;
    }
}

static void test_scanning(const char* path) {
    char* source = read_file(path);

    scanner_init(source);
    int32_t line = -1;
    for (;;) {
        Token token = scanner_next_token();
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }

        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF)
            break;
    }
    free(source);
}

int main(int argc, const char* argv[]) {
    init_vm();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else if ((argc == 3) && (strcmp(argv[2], "scan") == 0)) {
        test_scanning(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path] [scan]\n");
        exit(64);
    }

    free_vm();
    return 0;
}