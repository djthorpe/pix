// Inlined from legacy vg/canvas.c
#include "pix/frame.h"
#include <math.h>
#include <vg/canvas.h>
#include <vg/fill.h>
#include <vg/path.h>
#include <vg/shape.h>
#include <vg/transform.h>
#include <vg/vg.h>

// --- Viewport culling helpers ---
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

static void aabb_expand(float pad, float *minx, float *miny, float *maxx,
                        float *maxy) {
  *minx -= pad;
  *miny -= pad;
  *maxx += pad;
  *maxy += pad;
}

static void compute_path_aabb_screen(const vg_path_t *path,
                                     const vg_transform_t *xf, float *minx,
                                     float *miny, float *maxx, float *maxy) {
  aabb_init(minx, miny, maxx, maxy);
  const vg_path_t *seg = path;
  while (seg) {
    for (size_t j = 0; j < seg->size; ++j) {
      int32_t pt = seg->points[j];
      float x = (pt >> 16) & 0xFFFF;
      float y = pt & 0xFFFF;
      if (xf) {
        float tx, ty;
        vg_transform_point(xf, x, y, &tx, &ty);
        x = tx;
        y = ty;
      }
      aabb_include(x, y, minx, miny, maxx, maxy);
    }
    seg = seg->next;
  }
}

static inline bool aabb_intersects_frame(float minx, float miny, float maxx,
                                         float maxy, const pix_frame_t *frame) {
  // Convert frame bounds to float
  float fw = (float)frame->width;
  float fh = (float)frame->height;
  // No intersection if one is entirely to the side of the other
  if (maxx < 0.0f || maxy < 0.0f)
    return false;
  if (minx > fw - 1.0f || miny > fh - 1.0f)
    return false;
  return true;
}

static inline int32_t pack_point_i16(int xi, int yi) {
  return ((int32_t)(xi & 0xFFFF) << 16) | (int32_t)(yi & 0xFFFF);
}

// Forward declarations for local helpers
static void fill_quad_clip(pix_frame_t *frame, float ax, float ay, float bx,
                           float by, float cx, float cy, float dx, float dy,
                           uint32_t color, int clip_x0, int clip_y0,
                           int clip_x1, int clip_y1);
static void fill_triangle_clip(pix_frame_t *frame, float ax, float ay, float bx,
                               float by, float cx, float cy, uint32_t color,
                               int clip_x0, int clip_y0, int clip_x1,
                               int clip_y1);

// --- Xiaolin Wu antialiased line for hairline strokes ---
static inline float _fpart(float x) { return x - floorf(x); }
static inline float _rfpart(float x) { return 1.0f - _fpart(x); }

static inline void plot_aa(pix_frame_t *frame, int x, int y, uint32_t color,
                           float cov, bool steep) {
  if (cov <= 0.0f)
    return;
  if (cov > 1.0f)
    cov = 1.0f;
  uint8_t base_a = (uint8_t)((color >> 24) & 0xFF);
  if (base_a == 0)
    return;
  uint8_t a = (uint8_t)lroundf((float)base_a * cov);
  uint32_t mod = (color & 0x00FFFFFFu) | ((uint32_t)a << 24);
  size_t px = (size_t)(steep ? y : x);
  size_t py = (size_t)(steep ? x : y);
  if (px >= frame->width || py >= frame->height)
    return;
  frame->set_pixel(frame, px, py, mod);
}

