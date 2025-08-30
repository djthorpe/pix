if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Reuse the shared tiger geometry / canvas implementation from sdltiger to avoid duplication.
  add_executable(fbtiger
    main.c
    ../sdltiger/tiger.c
    ../sdltiger/canvas.c
  )
  target_include_directories(fbtiger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../sdltiger
  )
  target_link_libraries(fbtiger PRIVATE pix m)
endif()
