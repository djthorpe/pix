add_executable(sdlimage main.c)

# Link against core pix (includes SDL backend when enabled)
target_link_libraries(sdlimage PRIVATE pix)
