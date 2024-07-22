#ifndef clox_assert_h
#define clox_assert_h

#include <stdio.h>

#define ASSERT(expr, msg)                                                      \
    if (!(expr)) {                                                             \
        printf("ASSERTION FAILED: %s: %s\n", #expr, (msg));                    \
        exit(1);                                                               \
    }

#endif