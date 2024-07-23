#ifndef clox_assert_h
#define clox_assert_h

#include <stdio.h>

#define ASSERT(expr, msg)                                                      \
    if (!(expr)) {                                                             \
        printf("ASSERTION FAILED: on %s:%d: %s: %s\n", (__FILE__), (__LINE__), \
               #expr, (msg));                                                  \
        exit(1);                                                               \
    }

#define UNREACHABLE(msg)                                                       \
    printf("Entered unreachable code: on %s:%d: %s\n", (__FILE__), (__LINE__), \
           msg);                                                               \
    exit(1);

#endif