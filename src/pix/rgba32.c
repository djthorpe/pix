#include <pix/pix.h>
#include <stdint.h>

void pix_frame_set_pixel_rgba32(pix_frame_t *frame, pix_point_t pt,
                                pix_color_t color) {
  uint16_t x = (uint16_t)pt.x, y = (uint16_t)pt.y;
  uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
  uint32_t a = (color >> 24) & 0xFFu;
  if (a == 0)
    return; // nothing to do
  uint32_t sr = (color >> 16) & 0xFFu;
  uint32_t sg = (color >> 8) & 0xFFu;
  uint32_t sb = color & 0xFFu;
  if (a == 255u) {
    row[x * 4 + 0] = (uint8_t)a;
    row[x * 4 + 1] = (uint8_t)sr;
    row[x * 4 + 2] = (uint8_t)sg;
    row[x * 4 + 3] = (uint8_t)sb;
    return;
  }
  // src-over blend
  uint32_t da = row[x * 4 + 0];
  uint32_t dr = row[x * 4 + 1];
  uint32_t dg = row[x * 4 + 2];
  uint32_t db = row[x * 4 + 3];
  uint32_t ia = 255u - a;
  uint32_t r = (sr * a + dr * ia + 127u) / 255u;
  uint32_t g = (sg * a + dg * ia + 127u) / 255u;
  uint32_t b = (sb * a + db * ia + 127u) / 255u;
  uint32_t out_a = a + ((da * ia + 127u) / 255u);
  row[x * 4 + 0] = (uint8_t)out_a;
  row[x * 4 + 1] = (uint8_t)r;
  row[x * 4 + 2] = (uint8_t)g;
  row[x * 4 + 3] = (uint8_t)b;
}

void pix_frame_clear_rgba32(pix_frame_t *frame, pix_color_t value) {
  uint8_t a = (value >> 24) & 0xFF;
  uint8_t r = (value >> 16) & 0xFF;
  uint8_t g = (value >> 8) & 0xFF;
  uint8_t b = value & 0xFF;
  for (size_t y = 0; y < frame->size.h; ++y) {
    uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
    for (size_t x = 0; x < frame->size.w; ++x) {
      row[x * 4 + 0] = a;
      row[x * 4 + 1] = r;
      row[x * 4 + 2] = g;
      row[x * 4 + 3] = b;
    }
  }
}
