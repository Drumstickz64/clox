#include "chunk.h"
#include "assert.h"
#include "math.h"
#include "memory.h"

void chunk_init(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    value_array_init(&chunk->constants);
}

void chunk_free(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->count);
    value_array_free(&chunk->constants);
    chunk_init(chunk);
}

void chunk_write(Chunk *chunk, uint8_t byte, int line) {
    // why not chunk->count == chunk->capacity?
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code =
            GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);

        chunk->lines =
            GROW_ARRAY(int, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void chunk_write_constant(Chunk *chunk, Value constant, int line) {
    int index = chunk_add_constant(chunk, constant);
    ASSERT(index <= 255 * 3, "chunk has no more than 765 constants, so that a "
                             "3 byte index can to refer to them");

    if (index <= 255) {
        chunk_write(chunk, OP_CONSTANT, line);
        chunk_write(chunk, index, line);
        return;
    }

    chunk_write(chunk, OP_CONSTANT_LONG, line);
    int index1 = 255;
    int index2 = intclamp(0, index - 255, 255);
    int index3 = intclamp(0, index - 255 - 255, 255);
    chunk_write(chunk, index1, line);
    chunk_write(chunk, index2, line);
    chunk_write(chunk, index3, line);
}

int chunk_add_constant(Chunk *chunk, Value constant) {
    value_array_write(&chunk->constants, constant);
    return chunk->constants.count - 1;
}
