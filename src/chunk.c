#include <stdio.h>

#include "assert.h"
#include "chunk.h"
#include "memory.h"

void chunk_init(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    value_array_init(&chunk->constants);
    lines_init(&chunk->lines);
}

void chunk_free(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    value_array_free(&chunk->constants);
    lines_free(&chunk->lines);
    chunk_init(chunk);
}

void chunk_write(Chunk *chunk, uint8_t byte, int line) {
    // why not chunk->count == chunk->capacity?
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code =
            GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
    lines_write(&chunk->lines, line);
}

int chunk_add_constant(Chunk *chunk, Value constant) {
    value_array_write(&chunk->constants, constant);
    return chunk->constants.count - 1;
}

void lines_init(Lines *lines) {
    lines->count = 0;
    lines->capacity = 0;
    lines->entries = NULL;
}

void lines_free(Lines *lines) {
    FREE_ARRAY(LineEntry, &lines->entries, lines->count);
    lines_init(lines);
}

void lines_write(Lines *lines, int line) {
    if (lines->capacity < lines->count + 1) {
        int old_capacity = lines->capacity;
        lines->capacity = GROW_CAPACITY(old_capacity);
        lines->entries = GROW_ARRAY(LineEntry, lines->entries, old_capacity,
                                    lines->capacity);
    }

    if (lines->count > 0 && line == lines->entries[lines->count - 1].line) {
        ASSERT(lines->entries[lines->count - 1].occurrences > 0,
               "line occurrences must be greater than 0");
        lines->entries[lines->count - 1].occurrences++;
    } else {
        LineEntry newEntry = {.line = line, .occurrences = 1};
        lines->entries[lines->count] = newEntry;
        lines->count++;
    }
}

int lines_get_line(Lines *lines, int offset) {
    int offset_mirror = 0;
    for (int line_idx = 0; line_idx < lines->count; line_idx++) {
        offset_mirror += lines->entries[line_idx].occurrences;
        if (offset_mirror - 1 >= offset) {
            return lines->entries[line_idx].line;
        }
    }

    UNREACHABLE("line was not found in `Lines` array");
}
