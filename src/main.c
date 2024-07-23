#include <stdio.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    Chunk chunk;
    chunk_init(&chunk);

    int constant = chunk_add_constant(&chunk, 1.2);
    chunk_write(&chunk, OP_CONSTANT);
    chunk_write(&chunk, constant);

    chunk_write(&chunk, OP_RETURN);

    disassembleChunk(&chunk, "test chunk");

    chunk_free(&chunk);
    return 0;
}
