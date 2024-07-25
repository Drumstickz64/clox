cmake_minimum_required(VERSION 3.5.0)
project(clox VERSION 0.1)

add_executable(${PROJECT_NAME} "src/main.c"
                               "src/memory.c"
                               "src/chunk.c"
                               "src/value.c"
                               "src/vm.c"
                               "src/debug.c")

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic --pedantic-errors)
endif()