static void draw_line_aa(pix_frame_t *frame, float x0, float y0, float x1,
                         float y1, uint32_t color) {
  bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
  if (steep) {
    float t;
    t = x0;
    x0 = y0;
    y0 = t;
    t = x1;
    x1 = y1;
    y1 = t;
  }
  if (x0 > x1) {
    float t;
    t = x0;
    x0 = x1;
    x1 = t;
    t = y0;
    y0 = y1;
    y1 = t;
  }
  float dx = x1 - x0;
  float dy = y1 - y0;
  float grad = (dx == 0.0f) ? 0.0f : (dy / dx);

  // first endpoint
  float xend = roundf(x0);
  float yend = y0 + grad * (xend - x0);
  float xgap = _rfpart(x0 + 0.5f);
  int xpxl1 = (int)xend;
  int ypxl1 = (int)floorf(yend);
  plot_aa(frame, xpxl1, ypxl1, color, _rfpart(yend) * xgap, steep);
  plot_aa(frame, xpxl1, ypxl1 + 1, color, _fpart(yend) * xgap, steep);
  float intery = yend + grad;

  // second endpoint
  xend = roundf(x1);
  yend = y1 + grad * (xend - x1);
  xgap = _fpart(x1 + 0.5f);
  int xpxl2 = (int)xend;
  int ypxl2 = (int)floorf(yend);

  // main loop
  for (int x = xpxl1 + 1; x <= xpxl2 - 1; ++x) {
    int yi = (int)floorf(intery);
    plot_aa(frame, x, yi, color, _rfpart(intery), steep);
    plot_aa(frame, x, yi + 1, color, _fpart(intery), steep);
    intery += grad;
  }

  // draw the second endpoint
  plot_aa(frame, xpxl2, ypxl2, color, _rfpart(yend) * xgap, steep);
  plot_aa(frame, xpxl2, ypxl2 + 1, color, _fpart(yend) * xgap, steep);
}

static void draw_stroke_segment(pix_frame_t *frame, float x0, float y0,
                                float x1, float y1, uint32_t color, float width,
                                int clip_x0, int clip_y0, int clip_x1,
                                int clip_y1) {
  if (!frame)
    return;
  if (width <= 1.5f) {
    // Hairline: use antialiased line
    draw_line_aa(frame, x0, y0, x1, y1, color);
    return;
  }
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  float hw = width * 0.5f;
  if (len < 1e-6f) {
    // Degenerate: draw a small square centered at point
    float hx = hw, hy = hw;
    float xA = x0 - hx, yA = y0 - hy;
    float xB = x0 + hx, yB = y0 - hy;
    float xC = x0 + hx, yC = y0 + hy;
    float xD = x0 - hx, yD = y0 + hy;
    fill_quad_clip(frame, xA, yA, xB, yB, xC, yC, xD, yD, color, clip_x0,
                   clip_y0, clip_x1, clip_y1);
    return;
  }
  float nx = -dy / len;
  float ny = dx / len;
  float hx = nx * hw;
  float hy = ny * hw;
  // Rectangle around the segment: A(x0-hx,y0-hy), B(x0+hx,y0+hy),
  // C(x1+hx,y1+hy), D(x1-hx,y1-hy)
  float xA = x0 - hx, yA = y0 - hy;
  float xB = x0 + hx, yB = y0 + hy;
  float xC = x1 + hx, yC = y1 + hy;
  float xD = x1 - hx, yD = y1 - hy;
  fill_quad_clip(frame, xA, yA, xB, yB, xC, yC, xD, yD, color, clip_x0, clip_y0,
                 clip_x1, clip_y1);
}

static void fill_quad_clip(pix_frame_t *frame, float ax, float ay, float bx,
                           float by, float cx, float cy, float dx, float dy,
                           uint32_t color, int clip_x0, int clip_y0,
                           int clip_x1, int clip_y1) {
  vg_point_t pts[5];
  pts[0] = pack_point_i16((int)lroundf(ax), (int)lroundf(ay));
  pts[1] = pack_point_i16((int)lroundf(bx), (int)lroundf(by));
  pts[2] = pack_point_i16((int)lroundf(cx), (int)lroundf(cy));
  pts[3] = pack_point_i16((int)lroundf(dx), (int)lroundf(dy));
  pts[4] = pts[0];
  vg_path_t p = {.points = pts, .size = 5, .capacity = 5, .next = NULL};
  vg_fill_path_clipped(&p, NULL, frame, color, VG_FILL_EVEN_ODD, clip_x0,
                       clip_y0, clip_x1, clip_y1);
}

static void fill_triangle_clip(pix_frame_t *frame, float ax, float ay, float bx,
                               float by, float cx, float cy, uint32_t color,
                               int clip_x0, int clip_y0, int clip_x1,
                               int clip_y1) {
  vg_point_t pts[4];
  pts[0] = pack_point_i16((int)lroundf(ax), (int)lroundf(ay));
  pts[1] = pack_point_i16((int)lroundf(bx), (int)lroundf(by));
  pts[2] = pack_point_i16((int)lroundf(cx), (int)lroundf(cy));
  pts[3] = pts[0];
  vg_path_t p = {.points = pts, .size = 4, .capacity = 4, .next = NULL};
  vg_fill_path_clipped(&p, NULL, frame, color, VG_FILL_EVEN_ODD, clip_x0,
                       clip_y0, clip_x1, clip_y1);
}

