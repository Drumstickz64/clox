cmake_minimum_required(VERSION 3.5.0)
project(clox VERSION 0.1)

add_executable(${PROJECT_NAME} "src/main.c"
                               "src/memory.c"
                               "src/chunk.c"
                               "src/value.c"
                               "src/vm.c"
                               "src/compiling/scanner.c"
                               "src/compiling/compiler.c"
                               "src/debug.c")

target_include_directories(${PROJECT_NAME} PRIVATE "src" "${PROJECT_BINARY_DIR}/src")

option(DEBUG_TRACE_CODE OFF)
option(DEBUG_TRACE_EXECUTION OFF)

configure_file("src/config.h.in" "src/config.h")

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic --pedantic-errors)
endif()
