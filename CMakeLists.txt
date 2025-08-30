cmake_minimum_required(VERSION 3.10)
project(pix C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Libraries
add_subdirectory(src)

# Examples (SDL based)
add_subdirectory(examples/sdldemo)
add_subdirectory(examples/tiger)
add_subdirectory(examples/sdlimage)
