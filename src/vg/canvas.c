/* Clean reconstructed canvas implementation (trimmed stroke logic) */
#include "shape_internal.h"
#include <math.h>
#include <pix/pix.h>
#include <vg/vg.h>

static void aabb_init(float *minx, float *miny, float *maxx, float *maxy) {
  *minx = 1e30f;
  *miny = 1e30f;
  *maxx = -1e30f;
  *maxy = -1e30f;
}
static void aabb_include(float x, float y, float *minx, float *miny,
                         float *maxx, float *maxy) {
  if (x < *minx)
    *minx = x;
  if (y < *miny)
    *miny = y;
  if (x > *maxx)
    *maxx = x;
  if (y > *maxy)
    *maxy = y;
}
static inline int32_t vg_pack_i16(int xi, int yi) {
  return ((int32_t)(xi & 0xFFFF) << 16) | (int32_t)(yi & 0xFFFF);
}
static bool vg_transform_inverse_affine(const vg_transform_t *in,
                                        vg_transform_t *out) {
  float a = in->m[0][0], b = in->m[0][1], tx = in->m[0][2];
  float c = in->m[1][0], d = in->m[1][1], ty = in->m[1][2];
  float det = a * d - b * c;
  if (fabsf(det) < 1e-12f)
    return false;
  float inv = 1.f / det;
  out->m[0][0] = d * inv;
  out->m[0][1] = -b * inv;
  out->m[1][0] = -c * inv;
  out->m[1][1] = a * inv;
  out->m[0][2] = -(out->m[0][0] * tx + out->m[0][1] * ty);
  out->m[1][2] = -(out->m[1][0] * tx + out->m[1][1] * ty);
  out->m[2][0] = 0.f;
  out->m[2][1] = 0.f;
  out->m[2][2] = 1.f;
  return true;
}

static inline float _fpart(float x) { return x - floorf(x); }
static inline float _rfpart(float x) { return 1.f - _fpart(x); }
static inline void plot_aa(pix_frame_t *frame, int x, int y, pix_color_t color,
                           float cov, bool steep) {
  if (cov <= 0.f)
    return;
  if (cov > 1.f)
    cov = 1.f;
  uint8_t ba = (uint8_t)((color >> 24) & 0xFF);
  if (ba == 0)
    return;
  uint8_t a = (uint8_t)lroundf((float)ba * cov);
  uint32_t mod = (color & 0x00FFFFFFu) | ((uint32_t)a << 24);
  size_t px = (size_t)(steep ? y : x);
  size_t py = (size_t)(steep ? x : y);
  if (px >= frame->size.w || py >= frame->size.h)
    return;
  frame->set_pixel(frame, (pix_point_t){(int16_t)px, (int16_t)py}, mod);
}

static void draw_line_aa(pix_frame_t *frame, float x0, float y0, float x1,
                         float y1, pix_color_t color) {
  bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
  if (steep) {
    float t = x0;
    x0 = y0;
    y0 = t;
    t = x1;
    x1 = y1;
    y1 = t;
  }
  if (x0 > x1) {
    float t = x0;
    x0 = x1;
    x1 = t;
    t = y0;
    y0 = y1;
    y1 = t;
  }
  float dx = x1 - x0;
  float dy = y1 - y0;
  float grad = (dx == 0.f) ? 0.f : (dy / dx);
  float xend = roundf(x0);
  float yend = y0 + grad * (xend - x0);
  float xgap = _rfpart(x0 + 0.5f);
  int xpxl1 = (int)xend;
  int ypxl1 = (int)floorf(yend);
  plot_aa(frame, xpxl1, ypxl1, color, _rfpart(yend) * xgap, steep);
  plot_aa(frame, xpxl1, ypxl1 + 1, color, _fpart(yend) * xgap, steep);
  float intery = yend + grad;
  xend = roundf(x1);
  yend = y1 + grad * (xend - x1);
  xgap = _fpart(x1 + 0.5f);
  int xpxl2 = (int)xend;
  int ypxl2 = (int)floorf(yend);
  for (int x = xpxl1 + 1; x <= xpxl2 - 1; ++x) {
    int yi = (int)floorf(intery);
    plot_aa(frame, x, yi, color, _rfpart(intery), steep);
    plot_aa(frame, x, yi + 1, color, _fpart(intery), steep);
    intery += grad;
  }
  plot_aa(frame, xpxl2, ypxl2, color, _rfpart(yend) * xgap, steep);
  plot_aa(frame, xpxl2, ypxl2 + 1, color, _fpart(yend) * xgap, steep);
}

