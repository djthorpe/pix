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
#include <pix/pix.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pix_frame_t pix_frame_t;

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
   * @brief Draw a line segment (backend may implement hardware acceleration).
   * @param frame Frame to draw into (must be locked).
   * @param x0,y0 Start point.
   * @param x1,y1 End point.
   * @param color 0xAARRGGBB.
   */
  void (*draw_line)(pix_frame_t *frame, pix_point_t a, pix_point_t b,
                    pix_color_t color);
};

/**
 * @name Default implementations
 * @brief Software fallbacks usable by backends.
 * @{ */

/** Set a pixel with straight-alpha src-over blending (0xAARRGGBB). */
void pix_frame_set_pixel(pix_frame_t *frame, pix_point_t pt, pix_color_t color);

/** Draw a line using integer Bresenham (no anti-aliasing). */
void pix_frame_draw_line(pix_frame_t *frame, pix_point_t a, pix_point_t b,
                         pix_color_t color);

/** Fill the entire frame with a solid value.
 * For byte-addressable formats, value should be a packed pixel in the
 * frame's format; for GRAY8 it is a single byte replicated across the buffer.
 */
void pix_frame_clear(pix_frame_t *frame, pix_color_t value);

/** @} */

/**
 * @name Frame copy (blit) utility
 * @brief Copy a rectangle of pixels from one frame to another.
 *
 * Performs format conversion if needed; if PIX_BLIT_ALPHA is set and the
 * source has an alpha channel (RGBA32) a src-over blend is applied into the
 * destination. Otherwise replaces destination pixels. The function clips the
 * requested region to the bounds of both frames. Returns false only on invalid
 * parameters (NULL frames). size.w==0 or size.h==0 -> success no-op.
 *
 * Locking: Caller must ensure frames are locked (if required by backend).
 * This routine does not call lock/unlock.
 */
/** Blit/copy flags */
typedef enum {
  PIX_BLIT_NONE = 0u,      /**< Default copy (convert formats if needed). */
  PIX_BLIT_ALPHA = 1u << 0 /**< Src-over blend if source has alpha channel. */
} pix_blit_flags_t;

bool pix_frame_copy(struct pix_frame_t *dst, pix_point_t dst_origin,
                    const struct pix_frame_t *src, pix_point_t src_origin,
                    pix_size_t size, pix_blit_flags_t flags);

/** @} */

#ifdef __cplusplus
}
#endif
