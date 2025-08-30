#include "pix.h"
#include <math.h>
#include <string.h>

void pix_frame_set_pixel(pix_frame_t *frame, pix_point_t pt,
                         pix_color_t color) {
  if (!frame || !frame->pixels)
    return;
  if ((uint16_t)pt.x >= frame->size.w || (uint16_t)pt.y >= frame->size.h)
    return;
  switch (frame->format) {
  case PIX_FMT_RGB24:
    pix_frame_set_pixel_rgb24(frame, pt, color);
    break;
  case PIX_FMT_RGBA32:
    pix_frame_set_pixel_rgba32(frame, pt, color);
    break;
  case PIX_FMT_GRAY8:
    pix_frame_set_pixel_gray8(frame, pt, color);
    break;
  case PIX_FMT_RGB565:
    pix_frame_set_pixel_rgb565(frame, pt, color);
    break;
  default:
    break;
  }
}

void pix_frame_draw_line(pix_frame_t *frame, pix_point_t a, pix_point_t b,
                         pix_color_t color) {
  if (!frame || !frame->set_pixel)
    return;
  int dx = abs((int)b.x - (int)a.x), sx = a.x < b.x ? 1 : -1;
  int dy = -abs((int)b.y - (int)a.y), sy = a.y < b.y ? 1 : -1;
  int err = dx + dy;
  int x = (int)a.x, y = (int)a.y;
  for (;;) {
    frame->set_pixel(frame, (pix_point_t){(int16_t)x, (int16_t)y}, color);
    if (x == (int)b.x && y == (int)b.y)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y += sy;
    }
  }
}

void pix_frame_clear(pix_frame_t *frame, pix_color_t value) {
  if (!frame || !frame->pixels)
    return;
  switch (frame->format) {
  case PIX_FMT_RGB24:
    pix_frame_clear_rgb24(frame, value);
    break;
  case PIX_FMT_RGBA32:
    pix_frame_clear_rgba32(frame, value);
    break;
  case PIX_FMT_GRAY8:
    pix_frame_clear_gray8(frame, value);
    break;
  case PIX_FMT_RGB565:
    pix_frame_clear_rgb565(frame, value);
    break;
  default:
    // no-op
    break;
  }
}