vg_canvas_t vg_canvas_init(size_t capacity) {
  vg_canvas_t canvas;
  canvas.shapes = (vg_shape_t **)VG_MALLOC(capacity * sizeof(vg_shape_t *));
  if (!canvas.shapes) {
    canvas.size = 0;
    canvas.capacity = 0;
    canvas.pool = NULL;
    canvas.pool_capacity = 0;
    canvas.pool_used = 0;
    return canvas;
  }
  canvas.size = 0;
  canvas.capacity = capacity;
  canvas.pool = (vg_shape_t *)VG_MALLOC(capacity * sizeof(vg_shape_t));
  if (!canvas.pool) {
    canvas.pool_capacity = 0;
    canvas.pool_used = 0;
  } else {
    canvas.pool_capacity = capacity;
    canvas.pool_used = 0;
  }
  return canvas;
}

vg_shape_t *vg_canvas_append(vg_canvas_t *canvas) {
  if (!canvas || canvas->size == canvas->capacity)
    return NULL;
  vg_shape_t *s = NULL;
  if (canvas->pool && canvas->pool_used < canvas->pool_capacity) {
    s = &canvas->pool[canvas->pool_used++];
    vg__shape_internal_defaults(s);
  } else {
    s = vg_shape_create();
    if (!s)
      return NULL;
  }
  canvas->shapes[canvas->size++] = s;
  return s;
}

void vg_canvas_clear(vg_canvas_t *canvas) {
  if (!canvas)
    return;
  if (canvas->pool) {
    for (size_t i = 0; i < canvas->size; ++i) {
      vg_shape_t *s = canvas->shapes[i];
      if (!s)
        continue;
      if (s >= canvas->pool && s < canvas->pool + canvas->pool_capacity) {
        vg_path_finish(vg_shape_path(s));
      } else {
        vg_shape_destroy(s);
      }
    }
    canvas->pool_used = 0;
  } else {
    for (size_t i = 0; i < canvas->size; ++i)
      vg_shape_destroy(canvas->shapes[i]);
  }
  canvas->size = 0;
}

void vg_canvas_destroy(vg_canvas_t *canvas) {
  if (!canvas)
    return;
  vg_canvas_clear(canvas);
  if (canvas->shapes) {
    VG_FREE(canvas->shapes);
    canvas->shapes = NULL;
  }
  if (canvas->pool) {
    VG_FREE(canvas->pool);
    canvas->pool = NULL;
  }
  canvas->capacity = 0;
  canvas->pool_capacity = 0;
  canvas->pool_used = 0;
}

static void blit_scaled_contain(pix_frame_t *dst, const vg_image_ref_t *img,
                                const pix_frame_t *srcf, pix_size_t src_full) {
  int sw = src_full.w, sh = src_full.h;
  int dw_max = dst->size.w, dh_max = dst->size.h;
  float sx = (float)dw_max / (float)sw;
  float sy = (float)dh_max / (float)sh;
  float s = sx < sy ? sx : sy;
  if (s <= 0.f)
    s = 1.f;
  int dw = (int)lroundf(sw * s);
  if (dw < 1)
    dw = 1;
  int dh = (int)lroundf(sh * s);
  if (dh < 1)
    dh = 1;
  int dx0 = (dw_max - dw) / 2;
  int dy0 = (dh_max - dh) / 2;
  switch (srcf->format) {
  case PIX_FMT_RGB24: {
    const uint8_t *sp = (const uint8_t *)srcf->pixels +
                        (size_t)img->src_origin.y * srcf->stride +
                        (size_t)img->src_origin.x * 3u;
    for (int y = 0; y < dh; ++y) {
      int syi = (int)((float)y / dh * sh);
      if (syi >= sh)
        syi = sh - 1;
      uint8_t *drow = (uint8_t *)dst->pixels + (size_t)(dy0 + y) * dst->stride +
                      (size_t)dx0 * 3u;
      const uint8_t *srow = sp + (size_t)syi * srcf->stride;
      for (int x = 0; x < dw; ++x) {
        int sxi = (int)((float)x / dw * sw);
        if (sxi >= sw)
          sxi = sw - 1;
        const uint8_t *spx = srow + sxi * 3u;
        uint8_t *dp = drow + x * 3u;
        dp[0] = spx[0];
        dp[1] = spx[1];
        dp[2] = spx[2];
      }
    }
  } break;
  case PIX_FMT_GRAY8: {
    const uint8_t *sp = (const uint8_t *)srcf->pixels +
                        (size_t)img->src_origin.y * srcf->stride +
                        (size_t)img->src_origin.x;
    for (int y = 0; y < dh; ++y) {
      int syi = (int)((float)y / dh * sh);
      if (syi >= sh)
        syi = sh - 1;
      uint8_t *drow = (uint8_t *)dst->pixels + (size_t)(dy0 + y) * dst->stride +
                      (size_t)dx0;
      const uint8_t *srow = sp + (size_t)syi * srcf->stride;
      for (int x = 0; x < dw; ++x) {
        int sxi = (int)((float)x / dw * sw);
        if (sxi >= sw)
          sxi = sw - 1;
        drow[x] = srow[sxi];
      }
    }
  } break;
  case PIX_FMT_RGB565: {
    const uint16_t *sp =
        (const uint16_t *)((const uint8_t *)srcf->pixels +
                           (size_t)img->src_origin.y * srcf->stride) +
        img->src_origin.x;
    for (int y = 0; y < dh; ++y) {
      int syi = (int)((float)y / dh * sh);
      if (syi >= sh)
        syi = sh - 1;
      uint16_t *drow = (uint16_t *)((uint8_t *)dst->pixels +
                                    (size_t)(dy0 + y) * dst->stride) +
                       dx0;
      const uint16_t *srow =
          (const uint16_t *)((const uint8_t *)sp + (size_t)syi * srcf->stride);
      for (int x = 0; x < dw; ++x) {
        int sxi = (int)((float)x / dw * sw);
        if (sxi >= sw)
          sxi = sw - 1;
        drow[x] = srow[sxi];
      }
    }
  } break;
  default:
    pix_frame_copy(dst, img->dst_origin, (pix_frame_t *)srcf, img->src_origin,
                   src_full, (pix_blit_flags_t)img->flags);
    break;
  }
}

