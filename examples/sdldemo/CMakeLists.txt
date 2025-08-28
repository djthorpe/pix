
add_executable(sdldemo demo.c)

target_include_directories(sdldemo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against SDL wrapper (which itself links pix and SDL2)
target_link_libraries(sdldemo PRIVATE pixsdl)
