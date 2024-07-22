#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassembleChunk(Chunk *chunk, char const *name);
int disassembleInstruction(Chunk *chunk, int offset);

#endif