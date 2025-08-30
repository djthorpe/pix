#include <pix/pix.h>
#include <stdint.h>

static inline uint8_t pix_rgb_to_gray(pix_color_t color) {
  return (uint8_t)((((color >> 16) & 0xFF) * 30 + ((color >> 8) & 0xFF) * 59 +
                    (color & 0xFF) * 11) /
                   100);
}

void pix_frame_set_pixel_gray8(pix_frame_t *frame, pix_point_t pt,
                               pix_color_t color) {
  uint16_t x = (uint16_t)pt.x, y = (uint16_t)pt.y;
  uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
  row[x] = pix_rgb_to_gray(color);
}

void pix_frame_clear_gray8(pix_frame_t *frame, pix_color_t value) {
  uint8_t g = pix_rgb_to_gray(value);
  for (size_t y = 0; y < frame->size.h; ++y) {
    uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
    for (size_t x = 0; x < frame->size.w; ++x) {
      row[x] = g;
    }
  }
}
