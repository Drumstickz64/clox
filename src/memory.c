#include "memory.h"
#include "assert.h"

void *reallocate(void *pointer, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, new_size);
    ASSERT(result != NULL, "we are able to allocate memory");
    return result;
}