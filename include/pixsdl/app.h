/**
 * @file pixsdl/app.h
 * @ingroup pixsdl
 * @brief Opaque SDL application wrapper (window/renderer/texture lifecycle).
 */
#pragma once
#include <pix/frame.h>
#include <pix/pix.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque app handle; implementation hidden from demo
typedef struct sdl_app_t sdl_app_t;

// Create/destroy the SDL app, window, renderer, texture, and frame
/** @ingroup pixsdl */
sdl_app_t *sdl_app_create(int width, int height, pix_format_t fmt,
                          const char *title);
/** @ingroup pixsdl */
void sdl_app_destroy(sdl_app_t *app);

// Access the frame for rendering
/** @ingroup pixsdl */
pix_frame_t *sdl_app_get_frame(sdl_app_t *app);

// Timing and control helpers (no SDL in demo)
/** @ingroup pixsdl */
uint32_t sdl_app_ticks(sdl_app_t *app);
/** @ingroup pixsdl */
void sdl_app_delay(sdl_app_t *app, uint32_t ms);
/** @ingroup pixsdl */
bool sdl_app_poll_should_close(sdl_app_t *app);
/** @ingroup pixsdl */
void sdl_app_get_size(sdl_app_t *app, int *out_w, int *out_h);

#ifdef __cplusplus
}
#endif