static void cap_end(pix_frame_t *frame, float px, float py, float nx, float ny,
                    float dx, float dy, float hw, uint32_t color, vg_cap_t cap,
                    int clip_x0, int clip_y0, int clip_x1, int clip_y1) {
  // nx,ny is the normal (left-hand), dx,dy is the direction (unit), hw=half
  // width
  float pLx = px + nx * hw, pLy = py + ny * hw;
  float pRx = px - nx * hw, pRy = py - ny * hw;
  if (cap == VG_CAP_BUTT) {
    // nothing extra
    return;
  } else if (cap == VG_CAP_SQUARE) {
    // extend a half-width forward (for end) or backward (for start) handled by
    // caller Here, draw an extra quad beyond the end
    float ex = dx * hw, ey = dy * hw;
    fill_quad_clip(frame, pLx, pLy, pRx, pRy, pRx + ex, pRy + ey, pLx + ex,
                   pLy + ey, color, clip_x0, clip_y0, clip_x1, clip_y1);
  } else { // VG_CAP_ROUND
    // draw semicircle fan from pL -> arc -> pR using incremental rotation
    const int segs = 14;
    vg_point_t pts[14 + 3];
    int n = 0;
    pts[n++] = pack_point_i16((int)lroundf(pLx), (int)lroundf(pLy));
    // Start from left normal vector (nx, ny), rotate by d = PI/(segs+1)
    float cosd = cosf((float)M_PI / (float)(segs + 1));
    float sind = sinf((float)M_PI / (float)(segs + 1));
    float vx = nx, vy = ny; // unit left normal
    for (int i = 1; i <= segs; ++i) {
      // rotate v by d: v' = R(d)*v
      float rvx = vx * cosd - vy * sind;
      float rvy = vx * sind + vy * cosd;
      vx = rvx;
      vy = rvy;
      float x = px + vx * hw;
      float y = py + vy * hw;
      pts[n++] = pack_point_i16((int)lroundf(x), (int)lroundf(y));
    }
    pts[n++] = pack_point_i16((int)lroundf(pRx), (int)lroundf(pRy));
    vg_path_t p = {
        .points = pts, .size = (size_t)n, .capacity = (size_t)n, .next = NULL};
    vg_fill_path_clipped(&p, NULL, frame, color, VG_FILL_EVEN_ODD, clip_x0,
                         clip_y0, clip_x1, clip_y1);
  }
}

