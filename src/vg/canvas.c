/* vg/canvas.c - clean, format-agnostic canvas implementation */
#include "../pix/frame_internal.h"
#include "fill_internal.h"  /* internal fill */
#include "shape_internal.h" /* internal shape_create/destroy */
#include <math.h>
#include <pix/pix.h>
#include <string.h>

/* -------- Canvas management -------- */
vg_canvas_t vg_canvas_init(size_t capacity) {
  vg_canvas_t c;
  c.size = 0;
  c.capacity = capacity;
  c.shapes = NULL;
  c.next = NULL;
  if (capacity == 0)
    return c;
  c.shapes = (vg_shape_t **)VG_MALLOC(sizeof(vg_shape_t *) * capacity);
  if (!c.shapes) {
    c.capacity = 0;
  }
  return c;
}

static vg_canvas_t *vg_canvas_tail(vg_canvas_t *c) {
  while (c && c->next)
    c = c->next;
  return c;
}

vg_shape_t *vg_canvas_append(vg_canvas_t *canvas) {
  if (!canvas)
    return NULL;
  vg_canvas_t *tail = vg_canvas_tail(canvas);
  if (tail->size >= tail->capacity) {
    size_t new_cap = tail->capacity ? tail->capacity : 1;
    vg_canvas_t *chunk = (vg_canvas_t *)VG_MALLOC(sizeof(vg_canvas_t));
    if (!chunk)
      return NULL;
    *chunk = vg_canvas_init(new_cap);
    if (chunk->capacity == 0) {
      VG_FREE(chunk);
      return NULL;
    }
    tail->next = chunk;
    tail = chunk;
  }
  vg_shape_t *s = vg_shape_create();
  if (!s)
    return NULL;
  tail->shapes[tail->size++] = s;
  return s;
}

void vg_canvas_destroy(vg_canvas_t *canvas) {
  if (!canvas)
    return;
  /* free all overflow chunks */
  vg_canvas_t *c = canvas->next;
  while (c) {
    vg_canvas_t *next = c->next;
    for (size_t i = 0; i < c->size; ++i) {
      vg_shape_t *s = c->shapes[i];
      if (s)
        vg_shape_destroy(s);
    }
    if (c->shapes)
      VG_FREE(c->shapes);
    VG_FREE(c);
    c = next;
  }
  /* head */
  for (size_t i = 0; i < canvas->size; ++i) {
    vg_shape_t *s = canvas->shapes[i];
    if (s)
      vg_shape_destroy(s);
  }
  if (canvas->shapes)
    VG_FREE(canvas->shapes);
  canvas->shapes = NULL;
  canvas->size = 0;
  canvas->capacity = 0;
  canvas->next = NULL;
}

/* -------- Stroke rendering (Wu AA) -------- */
static inline float _fpart(float x) { return x - floorf(x); }
static inline float _rfpart(float x) { return 1.f - _fpart(x); }

static inline void blend_cov(pix_frame_t *f, int x, int y, pix_color_t c,
                             float cov) {
  if ((unsigned)x >= f->size.w || (unsigned)y >= f->size.h)
    return;
  if (cov <= 0.f)
    return;
  if (cov > 1.f)
    cov = 1.f;
  uint8_t sa = (uint8_t)(c >> 24);
  if (sa == 0)
    return;
  uint8_t sr = (c >> 16) & 0xFF, sg = (c >> 8) & 0xFF, sb = c & 0xFF;
  float a = (sa / 255.f) * cov;
  pix_color_t dstc =
      pix_frame_get_pixel(f, (pix_point_t){(int16_t)x, (int16_t)y});
  uint8_t da = (dstc >> 24) & 0xFF, dr = (dstc >> 16) & 0xFF,
          dg = (dstc >> 8) & 0xFF, db = dstc & 0xFF;
  float fa = a + (1.f - a) * (da / 255.f);
  float inv = (1.f - a);
  uint8_t or = (uint8_t)fminf(255.f, sr * a + dr * inv);
  uint8_t og = (uint8_t)fminf(255.f, sg * a + dg * inv);
  uint8_t ob = (uint8_t)fminf(255.f, sb * a + db * inv);
  uint8_t oa = (uint8_t)fminf(255.f, fa * 255.f);
  pix_color_t out = ((pix_color_t)oa << 24) | ((pix_color_t)or << 16) |
                    ((pix_color_t)og << 8) | (pix_color_t)ob;
  pix_frame_set_pixel(f, (pix_point_t){(int16_t)x, (int16_t)y}, out);
}

static void plot_aa(pix_frame_t *f, int x, int y, pix_color_t c, float cov) {
  blend_cov(f, x, y, c, cov);
}

