# Tiny C Compiler (tcc)

A minimalist C compiler for Windows x64. This project aims to be a small, self-containing compiler that can generate valid Windows executables (PE files) directly, without external linkers.

## Features

- **Variables**: Global and local variables, basic data types (`int`, `char`, pointers).
- **Control Flow**: `if`, `else`, `while`, `for`, `return`.
- **Arithmetic**: Basic integer arithmetic (+, -, \*, /, %, &, |, ^, <<, >>) and comparisons.
- **Output**: Generates native Windows x64 Executable (PE) files directly.
- **Function Calls**: Windows x64 ABI support (Register passing RCX/RDX/R8/R9, Stack passing, Shadow space).
- **Design**: One-pass compilation, simple recursive descent parser, register-based code generation.

## Building

You need Microsoft Visual Studio (MSVC) or a similar C compiler environment.

Using the provided batch script:

```cmd
build.bat
```

This will create `build/tcc.exe`.

## Usage

Compile a C file to an executable:

```cmd
build\tcc.exe input.c -o output.exe
```

Example:

```cmd
build\tcc.exe tests\test_arithmetic.c -o test.exe
test.exe
echo %ERRORLEVEL%
```

## Running Tests

The `tests/` directory contains several test cases. You can compile and run them to verify the compiler:

```cmd
build\tcc.exe tests\test_while.c -o loop_test.exe
loop_test.exe
echo %ERRORLEVEL%
```

## Project Structure

- `src/tcc.c`: Main entry point and driver.
- `src/lex.c`: Lexer (tokenization).
- `src/parse.c`: Parser (syntax analysis).
- `src/gen.c`: Generic code generation logic.
- `src/x86_64-gen.c`: x64-specific code emission.
- `src/pe.c`: PE file format generation.
- `src/sym.c`: Symbol table management.
- `src/section.c`: Section memory management.

## Status

**Active Development**. Currently supports basic C constructs.
Future work includes: structs, preprocessor directives.
