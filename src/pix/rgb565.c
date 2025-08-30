#include <pix/pix.h>
#include <stdint.h>

static inline uint16_t pack_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void pix_frame_set_pixel_rgb565(pix_frame_t *frame, pix_point_t pt,
                                pix_color_t color) {
  uint16_t x = (uint16_t)pt.x, y = (uint16_t)pt.y;
  uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
  uint32_t a = (color >> 24) & 0xFFu;
  uint8_t sr = (color >> 16) & 0xFFu;
  uint8_t sg = (color >> 8) & 0xFFu;
  uint8_t sb = color & 0xFFu;
  uint16_t *dst = (uint16_t *)(row + x * 2u);
  if (a >= 250u) { // opaque
    *dst = pack_rgb565(sr, sg, sb);
    return;
  } else if (a == 0) {
    return;
  }
  // Read existing
  uint16_t d = *dst;
  uint8_t dr = (uint8_t)((d >> 11) & 0x1F) * 255 / 31;
  uint8_t dg = (uint8_t)((d >> 5) & 0x3F) * 255 / 63;
  uint8_t db = (uint8_t)(d & 0x1F) * 255 / 31;
  uint32_t ia = 255u - a;
  uint8_t r = (uint8_t)((sr * a + dr * ia + 127) / 255);
  uint8_t g = (uint8_t)((sg * a + dg * ia + 127) / 255);
  uint8_t b = (uint8_t)((sb * a + db * ia + 127) / 255);
  *dst = pack_rgb565(r, g, b);
}

void pix_frame_clear_rgb565(pix_frame_t *frame, pix_color_t value) {
  uint8_t r = (value >> 16) & 0xFFu;
  uint8_t g = (value >> 8) & 0xFFu;
  uint8_t b = value & 0xFFu;
  uint16_t packed = pack_rgb565(r, g, b);
  for (size_t y = 0; y < frame->size.h; ++y) {
    uint16_t *row = (uint16_t *)((uint8_t *)frame->pixels + y * frame->stride);
    for (size_t x = 0; x < frame->size.w; ++x) {
      row[x] = packed;
    }
  }
}