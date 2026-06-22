cmake_minimum_required(VERSION 3.20)

project(KCPSM6)

include_directories(src)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

add_executable(picoterpret
        src/main.cpp
        src/cli/cli.cpp
        src/parse/lexer.cpp
        src/parse/diagnostics.cpp
)

target_include_directories(picoterpret PRIVATE src)
target_compile_options(picoterpret PRIVATE -Wall -Wextra -std=c++23)
