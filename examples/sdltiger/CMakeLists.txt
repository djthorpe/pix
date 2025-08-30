
add_executable(sdltiger main.c canvas.c tiger.c)

target_include_directories(sdltiger PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against core pix (now includes SDL backend when enabled)
target_link_libraries(sdltiger PRIVATE pix)