static void draw_stroke_path(const vg_shape_t *shape, const vg_path_t *path,
                             pix_frame_t *frame, uint32_t color, int clip_x0,
                             int clip_y0, int clip_x1, int clip_y1) {
  float w = shape->stroke_width > 0 ? shape->stroke_width : 1.0f;
  float hw = w * 0.5f;
  if (!path || path->size < 2)
    return;
  bool closed = false;
  if (path->size >= 2) {
    if (path->points[0] == path->points[path->size - 1]) {
      closed = true;
    }
  }
  // Iterate points with prev/current/next to build joins and caps
  for (size_t j = 1; j < path->size; ++j) {
    int32_t pt0 = path->points[j - 1];
    int32_t pt1 = path->points[j];
    float x0 = (pt0 >> 16) & 0xFFFF, y0 = pt0 & 0xFFFF;
    float x1 = (pt1 >> 16) & 0xFFFF, y1 = pt1 & 0xFFFF;
    if (shape->transform) {
      float tx0, ty0, tx1, ty1;
      vg_transform_point(shape->transform, x0, y0, &tx0, &ty0);
      vg_transform_point(shape->transform, x1, y1, &tx1, &ty1);
      x0 = tx0;
      y0 = ty0;
      x1 = tx1;
      y1 = ty1;
    }
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f)
      continue;
    float ux = dx / len, uy = dy / len; // direction
    float nx = -uy, ny = ux;            // left normal
    // Rectangle body
    float p0Lx = x0 + nx * hw, p0Ly = y0 + ny * hw;
    float p0Rx = x0 - nx * hw, p0Ry = y0 - ny * hw;
    float p1Lx = x1 + nx * hw, p1Ly = y1 + ny * hw;
    float p1Rx = x1 - nx * hw, p1Ry = y1 - ny * hw;
    fill_quad_clip(frame, p0Rx, p0Ry, p0Lx, p0Ly, p1Lx, p1Ly, p1Rx, p1Ry, color,
                   clip_x0, clip_y0, clip_x1, clip_y1);

    // Start cap
    if (!closed && j == 1) {
      if (shape->stroke_cap == VG_CAP_SQUARE) {
        // extend backward
        cap_end(frame, x0, y0, nx, ny, -ux, -uy, hw, color, VG_CAP_SQUARE,
                clip_x0, clip_y0, clip_x1, clip_y1);
      } else if (shape->stroke_cap == VG_CAP_ROUND) {
        cap_end(frame, x0, y0, nx, ny, -ux, -uy, hw, color, VG_CAP_ROUND,
                clip_x0, clip_y0, clip_x1, clip_y1);
      }
    }

    // End cap or prepare join with next segment
    if (!closed && j == path->size - 1) {
      if (shape->stroke_cap == VG_CAP_SQUARE) {
        cap_end(frame, x1, y1, nx, ny, ux, uy, hw, color, VG_CAP_SQUARE,
                clip_x0, clip_y0, clip_x1, clip_y1);
      } else if (shape->stroke_cap == VG_CAP_ROUND) {
        cap_end(frame, x1, y1, nx, ny, ux, uy, hw, color, VG_CAP_ROUND, clip_x0,
                clip_y0, clip_x1, clip_y1);
      }
    } else {
      // Join with next segment
      int32_t pt2 =
          (j == path->size - 1) ? path->points[1] : path->points[j + 1];
      float x2 = (pt2 >> 16) & 0xFFFF, y2 = pt2 & 0xFFFF;
      if (shape->transform) {
        float tx2, ty2;
        vg_transform_point(shape->transform, x2, y2, &tx2, &ty2);
        x2 = tx2;
        y2 = ty2;
      }
      float dx2 = x2 - x1, dy2 = y2 - y1;
      float len2 = sqrtf(dx2 * dx2 + dy2 * dy2);
      if (len2 > 1e-6f) {
        float ux2 = dx2 / len2, uy2 = dy2 / len2;
        float nx2 = -uy2, ny2 = ux2;
        // Bevel join: fill wedge on outer side
        float cross = ux * uy2 - uy * ux2; // + left turn, - right turn
        if (shape->stroke_join == VG_JOIN_ROUND) {
          // Round join: approximate arc on outer side using incremental
          // rotation
          const int segs = 10;
          if (cross >= 0.0f) {
            // outer on left: rotate from n to n2
            float ang0 = atan2f(ny, nx);
            float ang1 = atan2f(ny2, nx2);
            while (ang1 < ang0)
              ang1 += 2.0f * (float)M_PI;
            float d = (ang1 - ang0) / (float)(segs + 1);
            float cosd = cosf(d), sind = sinf(d);
            float vx = nx, vy = ny;
            vg_point_t pts[10 + 3];
            int npts = 0;
            pts[npts++] =
                pack_point_i16((int)lroundf(p1Lx), (int)lroundf(p1Ly));
            for (int k = 1; k <= segs; ++k) {
              float rvx = vx * cosd - vy * sind;
              float rvy = vx * sind + vy * cosd;
              vx = rvx;
              vy = rvy;
              float x = x1 + vx * hw;
              float y = y1 + vy * hw;
              pts[npts++] = pack_point_i16((int)lroundf(x), (int)lroundf(y));
            }
            float p1L2x = x1 + nx2 * hw, p1L2y = y1 + ny2 * hw;
            pts[npts++] =
                pack_point_i16((int)lroundf(p1L2x), (int)lroundf(p1L2y));
            vg_path_t p = {.points = pts,
                           .size = (size_t)npts,
                           .capacity = (size_t)npts,
                           .next = NULL};
            vg_fill_path_clipped(&p, NULL, frame, color, VG_FILL_EVEN_ODD,
                                 clip_x0, clip_y0, clip_x1, clip_y1);
          } else {
            // outer on right: rotate from -n to -n2
            float ang0 = atan2f(-ny, -nx);
            float ang1 = atan2f(-ny2, -nx2);
            while (ang1 > ang0)
              ang1 -= 2.0f * (float)M_PI;
            float d = (ang1 - ang0) / (float)(segs + 1);
            float cosd = cosf(d), sind = sinf(d);
            float vx = -nx, vy = -ny;
            vg_point_t pts[10 + 3];
            int npts = 0;
            pts[npts++] =
                pack_point_i16((int)lroundf(p1Rx), (int)lroundf(p1Ry));
            for (int k = 1; k <= segs; ++k) {
              float rvx = vx * cosd - vy * sind;
              float rvy = vx * sind + vy * cosd;
              vx = rvx;
              vy = rvy;
              float x = x1 + vx * hw;
              float y = y1 + vy * hw;
              pts[npts++] = pack_point_i16((int)lroundf(x), (int)lroundf(y));
            }
            float p1R2x = x1 - nx2 * hw, p1R2y = y1 - ny2 * hw;
            pts[npts++] =
                pack_point_i16((int)lroundf(p1R2x), (int)lroundf(p1R2y));
            vg_path_t p = {.points = pts,
                           .size = (size_t)npts,
                           .capacity = (size_t)npts,
                           .next = NULL};
            vg_fill_path_clipped(&p, NULL, frame, color, VG_FILL_EVEN_ODD,
                                 clip_x0, clip_y0, clip_x1, clip_y1);
          }
        } else if (shape->stroke_join == VG_JOIN_MITER) {
          // Miter join with limit using no-trig test.
          // For normalized directions u and v: miter_ratio = 1/sin(θ/2) =
          // sqrt(2/(1 - dot(u,v)))
          float dot = ux * ux2 + uy * uy2;
          if (dot < -1.0f)
            dot = -1.0f;
          if (dot > 1.0f)
            dot = 1.0f;
          float denom = 1.0f - dot; // small when segments are nearly colinear
          float miter_ratio = (denom > 1e-6f) ? sqrtf(2.0f / denom) : INFINITY;
          bool use_bevel = !(miter_ratio <= shape->miter_limit);
          // Also require non-parallel directions for intersection
          // Compute miter tip as intersection of offset outer lines
          if (!use_bevel) {
            float Px, Py, Qx, Qy; // points on the two outer offset lines
            if (cross >= 0.0f) {
              // left outer side
              float p1L2x = x1 + nx2 * hw, p1L2y = y1 + ny2 * hw;
              Px = p1Lx;
              Py = p1Ly;
              Qx = p1L2x;
              Qy = p1L2y;
            } else {
              // right outer side
              float p1R2x = x1 - nx2 * hw, p1R2y = y1 - ny2 * hw;
              Px = p1Rx;
              Py = p1Ry;
              Qx = p1R2x;
              Qy = p1R2y;
            }
            // r = direction of first segment, s = direction of second
            float r_x = ux, r_y = uy;
            float s_x = ux2, s_y = uy2;
            float denom = r_x * s_y - r_y * s_x; // cross(r, s)
            if (fabsf(denom) < 1e-6f) {
              use_bevel = true; // nearly parallel; avoid huge miters
            } else {
              float qpx = Qx - Px, qpy = Qy - Py;
              float t =
                  (qpx * s_y - qpy * s_x) / denom; // cross(Q-P, s)/cross(r,s)
              float Ix = Px + t * r_x;
              float Iy = Py + t * r_y;
              if (cross >= 0.0f) {
                // Fill wedge on left outer: p1L -> I -> p1L2
                float p1L2x = x1 + nx2 * hw, p1L2y = y1 + ny2 * hw;
                fill_triangle_clip(frame, p1Lx, p1Ly, Ix, Iy, p1L2x, p1L2y,
                                   color, clip_x0, clip_y0, clip_x1, clip_y1);
              } else {
                // Fill wedge on right outer: p1R -> I -> p1R2
                float p1R2x = x1 - nx2 * hw, p1R2y = y1 - ny2 * hw;
                fill_triangle_clip(frame, p1Rx, p1Ry, Ix, Iy, p1R2x, p1R2y,
                                   color, clip_x0, clip_y0, clip_x1, clip_y1);
              }
            }
          }
          if (use_bevel) {
            // Fallback to bevel
            if (cross >= 0.0f) {
              float p1L2x = x1 + nx2 * hw, p1L2y = y1 + ny2 * hw;
              fill_triangle_clip(frame, x1, y1, p1Lx, p1Ly, p1L2x, p1L2y, color,
                                 clip_x0, clip_y0, clip_x1, clip_y1);
            } else {
              float p1R2x = x1 - nx2 * hw, p1R2y = y1 - ny2 * hw;
              fill_triangle_clip(frame, x1, y1, p1Rx, p1Ry, p1R2x, p1R2y, color,
                                 clip_x0, clip_y0, clip_x1, clip_y1);
            }
          }
        } else { // BEVEL (default)
          if (cross >= 0.0f) {
            // left outer
            float p1L2x = x1 + nx2 * hw, p1L2y = y1 + ny2 * hw;
            fill_triangle_clip(frame, x1, y1, p1Lx, p1Ly, p1L2x, p1L2y, color,
                               clip_x0, clip_y0, clip_x1, clip_y1);
          } else {
            // right outer
            float p1R2x = x1 - nx2 * hw, p1R2y = y1 - ny2 * hw;
            fill_triangle_clip(frame, x1, y1, p1Rx, p1Ry, p1R2x, p1R2y, color,
                               clip_x0, clip_y0, clip_x1, clip_y1);
          }
        }
      }
    }
  }
}

