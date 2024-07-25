#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassemble_chunk(Chunk *chunk, char const *name);
int disassemble_instruction(Chunk *chunk, int offset);

#endif