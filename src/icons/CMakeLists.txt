## Icon library wrapping generated sources from Tabler

# Generation now outputs under src/icons/generated (Makefile: ICON_OUT_DIR=src/icons/generated)
set(GENERATED_ICON_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/generated)
set(FILLED_DIR ${GENERATED_ICON_ROOT}/filled)
set(OUTLINE_DIR ${GENERATED_ICON_ROOT}/outline)

if(NOT EXISTS ${FILLED_DIR} OR NOT EXISTS ${OUTLINE_DIR})
  message(FATAL_ERROR "Icon generation missing; run 'make icons' (expected ${FILLED_DIR} & ${OUTLINE_DIR})")
endif()

file(GLOB ICON_FILLED_SOURCES CONFIGURE_DEPENDS ${FILLED_DIR}/*.c)
file(GLOB ICON_OUTLINE_SOURCES CONFIGURE_DEPENDS ${OUTLINE_DIR}/*.c)
set(ICON_SOURCES ${ICON_FILLED_SOURCES} ${ICON_OUTLINE_SOURCES})

add_library(pixicons STATIC ${ICON_SOURCES})
target_include_directories(pixicons PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${GENERATED_ICON_ROOT}        # allow including generated headers (eg icons.h)
)

target_link_libraries(pixicons PUBLIC pix)
source_group("filled" FILES ${ICON_FILLED_SOURCES})
source_group("outline" FILES ${ICON_OUTLINE_SOURCES})