static void blit_transformed(pix_frame_t *dst, const vg_image_ref_t *img,
                             const pix_frame_t *srcf, pix_size_t src_full,
                             const vg_transform_t *xf) {
  vg_transform_t inv;
  if (!vg_transform_inverse_affine(xf, &inv)) {
    pix_frame_copy(dst, img->dst_origin, (pix_frame_t *)srcf, img->src_origin,
                   src_full, (pix_blit_flags_t)img->flags);
    return;
  }
  float x0 = (float)img->dst_origin.x, y0 = (float)img->dst_origin.y;
  float x1 = x0 + src_full.w, y1 = y0 + src_full.h;
  float cx[4], cy[4];
  vg_transform_point(xf, x0, y0, &cx[0], &cy[0]);
  vg_transform_point(xf, x1, y0, &cx[1], &cy[1]);
  vg_transform_point(xf, x1, y1, &cx[2], &cy[2]);
  vg_transform_point(xf, x0, y1, &cx[3], &cy[3]);
  float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
  for (int k = 0; k < 4; ++k) {
    if (cx[k] < minx)
      minx = cx[k];
    if (cy[k] < miny)
      miny = cy[k];
    if (cx[k] > maxx)
      maxx = cx[k];
    if (cy[k] > maxy)
      maxy = cy[k];
  }
  int ix0 = (int)floorf(minx), iy0 = (int)floorf(miny), ix1 = (int)ceilf(maxx),
      iy1 = (int)ceilf(maxy);
  if (ix0 < 0)
    ix0 = 0;
  if (iy0 < 0)
    iy0 = 0;
  if (ix1 > (int)dst->size.w)
    ix1 = (int)dst->size.w;
  if (iy1 > (int)dst->size.h)
    iy1 = (int)dst->size.h;
  if (ix0 >= ix1 || iy0 >= iy1)
    return;
  if (srcf->format != dst->format) {
    pix_frame_copy(dst, img->dst_origin, (pix_frame_t *)srcf, img->src_origin,
                   src_full, (pix_blit_flags_t)img->flags);
    return;
  }
  int sw = src_full.w, sh = src_full.h;
  switch (srcf->format) {
  case PIX_FMT_RGB24: {
    const uint8_t *base = (const uint8_t *)srcf->pixels;
    for (int y = iy0; y < iy1; ++y) {
      uint8_t *drow = (uint8_t *)dst->pixels + (size_t)y * dst->stride;
      for (int x = ix0; x < ix1; ++x) {
        float ux, uy;
        vg_transform_point(&inv, (float)x, (float)y, &ux, &uy);
        float sx_f = ux - (float)img->dst_origin.x;
        float sy_f = uy - (float)img->dst_origin.y;
        if (sx_f < 0.f || sy_f < 0.f || sx_f >= (float)sw || sy_f >= (float)sh)
          continue;
        int sxi = (int)sx_f, syi = (int)sy_f;
        int spx = img->src_origin.x + sxi, spy = img->src_origin.y + syi;
        if (spx < 0 || spy < 0 || spx >= srcf->size.w || spy >= srcf->size.h)
          continue;
        const uint8_t *sp =
            base + (size_t)spy * srcf->stride + (size_t)spx * 3u;
        uint8_t *dp = drow + (size_t)x * 3u;
        dp[0] = sp[0];
        dp[1] = sp[1];
        dp[2] = sp[2];
      }
    }
  } break;
  case PIX_FMT_GRAY8: {
    const uint8_t *base = (const uint8_t *)srcf->pixels;
    for (int y = iy0; y < iy1; ++y) {
      uint8_t *drow = (uint8_t *)dst->pixels + (size_t)y * dst->stride;
      for (int x = ix0; x < ix1; ++x) {
        float ux, uy;
        vg_transform_point(&inv, (float)x, (float)y, &ux, &uy);
        float sx_f = ux - (float)img->dst_origin.x;
        float sy_f = uy - (float)img->dst_origin.y;
        if (sx_f < 0.f || sy_f < 0.f || sx_f >= (float)sw || sy_f >= (float)sh)
          continue;
        int sxi = (int)sx_f, syi = (int)sy_f;
        int spx = img->src_origin.x + sxi, spy = img->src_origin.y + syi;
        if (spx < 0 || spy < 0 || spx >= srcf->size.w || spy >= srcf->size.h)
          continue;
        const uint8_t *sp = base + (size_t)spy * srcf->stride + (size_t)spx;
        drow[x] = *sp;
      }
    }
  } break;
  case PIX_FMT_RGB565: {
    const uint16_t *base = (const uint16_t *)srcf->pixels;
    for (int y = iy0; y < iy1; ++y) {
      uint16_t *drow =
          (uint16_t *)((uint8_t *)dst->pixels + (size_t)y * dst->stride);
      for (int x = ix0; x < ix1; ++x) {
        float ux, uy;
        vg_transform_point(&inv, (float)x, (float)y, &ux, &uy);
        float sx_f = ux - (float)img->dst_origin.x;
        float sy_f = uy - (float)img->dst_origin.y;
        if (sx_f < 0.f || sy_f < 0.f || sx_f >= (float)sw || sy_f >= (float)sh)
          continue;
        int sxi = (int)sx_f, syi = (int)sy_f;
        int spx = img->src_origin.x + sxi, spy = img->src_origin.y + syi;
        if (spx < 0 || spy < 0 || spx >= srcf->size.w || spy >= srcf->size.h)
          continue;
        const uint16_t *sp = (const uint16_t *)((const uint8_t *)base +
                                                (size_t)spy * srcf->stride) +
                             spx;
        drow[x] = *sp;
      }
    }
  } break;
  default:
    pix_frame_copy(dst, img->dst_origin, (pix_frame_t *)srcf, img->src_origin,
                   src_full, (pix_blit_flags_t)img->flags);
    break;
  }
}

