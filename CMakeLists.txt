cmake_minimum_required(VERSION 3.10)
project(hagl_demo C)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(demo
    demo.c
    src/pix/frame.c
    src/sdl/frame_sdl.c
    src/sdl/sdl_app.c
    src/vg/canvas.c
    src/vg/path.c
    src/vg/transform.c
    src/vg/fill.c
    src/vg/primitives.c
)

target_include_directories(demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pix)

target_link_libraries(demo ${SDL2_LIBRARIES} m)

# --- Doxygen documentation target ---
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
else()
    message(STATUS "Doxygen not found. 'docs' target will be unavailable.")
endif()
