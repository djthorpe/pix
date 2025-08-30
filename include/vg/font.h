#pragma once
#include "path.h"
#include "shape.h"
#include "transform.h"
#include <pix/pix.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct vg_font_t
 * @brief Minimal bitmap font metrics (EM box vertical extents).
 *
 * ascent + descent defines the nominal line height in unscaled pixel rows.
 */
typedef struct vg_font_t {
  uint8_t ascent;  /**< Baseline to top (rows). */
  uint8_t descent; /**< Baseline to bottom (rows). */
} vg_font_t;

/** @ingroup vg
 * Tiny built-in 5x7 baseline font (ascent=7, descent=2). */
extern const vg_font_t vg_font_tiny5x7;

/**
 * @ingroup vg
 * @brief Measure text width (single / multi-line) without shaping.
 *
 * Newlines reset the pen x (multi-line width is max line width).
 * @param font Font metrics.
 * @param text UTF-8 string (ASCII subset supported).
 * @param pixel_size Target pixel height for EM ascent (<=0 -> default 7).
 * @param letter_spacing Extra pixel spacing between glyph boxes (>=0).
 */
/** @ingroup vg */
float vg_font_text_width(const vg_font_t *font, const char *text,
                         float pixel_size, float letter_spacing);

/**
 * @ingroup vg
 * @brief Create a new shape containing a filled outline of @p text.
 *
 * The returned shape is owned by the caller (vg_shape_destroy when done).
 * @param font Font metrics.
 * @param text ASCII text string.
 * @param color Fill color (stroke disabled, stroke width=0).
 * @param pixel_size Desired pixel EM height.
 * @param letter_spacing Additional pixel spacing between glyph advances.
 * @param out_width Optional measured width output.
 */
/** @ingroup vg */
vg_shape_t *vg_font_make_text_shape(const vg_font_t *font, const char *text,
                                    pix_color_t color, float pixel_size,
                                    float letter_spacing, float *out_width);

/**
 * @ingroup vg
 * @brief Retrieve (or build and cache) an outline shape for @p text.
 *
 * Returned shape is owned by the cache; do not destroy. Safe until cache
 * cleared or evicted. See vg_font_text_cache_* for management.
 */
/** @ingroup vg */
vg_shape_t *vg_font_get_text_shape_cached(const vg_font_t *font,
                                          const char *text, pix_color_t color,
                                          float pixel_size,
                                          float letter_spacing,
                                          float *out_width);

/** @ingroup vg
 * Clear the global text shape cache (destroys cached shapes). */
void vg_font_text_cache_clear(void);
/** @ingroup vg Current number of cached entries. */
size_t vg_font_text_cache_size(void);
/** @ingroup vg Current entry capacity limit. */
size_t vg_font_text_cache_limit(void);
/** @ingroup vg Set cache entry limit (minimum 1). May evict immediately if
 * smaller. */
void vg_font_text_cache_set_limit(size_t n);

#ifdef __cplusplus
}
#endif
