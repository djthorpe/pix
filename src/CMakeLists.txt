add_library(pix STATIC
    pix/frame.c
    vg/canvas.c
    vg/path.c
    vg/transform.c
    vg/fill.c
    vg/primitives.c
)
target_include_directories(pix PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include        # for <pix/pix.h> <vg/vg.h> and <pixsdl/pixsdl.h>
)

find_package(SDL2 REQUIRED)

add_library(pixsdl STATIC
    pixsdl/frame.c
    pixsdl/app.c
)
target_link_libraries(pixsdl PUBLIC pix ${SDL2_LIBRARIES} m)
target_include_directories(pixsdl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include        # public pix headers
    ${SDL2_INCLUDE_DIRS}
)

# Propagate required compile defs if any in future
target_compile_features(pix PUBLIC c_std_99)
target_compile_features(pixsdl PUBLIC c_std_99)
