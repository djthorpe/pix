/**
 * @file include/pix/frame.h
 * @ingroup pix
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
#include <pix/pix.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pix_frame_t pix_frame_t;

/* Blit/copy flags */
typedef enum { PIX_BLIT_NONE = 0u, PIX_BLIT_ALPHA = 1u << 0 } pix_blit_flags_t;

/**
 * @brief A 2D pixel buffer and drawing interface.
 *
 * Backends should fill in the function pointers. The software renderer uses
 * set_pixel for composition and draw_line for fast hairline rendering where
 * available; otherwise it falls back to its own routines.
 */
struct pix_frame_t {
  void *pixels; /**< Pointer to the first pixel (may be NULL until locked). */
  pix_size_t size;     /**< Frame dimensions (w,h) in pixels. */
  size_t stride;       /**< Bytes per row (>= width * bytes_per_pixel). */
  pix_format_t format; /**< Pixel format of the buffer. */
  void *user; /**< Backend-specific context (e.g., SDL renderer/texture). */

  /** Optional finalize hook to release backend-owned resources (textures,
      pixel buffers, queues). It must NOT free the pix_frame_t object itself.
      After finalize, the frame can be reinitialized or freed by the caller.
      May be NULL if no special cleanup needed. */
  void (*finalize)(struct pix_frame_t *frame);

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
  void (*set_pixel)(pix_frame_t *frame, pix_point_t pt, pix_color_t color);

  /**
   * @brief Read a pixel (no bounds expansion). Returns 0 on OOB.
   * @param frame Frame to read from (must be locked if backend requires).
   * @param pt Coordinate to sample.
   * @return 0xAARRGGBB (alpha 0xFF if format has no alpha channel).
   */
  pix_color_t (*get_pixel)(const struct pix_frame_t *frame, pix_point_t pt);

  /**
   * @brief Draw a line segment (backend may implement hardware acceleration).
   * @param frame Frame to draw into (must be locked).
   * @param x0,y0 Start point.
   * @param x1,y1 End point.
   * @param color 0xAARRGGBB.
   */
  void (*draw_line)(pix_frame_t *frame, pix_point_t a, pix_point_t b,
                    pix_color_t color);

  /**
   * @brief Copy a rectangle of pixels from another frame (blit).
   */
  bool (*copy)(struct pix_frame_t *dst, pix_point_t dst_origin,
               const struct pix_frame_t *src, pix_point_t src_origin,
               pix_size_t size, pix_blit_flags_t flags);
};

#ifdef __cplusplus
}
#endif
