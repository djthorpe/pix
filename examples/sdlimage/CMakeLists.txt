add_executable(sdlimage main.c)

# Link against SDL wrapper (includes pix)
target_link_libraries(sdlimage PRIVATE pixsdl)