static void draw_line_aa(pix_frame_t *f, float x0, float y0, float x1, float y1,
                         pix_color_t c) {
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
  float dx = x1 - x0, dy = y1 - y0;
  float grad = (dx == 0.f) ? 1.f : dy / dx;
  float xend = floorf(x0 + 0.5f);
  float yend = y0 + grad * (xend - x0);
  float xgap = _rfpart(x0 + 0.5f);
  int xpxl1 = (int)xend;
  int ypxl1 = (int)floorf(yend);
  if (steep) {
    plot_aa(f, ypxl1, xpxl1, c, _rfpart(yend) * xgap);
    plot_aa(f, ypxl1 + 1, xpxl1, c, _fpart(yend) * xgap);
  } else {
    plot_aa(f, xpxl1, ypxl1, c, _rfpart(yend) * xgap);
    plot_aa(f, xpxl1, ypxl1 + 1, c, _fpart(yend) * xgap);
  }
  float intery = yend + grad;
  xend = floorf(x1 + 0.5f);
  yend = y1 + grad * (xend - x1);
  xgap = _fpart(x1 + 0.5f);
  int xpxl2 = (int)xend;
  int ypxl2 = (int)floorf(yend);
  if (steep) {
    for (int x = xpxl1 + 1; x < xpxl2; ++x) {
      plot_aa(f, (int)floorf(intery), x, c, _rfpart(intery));
      plot_aa(f, (int)floorf(intery) + 1, x, c, _fpart(intery));
      intery += grad;
    }
    plot_aa(f, ypxl2, xpxl2, c, _rfpart(yend) * xgap);
    plot_aa(f, ypxl2 + 1, xpxl2, c, _fpart(yend) * xgap);
  } else {
    for (int x = xpxl1 + 1; x < xpxl2; ++x) {
      plot_aa(f, x, (int)floorf(intery), c, _rfpart(intery));
      plot_aa(f, x, (int)floorf(intery) + 1, c, _fpart(intery));
      intery += grad;
    }
    plot_aa(f, xpxl2, ypxl2, c, _rfpart(yend) * xgap);
    plot_aa(f, xpxl2, ypxl2 + 1, c, _fpart(yend) * xgap);
  }
}

/* -------- Image blitting -------- */
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
  for (int y = 0; y < dh; ++y) {
    int syi = (int)((float)y / dh * sh);
    if (syi >= sh)
      syi = sh - 1;
    for (int x = 0; x < dw; ++x) {
      int sxi = (int)((float)x / dw * sw);
      if (sxi >= sw)
        sxi = sw - 1;
      pix_color_t c = pix_frame_get_pixel(
          srcf, (pix_point_t){(int16_t)(img->src_origin.x + sxi),
                              (int16_t)(img->src_origin.y + syi)});
      pix_frame_set_pixel(
          dst, (pix_point_t){(int16_t)(dx0 + x), (int16_t)(dy0 + y)}, c);
    }
  }
}

