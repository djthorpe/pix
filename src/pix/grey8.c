#include <pix/pix.h>
#include <stdint.h>
#include <string.h>

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

pix_color_t pix_frame_get_pixel_gray8(const pix_frame_t *frame,
                                      pix_point_t pt) {
  const uint8_t *row =
      (const uint8_t *)frame->pixels + (size_t)pt.y * frame->stride;
  uint8_t g = row[pt.x];
  return 0xFF000000u | ((uint32_t)g << 16) | ((uint32_t)g << 8) | (uint32_t)g;
}

bool pix_frame_copy_from_gray8(pix_frame_t *dst, pix_point_t dst_origin,
                               const pix_frame_t *src, pix_point_t src_origin,
                               pix_size_t size, pix_blit_flags_t flags) {
  (void)flags; /* no alpha in source */
  for (uint16_t row = 0; row < size.h; ++row) {
    const uint8_t *srow = (const uint8_t *)src->pixels +
                          (size_t)(src_origin.y + row) * src->stride +
                          (size_t)src_origin.x;
    uint8_t *drow =
        (uint8_t *)dst->pixels + (size_t)(dst_origin.y + row) * dst->stride;
    switch (dst->format) {
    case PIX_FMT_GRAY8: {
      uint8_t *dp = drow + (size_t)dst_origin.x;
      memcpy(dp, srow, (size_t)size.w);
      break;
    }
    case PIX_FMT_RGB24: {
      for (uint16_t col = 0; col < size.w; ++col) {
        uint8_t g = srow[col];
        uint8_t *dp = drow + ((size_t)dst_origin.x + col) * 3u;
        dp[0] = dp[1] = dp[2] = g;
      }
      break;
    }
    case PIX_FMT_RGBA32: {
      for (uint16_t col = 0; col < size.w; ++col) {
        uint8_t g = srow[col];
        uint8_t *dp = drow + ((size_t)dst_origin.x + col) * 4u;
        dp[0] = dp[1] = dp[2] = g;
        dp[3] = 255;
      }
      break;
    }
    case PIX_FMT_RGB565: {
      for (uint16_t col = 0; col < size.w; ++col) {
        uint8_t g = srow[col];
        uint16_t *dp = (uint16_t *)(drow + ((size_t)dst_origin.x + col) * 2u);
        *dp = (uint16_t)(((g & 0xF8) << 8) | ((g & 0xFC) << 3) | (g >> 3));
      }
      break;
    }
    default:
      break;
    }
  }
  return true;
}
