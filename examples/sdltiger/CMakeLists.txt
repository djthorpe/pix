
add_executable(sdltiger main.c canvas.c tiger.c)

target_include_directories(sdltiger PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against SDL wrapper (which itself links pix and SDL2)
target_link_libraries(sdltiger PRIVATE pixsdl)