static void blit_transformed(pix_frame_t *dst, const vg_image_ref_t *img,
                             const pix_frame_t *srcf, pix_size_t src_full,
                             const vg_transform_t *xf) {
  bool axis = fabsf(xf->m[0][1]) < 1e-6f && fabsf(xf->m[1][0]) < 1e-6f;
  if (axis) {
    float sx = xf->m[0][0], sy = xf->m[1][1];
    float tx = xf->m[0][2], ty = xf->m[1][2];
    if (fabsf(sx) < 1e-12f || fabsf(sy) < 1e-12f)
      return;
    int sw = src_full.w, sh = src_full.h;
    int dx0 = (int)floorf((float)img->dst_origin.x * sx + tx);
    int dy0 = (int)floorf((float)img->dst_origin.y * sy + ty);
    int dx1 = (int)ceilf((float)(img->dst_origin.x + sw) * sx + tx);
    int dy1 = (int)ceilf((float)(img->dst_origin.y + sh) * sy + ty);
    if (dx0 > dx1) {
      int t = dx0;
      dx0 = dx1;
      dx1 = t;
    }
    if (dy0 > dy1) {
      int t = dy0;
      dy0 = dy1;
      dy1 = t;
    }
    if (dx0 < 0)
      dx0 = 0;
    if (dy0 < 0)
      dy0 = 0;
    if (dx1 > (int)dst->size.w)
      dx1 = (int)dst->size.w;
    if (dy1 > (int)dst->size.h)
      dy1 = (int)dst->size.h;
    if (dx0 >= dx1 || dy0 >= dy1)
      return;
    float inv_sx = 1.f / sx, inv_sy = 1.f / sy;
    for (int y = dy0; y < dy1; ++y) {
      float syf = ((float)y - ty) * inv_sy - (float)img->dst_origin.y;
      int syi = (int)floorf(syf);
      if (syi < 0 || syi >= sh)
        continue;
      for (int x = dx0; x < dx1; ++x) {
        float sxf = ((float)x - tx) * inv_sx - (float)img->dst_origin.x;
        int sxi = (int)floorf(sxf);
        if (sxi < 0 || sxi >= sw)
          continue;
        pix_color_t c = pix_frame_get_pixel(
            srcf, (pix_point_t){(int16_t)(img->src_origin.x + sxi),
                                (int16_t)(img->src_origin.y + syi)});
        pix_frame_set_pixel(dst, (pix_point_t){(int16_t)x, (int16_t)y}, c);
      }
    }
    return;
  }
  vg_transform_t inv;
  if (!vg_transform_inverse_affine(xf, &inv)) {
    if (dst->copy) {
      dst->copy(dst, img->dst_origin, (pix_frame_t *)srcf, img->src_origin,
                src_full, (pix_blit_flags_t)img->flags);
    }
    return;
  }
  float x0 = (float)img->dst_origin.x, y0 = (float)img->dst_origin.y;
  float x1 = x0 + src_full.w, y1 = y0 + src_full.h;
  float cx[4], cy[4];
  vg_transform_point(xf, x0, y0, &cx[0], &cy[0]);
  vg_transform_point(xf, x1, y0, &cx[1], &cy[1]);
  vg_transform_point(xf, x1, y1, &cx[2], &cy[2]);
  vg_transform_point(xf, x0, y1, &cx[3], &cy[3]);
  float minx = cx[0], maxx = cx[0], miny = cy[0], maxy = cy[0];
  for (int i = 1; i < 4; ++i) {
    if (cx[i] < minx)
      minx = cx[i];
    if (cx[i] > maxx)
      maxx = cx[i];
    if (cy[i] < miny)
      miny = cy[i];
    if (cy[i] > maxy)
      maxy = cy[i];
  }
  int ix0 = (int)floorf(minx);
  if (ix0 < 0)
    ix0 = 0;
  int iy0 = (int)floorf(miny);
  if (iy0 < 0)
    iy0 = 0;
  int ix1 = (int)ceilf(maxx);
  if (ix1 > (int)dst->size.w)
    ix1 = dst->size.w;
  int iy1 = (int)ceilf(maxy);
  if (iy1 > (int)dst->size.h)
    iy1 = dst->size.h;
  int sw = src_full.w, sh = src_full.h;
  for (int y = iy0; y < iy1; ++y) {
    for (int x = ix0; x < ix1; ++x) {
      float ux, uy;
      vg_transform_point(&inv, (float)x, (float)y, &ux, &uy);
      float sx_f = ux - (float)img->dst_origin.x;
      float sy_f = uy - (float)img->dst_origin.y;
      if (sx_f < 0.f || sy_f < 0.f || sx_f >= (float)sw || sy_f >= (float)sh)
        continue;
      int sxi = (int)sx_f, syi = (int)sy_f;
      pix_color_t c = pix_frame_get_pixel(
          srcf, (pix_point_t){(int16_t)(img->src_origin.x + sxi),
                              (int16_t)(img->src_origin.y + syi)});
      pix_frame_set_pixel(dst, (pix_point_t){(int16_t)x, (int16_t)y}, c);
    }
  }
}

