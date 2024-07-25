#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

typedef struct VM {
    Chunk *chunk;
    uint8_t *ip;
} VM;

typedef enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void init_vm(void);
void free_vm(void);
InterpretResult interpret(Chunk *chunk);

#endif