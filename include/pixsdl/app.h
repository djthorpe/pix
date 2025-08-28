#pragma once
#include <pix/pix.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque app handle; implementation hidden from demo
typedef struct sdl_app_t sdl_app_t;

// Create/destroy the SDL app, window, renderer, texture, and frame
sdl_app_t *sdl_app_create(int width, int height, pix_format_t fmt,
                          const char *title);
void sdl_app_destroy(sdl_app_t *app);

// Access the frame for rendering
pix_frame_t *sdl_app_get_frame(sdl_app_t *app);

// Timing and control helpers (no SDL in demo)
uint32_t sdl_app_ticks(sdl_app_t *app);
void sdl_app_delay(sdl_app_t *app, uint32_t ms);
bool sdl_app_poll_should_close(sdl_app_t *app);
void sdl_app_get_size(sdl_app_t *app, int *out_w, int *out_h);

#ifdef __cplusplus
}
#endif
