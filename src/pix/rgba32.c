#include "color_internal.h"
#include <pix/pix.h>
#include <stdint.h>
#include <string.h>

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

pix_color_t pix_frame_get_pixel_rgba32(const pix_frame_t *frame,
                                       pix_point_t pt) {
  const uint8_t *row =
      (const uint8_t *)frame->pixels + (size_t)pt.y * frame->stride;
  const uint8_t *p = row + (size_t)pt.x * 4u;
  return ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) |
         ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

bool pix_frame_copy_from_rgba32(pix_frame_t *dst, pix_point_t dst_origin,
                                const pix_frame_t *src, pix_point_t src_origin,
                                pix_size_t size, pix_blit_flags_t flags) {
  bool want_alpha = (flags & PIX_BLIT_ALPHA) != 0;
  for (uint16_t row = 0; row < size.h; ++row) {
    const uint8_t *srow = (const uint8_t *)src->pixels +
                          (size_t)(src_origin.y + row) * src->stride +
                          (size_t)src_origin.x * 4u;
    uint8_t *drow =
        (uint8_t *)dst->pixels + (size_t)(dst_origin.y + row) * dst->stride;
    switch (dst->format) {
    case PIX_FMT_RGBA32: {
      uint8_t *dp = drow + (size_t)dst_origin.x * 4u;
      if (!want_alpha) {
        memcpy(dp, srow, (size_t)size.w * 4u);
      } else {
        for (uint16_t col = 0; col < size.w; ++col) {
          const uint8_t *sp = srow + col * 4u;
          uint8_t *op = dp + col * 4u;
          uint8_t sa = sp[3];
          if (sa == 255) {
            op[0] = sp[0];
            op[1] = sp[1];
            op[2] = sp[2];
            op[3] = 255;
            continue;
          }
          if (sa == 0)
            continue;
          uint8_t dr = op[0], dg = op[1], db = op[2], da = op[3];
          uint32_t inv = 255 - sa;
          op[0] = (uint8_t)((sp[0] * sa + dr * inv) / 255);
          op[1] = (uint8_t)((sp[1] * sa + dg * inv) / 255);
          op[2] = (uint8_t)((sp[2] * sa + db * inv) / 255);
          op[3] = (uint8_t)(sa + (da * inv) / 255);
        }
      }
      break;
    }
    case PIX_FMT_RGB24: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint8_t *sp = srow + col * 4u;
        uint8_t *dp = drow + ((size_t)dst_origin.x + col) * 3u;
        if (want_alpha) {
          uint8_t sa = sp[3];
          uint8_t dr = dp[0], dg = dp[1], db = dp[2];
          uint32_t inv = 255 - sa;
          dp[0] = (uint8_t)((sp[0] * sa + dr * inv) / 255);
          dp[1] = (uint8_t)((sp[1] * sa + dg * inv) / 255);
          dp[2] = (uint8_t)((sp[2] * sa + db * inv) / 255);
        } else {
          dp[0] = sp[0];
          dp[1] = sp[1];
          dp[2] = sp[2];
        }
      }
      break;
    }
    case PIX_FMT_GRAY8: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint8_t *sp = srow + col * 4u;
        uint8_t *dp = drow + (size_t)dst_origin.x + col;
        uint8_t lum = pix_luma(sp[0], sp[1], sp[2]);
        if (want_alpha) {
          uint8_t sa = sp[3];
          uint8_t dg = *dp;
          uint32_t inv = 255 - sa;
          *dp = (uint8_t)((lum * sa + dg * inv) / 255);
        } else {
          *dp = lum;
        }
      }
      break;
    }
    case PIX_FMT_RGB565: {
      for (uint16_t col = 0; col < size.w; ++col) {
        const uint8_t *sp = srow + col * 4u;
        uint16_t *dp = (uint16_t *)(drow + ((size_t)dst_origin.x + col) * 2u);
        uint8_t r = sp[0], g = sp[1], b = sp[2];
        if (want_alpha) {
          uint8_t sa = sp[3];
          if (sa == 255) {
            *dp = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
          } else if (sa) {
            uint16_t dv = *dp;
            uint8_t dr = (uint8_t)(((dv >> 11) & 0x1F) << 3);
            uint8_t dg = (uint8_t)(((dv >> 5) & 0x3F) << 2);
            uint8_t db = (uint8_t)((dv & 0x1F) << 3);
            uint32_t inv = 255 - sa;
            uint8_t rr = (uint8_t)((r * sa + dr * inv) / 255);
            uint8_t rg = (uint8_t)((g * sa + dg * inv) / 255);
            uint8_t rb = (uint8_t)((b * sa + db * inv) / 255);
            *dp =
                (uint16_t)(((rr & 0xF8) << 8) | ((rg & 0xFC) << 3) | (rb >> 3));
          }
        } else {
          *dp = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }
      }
      break;
    }
    default:
      break;
    }
  }
  return true;
}
