add_executable(sdlicons icons.c)

# Link against core pix (SDL backend integrated) and pixicons
target_link_libraries(sdlicons PRIVATE pix pixicons)

target_include_directories(sdlicons PRIVATE ${CMAKE_SOURCE_DIR}/include)

set_target_properties(sdlicons PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
