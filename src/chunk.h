#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

typedef enum OpCode { OP_RETURN } OpCode;

typedef struct Chunk {
    int count;
    int capacity;
    uint8_t *code;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte);

#endif
