#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum OpCode { OP_CONSTANT, OP_CONSTANT_LONG, OP_RETURN } OpCode;

typedef struct Chunk {
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    ValueArray constants;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, int line);
int chunk_add_constant(Chunk *chunk, Value constant);
void chunk_write_constant(Chunk *chunk, Value constant, int line);

#endif
