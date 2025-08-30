#include "frame_internal.h"
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

pix_color_t pix_frame_get_pixel(const pix_frame_t *frame, pix_point_t pt) {
  if (!frame || !frame->pixels)
    return 0;
  if ((uint16_t)pt.x >= frame->size.w || (uint16_t)pt.y >= frame->size.h)
    return 0;
  switch (frame->format) {
  case PIX_FMT_RGB24:
    return pix_frame_get_pixel_rgb24(frame, pt);
  case PIX_FMT_RGBA32:
    return pix_frame_get_pixel_rgba32(frame, pt);
  case PIX_FMT_GRAY8:
    return pix_frame_get_pixel_gray8(frame, pt);
  case PIX_FMT_RGB565:
    return pix_frame_get_pixel_rgb565(frame, pt);
  default:
    return 0;
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

  /* Dispatch to source-format specific helper */
  pix_point_t clipped_src = {(int16_t)sx, (int16_t)sy};
  pix_point_t clipped_dst = {(int16_t)dx, (int16_t)dy};
  pix_size_t clipped_size = {(uint16_t)w, (uint16_t)h};
  switch (src->format) {
  case PIX_FMT_RGB24:
    return pix_frame_copy_from_rgb24(dst, clipped_dst, src, clipped_src,
                                     clipped_size, flags);
  case PIX_FMT_RGBA32:
    return pix_frame_copy_from_rgba32(dst, clipped_dst, src, clipped_src,
                                      clipped_size, flags);
  case PIX_FMT_GRAY8:
    return pix_frame_copy_from_gray8(dst, clipped_dst, src, clipped_src,
                                     clipped_size, flags);
  case PIX_FMT_RGB565:
    return pix_frame_copy_from_rgb565(dst, clipped_dst, src, clipped_src,
                                      clipped_size, flags);
  default:
    return false;
  }
}
