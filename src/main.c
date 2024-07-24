#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "value.h"

int main(void) {
    Chunk chunk;
    chunk_init(&chunk);

    for (int i = 0; i < 766; i++) {
        chunk_write_constant(&chunk, (Value)i * 2, 123);
    }

    // chunk_write_constant(&chunk, (Value)766 * 2, 123); // invalid

    chunk_write(&chunk, OP_RETURN, 123);

    disassembleChunk(&chunk, "test chunk");

    chunk_free(&chunk);
    return 0;
}
