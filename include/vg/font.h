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

typedef struct vg_font_t {
  uint8_t ascent;  // baseline to top (rows)
  uint8_t descent; // baseline to bottom (rows)
} vg_font_t;

extern const vg_font_t vg_font_tiny5x7;

float vg_font_text_width(const vg_font_t *font, const char *text,
                         float pixel_size, float letter_spacing);

vg_shape_t *vg_font_make_text_shape(const vg_font_t *font, const char *text,
                                    pix_color_t color, float pixel_size,
                                    float letter_spacing, float *out_width);

vg_shape_t *vg_font_get_text_shape_cached(const vg_font_t *font,
                                          const char *text, pix_color_t color,
                                          float pixel_size,
                                          float letter_spacing,
                                          float *out_width);

void vg_font_text_cache_clear(void);
size_t vg_font_text_cache_size(void);
size_t vg_font_text_cache_limit(void);
void vg_font_text_cache_set_limit(size_t n);

#ifdef __cplusplus
}
#endif
