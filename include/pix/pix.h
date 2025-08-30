#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Pixel formats for frames. */
typedef enum {
  PIX_FMT_UNKNOWN = 0,
  PIX_FMT_RGB24,
  PIX_FMT_RGBA32,
  PIX_FMT_GRAY8,
  PIX_FMT_RGB565, /**< 16-bit 5:6:5 little-endian */
} pix_format_t;

/** Integer points (can be negative) */
typedef struct pix_point_t {
  int16_t x;
  int16_t y;
} pix_point_t;

/** Integer width and height */
typedef struct pix_size_t {
  uint16_t w;
  uint16_t h;
} pix_size_t;

/** Non-negative scalar pixel measure (e.g. radius, length). */
typedef uint16_t pix_scalar_t;

/** Color (0xAARRGGBB format) */
typedef uint32_t pix_color_t;

#ifdef __cplusplus
}
#endif

#include "frame.h"
#include "image.h"