void vg_canvas_render(const vg_canvas_t *canvas, pix_frame_t *frame,
                      uint32_t fallback_stroke_color) {
  if (!canvas || !frame || !frame->pixels || !frame->set_pixel)
    return;
  for (size_t i = 0; i < canvas->size; ++i) {
    const vg_shape_t *shape = canvas->shapes[i];
    uint32_t stroke = (shape->stroke_color == VG_COLOR_NONE)
                          ? fallback_stroke_color
                          : shape->stroke_color;
    const vg_path_t *path = &shape->path;
    // Viewport culling for stroke: inflate by half width and AA (~1px)
    float minx, miny, maxx, maxy;
    compute_path_aabb_screen(path, shape->transform, &minx, &miny, &maxx,
                             &maxy);
    float pad = (shape->stroke_width > 0 ? shape->stroke_width * 0.5f : 0.5f) +
                1.0f; // add 1px for AA
    aabb_expand(pad, &minx, &miny, &maxx, &maxy);
    if (!aabb_intersects_frame(minx, miny, maxx, maxy, frame)) {
      continue; // skip shape entirely
    }
    while (path) {
      draw_stroke_path(shape, path, frame, stroke, 0, 0, (int)frame->width - 1,
                       (int)frame->height - 1);
      path = path->next;
    }
  }
}