void vg_canvas_render(const vg_canvas_t *canvas, pix_frame_t *frame) {
  if (!canvas || !frame)
    return;
  for (size_t i = 0; i < canvas->size; ++i) {
    vg_shape_t *shape = canvas->shapes[i];
    if (!shape)
      continue;
    if (shape->kind == VG_SHAPE_PATH) {
      if (vg_shape_get_fill_color(shape) != VG_COLOR_NONE) {
        vg_fill_path(vg_shape_const_path(shape), vg_shape_get_transform(shape),
                     frame, vg_shape_get_fill_color(shape),
                     vg_shape_get_fill_rule(shape));
      }
      if (vg_shape_get_stroke_color(shape) != VG_COLOR_NONE) {
        const vg_path_t *p = vg_shape_const_path(shape);
        while (p) { /* stroke not reimplemented in this trimmed restore */
          p = p->next;
        }
      }
    } else if (shape->kind == VG_SHAPE_IMAGE) {
      const vg_image_ref_t *img = &shape->data.img;
      if (!img->frame)
        continue;
      const pix_frame_t *srcf = img->frame;
      pix_size_t src_full = img->src_size.w
                                ? img->src_size
                                : (pix_size_t){srcf->size.w, srcf->size.h};
      const vg_transform_t *xf = vg_shape_get_transform(shape);
      if (xf) {
        blit_transformed(frame, img, srcf, src_full, xf);
      } else {
        bool at_origin = (img->dst_origin.x == 0 && img->dst_origin.y == 0);
        bool size_match =
            (src_full.w == frame->size.w && src_full.h == frame->size.h);
        bool fmt_match = (srcf->format == frame->format);
        bool do_scale = at_origin && !size_match && fmt_match;
        if (do_scale)
          blit_scaled_contain(frame, img, srcf, src_full);
        else
          pix_frame_copy(frame, img->dst_origin, (pix_frame_t *)srcf,
                         img->src_origin, src_full,
                         (pix_blit_flags_t)img->flags);
      }
    }
  }
}
