#include "color_internal.h"
#include <pix/pix.h>
#include <stdint.h>
#include <string.h>

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

pix_color_t pix_frame_get_pixel_rgb565(const pix_frame_t *frame,
                                       pix_point_t pt) {
  const uint16_t *row = (const uint16_t *)((const uint8_t *)frame->pixels +
                                           (size_t)pt.y * frame->stride);
  uint16_t v = row[pt.x];
  uint8_t r = (uint8_t)(((v >> 11) & 0x1F) << 3);
  uint8_t g = (uint8_t)(((v >> 5) & 0x3F) << 2);
  uint8_t b = (uint8_t)((v & 0x1F) << 3);
  return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

bool pix_frame_copy_from_rgb565(pix_frame_t *dst, pix_point_t dst_origin,
                                const pix_frame_t *src, pix_point_t src_origin,
                                pix_size_t size, pix_blit_flags_t flags) {
  (void)flags; /* no alpha in source */
  for (uint16_t row = 0; row < size.h; ++row) {
    const uint8_t *srow = (const uint8_t *)src->pixels +
                          (size_t)(src_origin.y + row) * src->stride +
                          (size_t)src_origin.x * 2u;
    uint8_t *drow =
        (uint8_t *)dst->pixels + (size_t)(dst_origin.y + row) * dst->stride;
    switch (dst->format) {
    case PIX_FMT_RGB565: {
      uint8_t *dp = drow + (size_t)dst_origin.x * 2u;
      memcpy(dp, srow, (size_t)size.w * 2u);
      break;
    }
    case PIX_FMT_RGB24: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint16_t *sp = (const uint16_t *)(srow + col * 2u);
        uint16_t v = *sp;
        uint8_t r = (uint8_t)(((v >> 11) & 0x1F) << 3);
        uint8_t g = (uint8_t)(((v >> 5) & 0x3F) << 2);
        uint8_t b = (uint8_t)((v & 0x1F) << 3);
        uint8_t *dp = drow + ((size_t)dst_origin.x + col) * 3u;
        dp[0] = r;
        dp[1] = g;
        dp[2] = b;
      }
      break;
    }
    case PIX_FMT_RGBA32: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint16_t *sp = (const uint16_t *)(srow + col * 2u);
        uint16_t v = *sp;
        uint8_t r = (uint8_t)(((v >> 11) & 0x1F) << 3);
        uint8_t g = (uint8_t)(((v >> 5) & 0x3F) << 2);
        uint8_t b = (uint8_t)((v & 0x1F) << 3);
        uint8_t *dp = drow + ((size_t)dst_origin.x + col) * 4u;
        dp[0] = r;
        dp[1] = g;
        dp[2] = b;
        dp[3] = 255;
      }
      break;
    }
    case PIX_FMT_GRAY8: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint16_t *sp = (const uint16_t *)(srow + col * 2u);
        uint16_t v = *sp;
        uint8_t r = (uint8_t)(((v >> 11) & 0x1F) << 3);
        uint8_t g = (uint8_t)(((v >> 5) & 0x3F) << 2);
        uint8_t b = (uint8_t)((v & 0x1F) << 3);
        uint8_t *dp = drow + (size_t)dst_origin.x + col;
        *dp = pix_luma(r, g, b);
      }
      break;
    }
    default:
      break;
    }
  }
  return true;
}