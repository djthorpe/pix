# Icon library wrapping generated sources from Tabler

# Expect generated sources under ${CMAKE_SOURCE_DIR}/generated/icons
set(GENERATED_ICON_ROOT ${CMAKE_SOURCE_DIR}/generated/icons)
set(FILLED_DIR ${GENERATED_ICON_ROOT}/filled)
set(OUTLINE_DIR ${GENERATED_ICON_ROOT}/outline)

if(NOT EXISTS ${FILLED_DIR} OR NOT EXISTS ${OUTLINE_DIR})
  message(FATAL_ERROR "Icon generation missing; run 'make icons' (filled/outline dirs not found)")
endif()

file(GLOB ICON_FILLED_SOURCES ${FILLED_DIR}/*.c)
file(GLOB ICON_OUTLINE_SOURCES ${OUTLINE_DIR}/*.c)
set(ICON_SOURCES ${ICON_FILLED_SOURCES} ${ICON_OUTLINE_SOURCES})

add_library(pixicons STATIC ${ICON_SOURCES})
# Include root so includes like <generated/icons/filled/icons.h> work
# and public library headers remain accessible.
target_include_directories(pixicons PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/generated
)

target_link_libraries(pixicons PUBLIC pix)

source_group("filled" FILES ${ICON_FILLED_SOURCES})
source_group("outline" FILES ${ICON_OUTLINE_SOURCES})