vg_canvas_t vg_canvas_init(size_t capacity) {
  vg_canvas_t canvas;
  canvas.shapes = (vg_shape_t **)VG_MALLOC(capacity * sizeof(vg_shape_t *));
  if (!canvas.shapes) {
    canvas.size = 0;
    canvas.capacity = 0;
    return canvas;
  }
  canvas.size = 0;
  canvas.capacity = capacity;
  return canvas;
}

bool vg_canvas_append(vg_canvas_t *canvas, vg_shape_t *shape) {
  if (canvas->size == canvas->capacity) {
    return false;
  }
  canvas->shapes[canvas->size++] = shape;
  return true;
}

void vg_canvas_clear(vg_canvas_t *canvas) {
  if (!canvas)
    return;
  canvas->size = 0;
}

void vg_canvas_destroy(vg_canvas_t *canvas) {
  if (!canvas)
    return;
  if (canvas->shapes) {
    VG_FREE(canvas->shapes);
    canvas->shapes = NULL;
  }
  canvas->size = 0;
  canvas->capacity = 0;
}

void vg_canvas_render_all(const vg_canvas_t *canvas, pix_frame_t *frame,
                          uint32_t fallback_fill_color,
                          uint32_t fallback_stroke_color, vg_fill_rule_t rule) {
  if (!canvas || !frame)
    return;
  // Tile-based culling: iterate tiles and clip rasterization per tile.
  const int TILE = 64;
  int tiles_x = (int)((frame->width + TILE - 1) / TILE);
  int tiles_y = (int)((frame->height + TILE - 1) / TILE);
  // Precompute base AABB for each shape once (in screen space)
  size_t n = canvas->size;
  float *base_minx = (float *)VG_MALLOC(sizeof(float) * n);
  float *base_miny = (float *)VG_MALLOC(sizeof(float) * n);
  float *base_maxx = (float *)VG_MALLOC(sizeof(float) * n);
  float *base_maxy = (float *)VG_MALLOC(sizeof(float) * n);
  if (!base_minx || !base_miny || !base_maxx || !base_maxy) {
    if (base_minx)
      VG_FREE(base_minx);
    if (base_miny)
      VG_FREE(base_miny);
    if (base_maxx)
      VG_FREE(base_maxx);
    if (base_maxy)
      VG_FREE(base_maxy);
    return;
  }
  for (size_t i = 0; i < n; ++i) {
    float mnx, mny, mxx, mxy;
    compute_path_aabb_screen(&canvas->shapes[i]->path,
                             canvas->shapes[i]->transform, &mnx, &mny, &mxx,
                             &mxy);
    base_minx[i] = mnx;
    base_miny[i] = mny;
    base_maxx[i] = mxx;
    base_maxy[i] = mxy;
  }
  for (int ty = 0; ty < tiles_y; ++ty) {
    int clip_y0 = ty * TILE;
    int clip_y1 = clip_y0 + TILE - 1;
    if (clip_y1 >= (int)frame->height)
      clip_y1 = (int)frame->height - 1;
    for (int tx = 0; tx < tiles_x; ++tx) {
      int clip_x0 = tx * TILE;
      int clip_x1 = clip_x0 + TILE - 1;
      if (clip_x1 >= (int)frame->width)
        clip_x1 = (int)frame->width - 1;
      // Per-tile, process shapes whose padded AABB intersects this tile
      for (size_t i = 0; i < n; ++i) {
        const vg_shape_t *shape = canvas->shapes[i];
        uint32_t fill = (shape->fill_color == VG_COLOR_NONE)
                            ? fallback_fill_color
                            : shape->fill_color;
        uint32_t stroke = (shape->stroke_color == VG_COLOR_NONE)
                              ? fallback_stroke_color
                              : shape->stroke_color;
        // Use precomputed base AABB
        float minx = base_minx[i], miny = base_miny[i], maxx = base_maxx[i],
              maxy = base_maxy[i];
        // Tile rect as floats
        float tminx = (float)clip_x0, tminy = (float)clip_y0;
        float tmaxx = (float)clip_x1, tmaxy = (float)clip_y1;

        // Fill path if intersects this tile
        if (shape->has_fill && fill != VG_COLOR_NONE) {
          float fminx = minx, fminy = miny, fmaxx = maxx, fmaxy = maxy;
          aabb_expand(1.0f, &fminx, &fminy, &fmaxx, &fmaxy);
          bool hit = !(fmaxx < tminx || fmaxy < tminy || fminx > tmaxx ||
                       fminy > tmaxy);
          if (hit) {
            vg_fill_path_clipped(&shape->path, shape->transform, frame, fill,
                                 rule, clip_x0, clip_y0, clip_x1, clip_y1);
          }
        }
        // Stroke path if intersects this tile
        if (shape->has_stroke && stroke != VG_COLOR_NONE) {
          float sminx = minx, sminy = miny, smaxx = maxx, smaxy = maxy;
          float pad =
              (shape->stroke_width > 0 ? shape->stroke_width * 0.5f : 0.5f) +
              1.0f;
          aabb_expand(pad, &sminx, &sminy, &smaxx, &smaxy);
          bool hit = !(smaxx < tminx || smaxy < tminy || sminx > tmaxx ||
                       sminy > tmaxy);
          if (hit) {
            const vg_path_t *path = &shape->path;
            while (path) {
              draw_stroke_path(shape, path, frame, stroke, clip_x0, clip_y0,
                               clip_x1, clip_y1);
              path = path->next;
            }
          }
        }
      }
    }
  }
  VG_FREE(base_minx);
  VG_FREE(base_miny);
  VG_FREE(base_maxx);
  VG_FREE(base_maxy);
}
