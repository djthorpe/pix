cmake_minimum_required(VERSION 3.10)
project(pix C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Libraries (including generated icons)
add_subdirectory(src)

# Examples (SDL based)
if (SDL2_FOUND)
	add_subdirectory(examples/sdldemo)
	add_subdirectory(examples/sdltiger)
	add_subdirectory(examples/sdlimage)
	add_subdirectory(examples/sdlicons)
else()
	message(STATUS "SDL2 not found: SDL examples disabled")
endif()