/* Internal helpers */
static inline uint8_t _luma(uint8_t r, uint8_t g, uint8_t b) {
  return (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
}

bool pix_frame_copy(pix_frame_t *dst, pix_point_t dst_origin,
                    const pix_frame_t *src, pix_point_t src_origin,
                    pix_size_t size, pix_blit_flags_t flags) {
  if (!dst || !src)
    return false;
  if (size.w == 0 || size.h == 0)
    return true; /* trivial */
  /* Promote to signed working variables */
  int sx = src_origin.x;
  int sy = src_origin.y;
  int dx = dst_origin.x;
  int dy = dst_origin.y;
  int w = (int)size.w;
  int h = (int)size.h;

  /* Clip against source */
  if (sx < 0) {
    int delta = -sx;
    if (delta >= w)
      return true; /* fully out */
    sx = 0;
    dx += delta;
    w -= delta;
  }
  if (sy < 0) {
    int delta = -sy;
    if (delta >= h)
      return true;
    sy = 0;
    dy += delta;
    h -= delta;
  }
  if (sx + w > src->size.w) {
    int delta = (sx + w) - src->size.w;
    if (delta >= w)
      return true;
    w -= delta;
  }
  if (sy + h > src->size.h) {
    int delta = (sy + h) - src->size.h;
    if (delta >= h)
      return true;
    h -= delta;
  }
  /* Clip against destination */
  if (dx < 0) {
    int delta = -dx;
    if (delta >= w)
      return true;
    dx = 0;
    sx += delta;
    w -= delta;
  }
  if (dy < 0) {
    int delta = -dy;
    if (delta >= h)
      return true;
    dy = 0;
    sy += delta;
    h -= delta;
  }
  if (dx + w > dst->size.w) {
    int delta = (dx + w) - dst->size.w;
    if (delta >= w)
      return true;
    w -= delta;
  }
  if (dy + h > dst->size.h) {
    int delta = (dy + h) - dst->size.h;
    if (delta >= h)
      return true;
    h -= delta;
  }
  if (w <= 0 || h <= 0)
    return true;
  if (!src->pixels || !dst->pixels)
    return false;

  /* Fast path: identical formats, no alpha blend requested or source has no
   * alpha */
  bool want_alpha = (flags & PIX_BLIT_ALPHA) != 0;
  bool src_has_alpha = (src->format == PIX_FMT_RGBA32);
  if (src->format == dst->format && (!want_alpha || !src_has_alpha)) {
    size_t row_bytes;
    switch (src->format) {
    case PIX_FMT_RGB24:
      row_bytes = (size_t)w * 3u;
      break;
    case PIX_FMT_RGBA32:
      row_bytes = (size_t)w * 4u;
      break;
    case PIX_FMT_GRAY8:
      row_bytes = (size_t)w;
      break;
    case PIX_FMT_RGB565:
      row_bytes = (size_t)w * 2u;
      break;
    default:
      row_bytes = 0;
    }
    if (row_bytes) {
      for (int row = 0; row < h; ++row) {
        const uint8_t *srow = (const uint8_t *)src->pixels +
                              (size_t)(sy + row) * src->stride +
                              (size_t)sx * row_bytes / (size_t)w;
        uint8_t *drow = (uint8_t *)dst->pixels +
                        (size_t)(dy + row) * dst->stride +
                        (size_t)dx * row_bytes / (size_t)w;
        memcpy(drow, srow, row_bytes);
      }
      return true;
    }
  }

  /* Helper lambdas (static inline functions not available inside function, use
   * macros) */
  for (int row = 0; row < h; ++row) {
    int sr = sy + row;
    int dr = dy + row;
    /* Row pointers */
    uint8_t *drow = (uint8_t *)dst->pixels + (size_t)dr * dst->stride;
    const uint8_t *srow =
        (const uint8_t *)src->pixels + (size_t)sr * src->stride;
    for (int col = 0; col < w; ++col) {
      int sc = sx + col;
      int dc = dx + col;
      switch (src->format) {
      case PIX_FMT_RGBA32: {
        const uint8_t *sp = srow + sc * 4;
        uint8_t sr_ = sp[0], sg_ = sp[1], sb_ = sp[2], sa_ = sp[3];
        if (!want_alpha)
          sa_ = 255; /* treat as opaque replace */
        if (dst->format == PIX_FMT_RGBA32) {
          uint8_t *dp = drow + dc * 4;
          if (want_alpha) {
            uint8_t dr_ = dp[0], dg_ = dp[1], db_ = dp[2], da_ = dp[3];
            /* src-over */
            uint32_t inv = 255 - sa_;
            dp[0] = (uint8_t)((sr_ * sa_ + dr_ * inv) / 255);
            dp[1] = (uint8_t)((sg_ * sa_ + dg_ * inv) / 255);
            dp[2] = (uint8_t)((sb_ * sa_ + db_ * inv) / 255);
            dp[3] = (uint8_t)(sa_ + (da_ * inv) / 255);
          } else {
            dp[0] = sr_;
            dp[1] = sg_;
            dp[2] = sb_;
            dp[3] = sa_;
          }
        } else if (dst->format == PIX_FMT_RGB24) {
          uint8_t *dp = drow + dc * 3;
          if (want_alpha) {
            uint8_t dr_ = dp[0], dg_ = dp[1], db_ = dp[2];
            uint32_t inv = 255 - sa_;
            dp[0] = (uint8_t)((sr_ * sa_ + dr_ * inv) / 255);
            dp[1] = (uint8_t)((sg_ * sa_ + dg_ * inv) / 255);
            dp[2] = (uint8_t)((sb_ * sa_ + db_ * inv) / 255);
          } else {
            dp[0] = sr_;
            dp[1] = sg_;
            dp[2] = sb_;
          }
        } else if (dst->format == PIX_FMT_GRAY8) {
          uint8_t *dp = drow + dc;
          if (want_alpha) {
            uint8_t dg_ = *dp;
            uint8_t lum = _luma(sr_, sg_, sb_);
            *dp = (uint8_t)((lum * sa_ + dg_ * (255 - sa_)) / 255);
          } else {
            *dp = _luma(sr_, sg_, sb_);
          }
        } else if (dst->format == PIX_FMT_RGB565) {
          uint16_t *dp = (uint16_t *)(drow + dc * 2);
          if (want_alpha) {
            /* Read existing */
            uint16_t dv = *dp;
            uint8_t dr_ = (uint8_t)(((dv >> 11) & 0x1F) << 3);
            uint8_t dg_ = (uint8_t)(((dv >> 5) & 0x3F) << 2);
            uint8_t db_ = (uint8_t)((dv & 0x1F) << 3);
            uint32_t inv = 255 - sa_;
            uint8_t rr = (uint8_t)((sr_ * sa_ + dr_ * inv) / 255);
            uint8_t rg = (uint8_t)((sg_ * sa_ + dg_ * inv) / 255);
            uint8_t rb = (uint8_t)((sb_ * sa_ + db_ * inv) / 255);
            *dp =
                (uint16_t)(((rr & 0xF8) << 8) | ((rg & 0xFC) << 3) | (rb >> 3));
          } else {
            *dp = (uint16_t)(((sr_ & 0xF8) << 8) | ((sg_ & 0xFC) << 3) |
                             (sb_ >> 3));
          }
        }
        break;
      }
      case PIX_FMT_RGB24: {
        const uint8_t *sp = srow + sc * 3;
        uint8_t sr_ = sp[0], sg_ = sp[1], sb_ = sp[2];
        if (dst->format == PIX_FMT_RGB24) {
          uint8_t *dp = drow + dc * 3;
          dp[0] = sr_;
          dp[1] = sg_;
          dp[2] = sb_;
        } else if (dst->format == PIX_FMT_RGBA32) {
          uint8_t *dp = drow + dc * 4;
          dp[0] = sr_;
          dp[1] = sg_;
          dp[2] = sb_;
          dp[3] = 255;
        } else if (dst->format == PIX_FMT_GRAY8) {
          drow[dc] = _luma(sr_, sg_, sb_);
        } else if (dst->format == PIX_FMT_RGB565) {
          uint16_t *dp = (uint16_t *)(drow + dc * 2);
          *dp = (uint16_t)(((sr_ & 0xF8) << 8) | ((sg_ & 0xFC) << 3) |
                           (sb_ >> 3));
        }
        break;
      }
      case PIX_FMT_GRAY8: {
        uint8_t g = srow[sc];
        if (dst->format == PIX_FMT_GRAY8) {
          drow[dc] = g;
        } else if (dst->format == PIX_FMT_RGB24) {
          uint8_t *dp = drow + dc * 3;
          dp[0] = dp[1] = dp[2] = g;
        } else if (dst->format == PIX_FMT_RGBA32) {
          uint8_t *dp = drow + dc * 4;
          dp[0] = dp[1] = dp[2] = g;
          dp[3] = 255;
        } else if (dst->format == PIX_FMT_RGB565) {
          uint16_t *dp = (uint16_t *)(drow + dc * 2);
          *dp = (uint16_t)(((g & 0xF8) << 8) | ((g & 0xFC) << 3) | (g >> 3));
        }
        break;
      }
      case PIX_FMT_RGB565: {
        const uint16_t *sp = (const uint16_t *)(srow + sc * 2);
        uint16_t v = *sp;
        uint8_t sr_ = (uint8_t)(((v >> 11) & 0x1F) << 3);
        uint8_t sg_ = (uint8_t)(((v >> 5) & 0x3F) << 2);
        uint8_t sb_ = (uint8_t)((v & 0x1F) << 3);
        if (dst->format == PIX_FMT_RGB565) {
          uint16_t *dp = (uint16_t *)(drow + dc * 2);
          *dp = v;
        } else if (dst->format == PIX_FMT_RGB24) {
          uint8_t *dp = drow + dc * 3;
          dp[0] = sr_;
          dp[1] = sg_;
          dp[2] = sb_;
        } else if (dst->format == PIX_FMT_RGBA32) {
          uint8_t *dp = drow + dc * 4;
          dp[0] = sr_;
          dp[1] = sg_;
          dp[2] = sb_;
          dp[3] = 255;
        } else if (dst->format == PIX_FMT_GRAY8) {
          drow[dc] = _luma(sr_, sg_, sb_);
        }
        break;
      }
      default:
        break;
      }
    }
  }
  return true;
}
