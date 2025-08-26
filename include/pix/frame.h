/**
 * @file include/pix/frame.h
 * @brief Pixel frame abstraction used by the software renderer and backends.
 *
 * This header defines a minimal, backend-agnostic pixel frame interface
 * (pix_frame_t) that provides access to a 2D pixel buffer and a few primitive
 * drawing operations. Backends (e.g., SDL) populate the function pointers to
 * implement lock/unlock, set_pixel, and draw_line. The software vector
 * renderer builds on this API to render fills and strokes into the frame.
 *
 * Coordinates are zero-based with the origin at the top-left.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported pixel formats for a pix_frame_t.
 */
typedef enum {
  PIX_FMT_UNKNOWN = 0, /**< Unspecified or unsupported format. */
  PIX_FMT_RGB888,      /**< 24-bit RGB, 8 bits per channel, no alpha. */
  PIX_FMT_RGBA8888,    /**< 32-bit RGBA, 8 bits per channel (0xAARRGGBB). */
  PIX_FMT_GRAY8,       /**< 8-bit grayscale. */
} pix_format_t;

typedef struct pix_frame_t pix_frame_t;
/**
 * @brief A 2D pixel buffer and drawing interface.
 *
 * Backends should fill in the function pointers. The software renderer uses
 * set_pixel for composition and draw_line for fast hairline rendering where
 * available; otherwise it falls back to its own routines.
 */
struct pix_frame_t {
  void *pixels;  /**< Pointer to the first pixel (may be NULL until locked). */
  size_t width;  /**< Width in pixels. */
  size_t height; /**< Height in pixels. */
  size_t stride; /**< Bytes per row (>= width * bytes_per_pixel). */
  pix_format_t format; /**< Pixel format of the buffer. */
  void *user; /**< Backend-specific context (e.g., SDL renderer/texture). */

  /**
   * @brief Acquire access to the frame's pixel buffer.
   * @param frame Frame to lock.
   * @return true on success, false on failure.
   *
   * Must be called before any drawing that touches pixels. Implementations may
   * map or allocate a CPU-accessible buffer. Nested locks are not supported.
   */
  bool (*lock)(pix_frame_t *frame);

  /**
   * @brief Release the pixel buffer and present it if applicable.
   * @param frame Frame to unlock.
   *
   * After unlock, the pixels pointer may become invalid. Backends typically
   * upload the updated buffer (e.g., SDL texture) and present to the screen.
   */
  void (*unlock)(pix_frame_t *frame);

  /**
   * @brief Set a pixel with straight-alpha src-over blending.
   * @param frame Frame to draw into (must be locked).
   * @param x X coordinate [0, width).
   * @param y Y coordinate [0, height).
   * @param color 0xAARRGGBB.
   */
  void (*set_pixel)(pix_frame_t *frame, size_t x, size_t y, uint32_t color);

  /**
   * @brief Draw a line segment (backend may implement hardware acceleration).
   * @param frame Frame to draw into (must be locked).
   * @param x0,y0 Start point.
   * @param x1,y1 End point.
   * @param color 0xAARRGGBB.
   */
  void (*draw_line)(pix_frame_t *frame, size_t x0, size_t y0, size_t x1,
                    size_t y1, uint32_t color);
};

/**
 * @name Default implementations
 * @brief Software fallbacks usable by backends.
 * @{ */
/** Set a pixel with straight-alpha src-over blending (0xAARRGGBB). */
void pix_frame_set_pixel(pix_frame_t *frame, size_t x, size_t y,
                         uint32_t color);
/** Draw a line using integer Bresenham (no anti-aliasing). */
void pix_frame_draw_line(pix_frame_t *frame, size_t x0, size_t y0, size_t x1,
                         size_t y1, uint32_t color);
/** Fill the entire frame with a solid value.
 * For byte-addressable formats, value should be a packed pixel in the
 * frame's format; for GRAY8 it is a single byte replicated across the buffer.
 */
void pix_frame_clear(pix_frame_t *frame, uint32_t value);
/** @} */

#ifdef __cplusplus
}
#endif
