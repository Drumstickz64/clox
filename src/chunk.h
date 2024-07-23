#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum OpCode { OP_CONSTANT, OP_RETURN } OpCode;

typedef struct LineEntry {
    int line;
    int occurrences;
} LineEntry;

typedef struct Lines {
    int count;
    int capacity;
    LineEntry *entries;
} Lines;

typedef struct Chunk {
    int count;
    int capacity;
    uint8_t *code;
    Lines lines;
    ValueArray constants;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, int line);
int chunk_add_constant(Chunk *chunk, Value constant);

void lines_init(Lines *lines);
void lines_free(Lines *lines);
void lines_write(Lines *lines, int line);
int lines_get_line(Lines *lines, int offset);

#endif
