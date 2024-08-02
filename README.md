# CLox

An implementation of clox from the "Crafting Interpreters" book

## Building

To build CLox you need a C compiler and CMake

```shell
cmake -S . -B build
cmake --build build
```

## Testing

You need NodeJs to run the integration tests, also make sure to have run CMake configuration

```shell
node runTests.mjs <path-to-build-directory> <path-to-binary>
```

For example:

```shell
node runTests.mjs build/gcc/ build/gcc/clox.exe
```

If a test fails, it outputs an `output.txt` in the directory of the failed test for easy diffing
