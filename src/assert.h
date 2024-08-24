#ifndef clox_assert_h
#define clox_assert_h

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef DEBUG_ENABLE_ASSERT
#define ASSERT(expr, assertion)                                \
    {                                                          \
        if (!(expr)) {                                         \
            printf(                                            \
                "\n========== ASSERTION FAILED ==========\n"   \
                "LOCATION: %s:%d\n"                            \
                "EXPRESSION: %s\n"                             \
                "EXPECTED THAT: %s\n",                         \
                (__FILE__), (__LINE__), (#expr), (assertion)); \
            exit(1);                                           \
        }                                                      \
    }
#else
#define ASSERT(expr, assert) \
    {}
#endif

#define UNREACHABLE(msg)                                       \
    {                                                          \
        printf(                                                \
            "========== ENTERED UNREACHABLE CODE ==========\n" \
            "LOCATION: %s:%d\n"                                \
            "MESSAGE: %s\n",                                   \
            (__FILE__), (__LINE__), (msg));                    \
        exit(1);                                               \
    }

#endif