/* -------- Render loop -------- */
void vg_canvas_render(const vg_canvas_t *canvas, pix_frame_t *frame) {
  if (!canvas || !frame)
    return;
  for (vg_canvas_t *chunk = (vg_canvas_t *)canvas; chunk; chunk = chunk->next) {
    for (size_t i = 0; i < chunk->size; ++i) {
      vg_shape_t *shape = chunk->shapes[i];
      if (!shape)
        continue;
      if (shape->kind == VG_SHAPE_PATH) {
        if (vg_shape_get_fill_color(shape) != PIX_COLOR_NONE) {
          vg_fill_path(vg_shape_path(shape), vg_shape_get_transform(shape),
                       frame, vg_shape_get_fill_color(shape),
                       vg_shape_get_fill_rule(shape));
        }
        if (vg_shape_get_stroke_color(shape) != PIX_COLOR_NONE) {
          vg_path_t *seg = vg_shape_path(shape);
          float width = vg_shape_get_stroke_width(shape);
          if (width <= 0.0f) {
            // Width exactly zero or negative: treat as disabled stroke (common
            // in tiger data where stroke flags may be set but width=0 meaning
            // none).
            continue;
          }
          if (width < 0.5f) // sub‑pixel widths still get single AA line
            width = 0.5f;
          pix_color_t scolor = vg_shape_get_stroke_color(shape);
          const vg_transform_t *sxf = vg_shape_get_transform(shape);
          while (seg) {
            if (seg->size >= 2) {
              for (size_t si = 1; si < seg->size; ++si) {
                pix_point_t a = seg->points[si - 1];
                pix_point_t b = seg->points[si];
                float x0 = a.x, y0 = a.y;
                float x1 = b.x, y1 = b.y;
                if (sxf) {
                  float tx0 =
                      sxf->m[0][0] * x0 + sxf->m[0][1] * y0 + sxf->m[0][2];
                  float ty0 =
                      sxf->m[1][0] * x0 + sxf->m[1][1] * y0 + sxf->m[1][2];
                  float tx1 =
                      sxf->m[0][0] * x1 + sxf->m[0][1] * y1 + sxf->m[0][2];
                  float ty1 =
                      sxf->m[1][0] * x1 + sxf->m[1][1] * y1 + sxf->m[1][2];
                  x0 = tx0;
                  y0 = ty0;
                  x1 = tx1;
                  y1 = ty1;
                }
                if (width <= 1.01f) {
                  draw_line_aa(frame, x0, y0, x1, y1, scolor);
                } else {
                  int layers = (int)ceilf(width);
                  float half = (layers - 1) * 0.5f;
                  float dx = x1 - x0, dy = y1 - y0;
                  float len = sqrtf(dx * dx + dy * dy) + 1e-6f;
                  float nx = -dy / len, ny = dx / len;
                  for (int li = 0; li < layers; ++li) {
                    float o = (li - half);
                    float ox = nx * o, oy = ny * o;
                    draw_line_aa(frame, x0 + ox, y0 + oy, x1 + ox, y1 + oy,
                                 scolor);
                  }
                }
              }
            }
            seg = seg->next;
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
        if (xf)
          blit_transformed(frame, img, srcf, src_full, xf);
        else {
          bool at_origin = (img->dst_origin.x == 0 && img->dst_origin.y == 0);
          bool size_match =
              (src_full.w == frame->size.w && src_full.h == frame->size.h);
          bool fmt_match = (srcf->format == frame->format);
          bool do_scale = at_origin && !size_match && fmt_match;
          if (do_scale)
            blit_scaled_contain(frame, img, srcf, src_full);
          else if (frame->copy) {
            frame->copy(frame, img->dst_origin, (pix_frame_t *)srcf,
                        img->src_origin, src_full,
                        (pix_blit_flags_t)img->flags);
          }
        }
      }
    }
  }
}

/* Bounding box (ignores transforms) */
void vg_canvas_bbox(const vg_canvas_t *canvas, pix_point_t *origin,
                    pix_size_t *size) {
  if (origin) {
    origin->x = 0;
    origin->y = 0;
  }
  if (size) {
    size->w = 0;
    size->h = 0;
  }
  if (!canvas)
    return;
  bool first = true;
  int minx = 0, miny = 0, maxx = 0, maxy = 0;
  for (const vg_canvas_t *chunk = canvas; chunk; chunk = chunk->next) {
    for (size_t i = 0; i < chunk->size; ++i) {
      const vg_shape_t *shape = chunk->shapes[i];
      if (!shape)
        continue;
      if (shape->kind == VG_SHAPE_PATH) {
        vg_path_t *p = vg_shape_path((vg_shape_t *)shape);
        while (p) {
          for (size_t pi = 0; pi < p->size; ++pi) {
            pix_point_t pt = p->points[pi];
            int x = pt.x;
            int y = pt.y;
            if (first) {
              minx = maxx = x;
              miny = maxy = y;
              first = false;
            } else {
              if (x < minx)
                minx = x;
              if (x > maxx)
                maxx = x;
              if (y < miny)
                miny = y;
              if (y > maxy)
                maxy = y;
            }
          }
          p = p->next;
        }
      } else if (shape->kind == VG_SHAPE_IMAGE) {
        const vg_image_ref_t *img = &shape->data.img;
        if (!img->frame)
          continue;
        int x0 = img->dst_origin.x;
        int y0 = img->dst_origin.y;
        int w = img->src_size.w ? img->src_size.w : img->frame->size.w;
        int h = img->src_size.h ? img->src_size.h : img->frame->size.h;
        int x1 = x0 + w;
        int y1 = y0 + h;
        if (first) {
          minx = x0;
          miny = y0;
          maxx = x1;
          maxy = y1;
          first = false;
        } else {
          if (x0 < minx)
            minx = x0;
          if (y0 < miny)
            miny = y0;
          if (x1 > maxx)
            maxx = x1;
          if (y1 > maxy)
            maxy = y1;
        }
      }
    }
  }
  if (!first) {
    if (origin) {
      origin->x = (int16_t)minx;
      origin->y = (int16_t)miny;
    }
    if (size) {
      int w = maxx - minx;
      int h = maxy - miny;
      if (w < 0)
        w = 0;
      if (h < 0)
        h = 0;
      if (w > 0xFFFF)
        w = 0xFFFF;
      if (h > 0xFFFF)
        h = 0xFFFF;
      size->w = (uint16_t)w;
      size->h = (uint16_t)h;
    }
  }
}
