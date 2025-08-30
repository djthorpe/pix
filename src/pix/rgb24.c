#include <pix/pix.h>
#include <stdint.h>

void pix_frame_set_pixel_rgb24(pix_frame_t *frame, pix_point_t pt,
                               pix_color_t color) {
  uint16_t x = (uint16_t)pt.x, y = (uint16_t)pt.y;
  uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
  row[x * 3 + 0] = (color >> 16) & 0xFF; // R
  row[x * 3 + 1] = (color >> 8) & 0xFF;  // G
  row[x * 3 + 2] = color & 0xFF;         // B
}

void pix_frame_clear_rgb24(pix_frame_t *frame, pix_color_t value) {
  uint8_t r = (value >> 16) & 0xFF;
  uint8_t g = (value >> 8) & 0xFF;
  uint8_t b = value & 0xFF;
  for (size_t y = 0; y < frame->size.h; ++y) {
    uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
    for (size_t x = 0; x < frame->size.w; ++x) {
      row[x * 3 + 0] = r;
      row[x * 3 + 1] = g;
      row[x * 3 + 2] = b;
    }
  }
}
