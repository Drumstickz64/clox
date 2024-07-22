cmake_minimum_required(VERSION 3.15)
project(clox VERSION 0.1.0 LANGUAGES C)

add_executable(${PROJECT_NAME} "src/main.c"
                               "src/memory.c"
                               "src/chunk.c"
                               "src/debug.c")