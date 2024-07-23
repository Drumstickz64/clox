#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum OpCode { OP_CONSTANT, OP_RETURN } OpCode;

typedef struct Chunk {
    int count;
    int capacity;
    uint8_t *code;
    ValueArray constants;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte);
int chunk_add_constant(Chunk *chunk, Value constant);

#endif
