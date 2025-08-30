/**
 * @defgroup pix Pixel Frame Core
 * @brief Core pixel types, formats, colors, and frame interface.
 *
 * Fundamental pixel abstractions used by higher level modules.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup pix
 *  Pixel formats for frames. */
typedef enum {
  PIX_FMT_UNKNOWN = 0,
  PIX_FMT_RGB24,
  PIX_FMT_RGBA32,
  PIX_FMT_GRAY8,
  PIX_FMT_RGB565,
} pix_format_t;

/** @ingroup pix
 * Integer points (can be negative). */
typedef struct pix_point_t {
  int16_t x;
  int16_t y;
} pix_point_t;

/** @ingroup pix
 * Integer width and height. */
typedef struct pix_size_t {
  uint16_t w;
  uint16_t h;
} pix_size_t;

/** @ingroup pix
 * Non-negative scalar pixel measure (e.g. radius, length). */
typedef uint16_t pix_scalar_t;

/** @ingroup pix
 * Color (0xAARRGGBB format). */
typedef uint32_t pix_color_t;

/** @ingroup pix
 * NONE color constant disables a paint. */
#define PIX_COLOR_NONE ((pix_color_t)0)

#ifdef __cplusplus
}
#endif

#include "frame.h"
#include "image.h"
#ifdef PIX_ENABLE_SDL
#include "sdl.h"
#endif
