cmake_minimum_required(VERSION 3.16)
project(tcc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Source files
set(TCC_SOURCES
    src/tcc.c
    src/lex.c
    src/parse.c
    src/sym.c
    src/gen.c
    src/x86_64-gen.c
    src/pe.c
    src/section.c
    src/utils.c
)

# Main executable
add_executable(tcc ${TCC_SOURCES})

# Include directories
target_include_directories(tcc PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Output directory
set_target_properties(tcc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
