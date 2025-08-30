add_library(pix STATIC
    pix/frame.c
    pix/rgb24.c
    pix/rgba32.c
    pix/grey8.c
    pix/rgb565.c
    pix/jpeg.c
    vg/canvas.c
    vg/shape.c
    vg/path.c
    vg/transform.c
    vg/fill.c
    vg/primitives.c
    vg/font.c
    ../third_party/tjpgd3/src/tjpgd.c
)
target_include_directories(pix PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include  
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tjpgd3/src
)

# Include SDL2 backend
find_package(SDL2)
if (SDL2_FOUND)
    target_sources(pix PRIVATE pix/sdl.c)
    target_include_directories(pix PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(pix PUBLIC ${SDL2_LIBRARIES} m)
    target_compile_definitions(pix PUBLIC PIX_ENABLE_SDL)
endif()

# Linux framebuffer backend
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(pix PUBLIC PIX_ENABLE_FB)
    target_sources(pix PRIVATE pix/fb.c)
endif()

# Add icons
add_subdirectory(icons)
