# CLox

An implementation of clox from the "Crafting Interpreters" book

## Building

To build CLox you need a C compiler and CMake

```shell
cmake -S . -B build
cmake --build build
```

## Testing

You need NodeJs to run the integration tests

```shell
node runTests.mjs <path-to-binary>
```
