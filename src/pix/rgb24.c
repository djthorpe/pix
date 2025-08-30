#include "color_internal.h"
#include <pix/pix.h>
#include <stdint.h>
#include <string.h>

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

pix_color_t pix_frame_get_pixel_rgb24(const pix_frame_t *frame,
                                      pix_point_t pt) {
  const uint8_t *row =
      (const uint8_t *)frame->pixels + (size_t)pt.y * frame->stride;
  const uint8_t *p = row + (size_t)pt.x * 3u;
  return 0xFF000000u | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) |
         (uint32_t)p[2];
}

bool pix_frame_copy_from_rgb24(pix_frame_t *dst, pix_point_t dst_origin,
                               const pix_frame_t *src, pix_point_t src_origin,
                               pix_size_t size, pix_blit_flags_t flags) {
  (void)flags; /* no alpha in source */
  for (uint16_t row = 0; row < size.h; ++row) {
    const uint8_t *srow = (const uint8_t *)src->pixels +
                          (size_t)(src_origin.y + row) * src->stride +
                          (size_t)src_origin.x * 3u;
    uint8_t *drow =
        (uint8_t *)dst->pixels + (size_t)(dst_origin.y + row) * dst->stride;
    switch (dst->format) {
    case PIX_FMT_RGB24: {
      uint8_t *dp = drow + (size_t)dst_origin.x * 3u;
      memcpy(dp, srow, (size_t)size.w * 3u);
      break;
    }
    case PIX_FMT_RGBA32: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint8_t *sp = srow + col * 3u;
        uint8_t *dp = drow + ((size_t)dst_origin.x + col) * 4u;
        dp[0] = sp[0];
        dp[1] = sp[1];
        dp[2] = sp[2];
        dp[3] = 255;
      }
      break;
    }
    case PIX_FMT_GRAY8: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint8_t *sp = srow + col * 3u;
        uint8_t *dp = drow + (size_t)dst_origin.x + col;
        *dp = pix_luma(sp[0], sp[1], sp[2]);
      }
      break;
    }
    case PIX_FMT_RGB565: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint8_t *sp = srow + col * 3u;
        uint16_t *dp = (uint16_t *)(drow + ((size_t)dst_origin.x + col) * 2u);
        uint8_t r = sp[0], g = sp[1], b = sp[2];
        *dp = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
      }
      break;
    }
    default:
      break;
    }
  }
  return true;
}
