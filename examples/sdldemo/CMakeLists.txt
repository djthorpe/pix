
add_executable(sdldemo demo.c)

target_include_directories(sdldemo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against core pix (now includes SDL backend when enabled)
target_link_libraries(sdldemo PRIVATE pix)
