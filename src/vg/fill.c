#include "pix/frame.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <vg/canvas.h>
#include <vg/fill.h>
#include <vg/path.h>
#include <vg/shape.h>
#include <vg/transform.h>

// Edge for scanline fill
typedef struct {
  int y_min, y_max; // inclusive range [y_min, y_max]
  float x0, y0;     // edge start (y0 == y_min)
  float inv_slope;  // dx/dy
  int wind;         // +1 for upward edge, -1 for downward (for NON_ZERO rule)
} vg_edge_t;

// Intersection record for scanline fill
typedef struct {
  float x;
  int w;
} xi_t;
static int xi_cmp(const void *a, const void *b) {
  const xi_t *pa = (const xi_t *)a;
  const xi_t *pb = (const xi_t *)b;
  return (pa->x > pb->x) - (pa->x < pb->x);
}

// Persistent reusable buffers to reduce per-call allocations
static vg_edge_t *g_edges_buf = NULL;
static int g_edges_cap = 0; // in elements
static xi_t *g_xis_buf = NULL;
static int g_xis_cap = 0; // in elements

static inline bool ensure_edges_capacity(int need) {
  if (need <= g_edges_cap)
    return true;
  int new_cap = g_edges_cap ? g_edges_cap : 256;
  while (new_cap < need) {
    new_cap = new_cap + (new_cap >> 1); // 1.5x growth
    if (new_cap < 0)
      return false;
  }
  void *nb = realloc(g_edges_buf, (size_t)new_cap * sizeof(vg_edge_t));
  if (!nb)
    return false;
  g_edges_buf = (vg_edge_t *)nb;
  g_edges_cap = new_cap;
  return true;
}

static inline bool ensure_xis_capacity(int need) {
  if (need <= g_xis_cap)
    return true;
  int new_cap = g_xis_cap ? g_xis_cap : 256;
  while (new_cap < need) {
    new_cap = new_cap + (new_cap >> 1);
    if (new_cap < 0)
      return false;
  }
  void *nb = realloc(g_xis_buf, (size_t)new_cap * sizeof(xi_t));
  if (!nb)
    return false;
  g_xis_buf = (xi_t *)nb;
  g_xis_cap = new_cap;
  return true;
}

static void add_edge(vg_edge_t *edges, int *edge_count, float x0, float y0,
                     float x1, float y1) {
  // Ignore horizontal edges
  if ((int)y0 == (int)y1)
    return;
  int wind = (y1 > y0) ? +1 : -1;
  if (y0 > y1) {
    float tx = x0;
    x0 = x1;
    x1 = tx;
    float ty = y0;
    y0 = y1;
    y1 = ty;
  }
  vg_edge_t e;
  // Half-open coverage using pixel centers (y+0.5): include scanlines where
  // (y+0.5) >= y0 and (y+0.5) < y1
  e.y_min = (int)ceilf(y0 - 0.5f);
  e.y_max = (int)floorf(y1 - 0.5f);
  if (e.y_min > e.y_max)
    return;
  float dy = (y1 - y0);
  e.inv_slope = (x1 - x0) / dy;
  e.x0 = x0;
  e.y0 = y0;
  e.wind = wind;
  edges[(*edge_count)++] = e;
}

static void build_edges_from_path(const vg_path_t *path,
                                  const vg_transform_t *xform, vg_edge_t *edges,
                                  int *edge_count) {
  const vg_path_t *seg = path;
  while (seg) {
    for (size_t j = 1; j < seg->size; ++j) {
      int32_t p0 = seg->points[j - 1];
      int32_t p1 = seg->points[j];
      float x0 = (p0 >> 16) & 0xFFFF;
      float y0 = p0 & 0xFFFF;
      float x1 = (p1 >> 16) & 0xFFFF;
      float y1 = p1 & 0xFFFF;
      if (xform) {
        float tx0, ty0, tx1, ty1;
        vg_transform_point(xform, x0, y0, &tx0, &ty0);
        vg_transform_point(xform, x1, y1, &tx1, &ty1);
        x0 = tx0;
        y0 = ty0;
        x1 = tx1;
        y1 = ty1;
      }
      add_edge(edges, edge_count, x0, y0, x1, y1);
    }
    seg = seg->next;
  }
}

// xi_t and xi_cmp defined above

static inline void intersect_with_frame(int *x0, int *y0, int *x1, int *y1,
                                        const pix_frame_t *frame) {
  if (*x0 < 0)
    *x0 = 0;
  if (*y0 < 0)
    *y0 = 0;
  if (*x1 >= (int)frame->width)
    *x1 = (int)frame->width - 1;
  if (*y1 >= (int)frame->height)
    *y1 = (int)frame->height - 1;
}

void vg_fill_path_clipped(const vg_path_t *path, const vg_transform_t *xform,
                          pix_frame_t *frame, uint32_t color,
                          vg_fill_rule_t rule, int clip_x0, int clip_y0,
                          int clip_x1, int clip_y1) {
  if (!path || !frame)
    return;
  // Edge budget: worst case edges ~ total points
  int max_edges = (int)vg_path_count(path) + 8;
  if (max_edges <= 0)
    return;
  if (!ensure_edges_capacity(max_edges))
    return;
  vg_edge_t *edges = g_edges_buf;
  int edge_count = 0;
  build_edges_from_path(path, xform, edges, &edge_count);
  if (edge_count <= 0) {
    return;
  }
  // Expect caller to lock; if not locked, we likely have no pixels.
  if (!frame->pixels || !frame->set_pixel) {
    return;
  }

  // Determine y-range
  int y_min = INT32_MAX, y_max = INT32_MIN;
  for (int i = 0; i < edge_count; ++i) {
    if (edges[i].y_min < y_min)
      y_min = edges[i].y_min;
    if (edges[i].y_max > y_max)
      y_max = edges[i].y_max;
  }
  // Intersect scanline range with clip rect and frame
  int cx0 = clip_x0, cy0 = clip_y0, cx1 = clip_x1, cy1 = clip_y1;
  intersect_with_frame(&cx0, &cy0, &cx1, &cy1, frame);
  if (cy0 > cy1 || cx0 > cx1)
    return;
  if (y_min < cy0)
    y_min = cy0;
  if (y_max > cy1)
    y_max = cy1;
  if (y_min > y_max)
    return;

  // Active Edge Table intersections buffer (reused)
  if (!ensure_xis_capacity(edge_count)) {
    return;
  }
  xi_t *xis_buf = g_xis_buf;
  for (int y = y_min; y <= y_max; ++y) {
    int n = 0;
    xi_t *xis = xis_buf;
    for (int i = 0; i < edge_count; ++i) {
      if (y >= edges[i].y_min && y <= edges[i].y_max) {
        float x_at_y =
            edges[i].x0 + (float)(y + 0.5f - edges[i].y0) * edges[i].inv_slope;
        xis[n].x = x_at_y;
        xis[n].w = edges[i].wind;
        n++;
      }
    }
    if (n == 0)
      continue;
    qsort(xis, n, sizeof(xi_t), xi_cmp);
    // Coverage-based AA along X using overlap of [x, x+1) with fill interval
    // Compose alpha by scaling source alpha with coverage.
    uint8_t base_a = (uint8_t)((color >> 24) & 0xFF);
    if (rule == VG_FILL_EVEN_ODD) {
      for (int i = 0; i + 1 < n; i += 2) {
        float L = xis[i].x;
        float R = xis[i + 1].x;
        if (R <= L)
          continue;
        int startx = (int)floorf(L);
        int endx = (int)floorf(R);
        // Intersect with clip rect X range
        if (startx < cx0)
          startx = cx0;
        if (endx > cx1)
          endx = cx1;
        if (endx < 0 || startx >= (int)frame->width)
          continue;
        if (startx < 0)
          startx = 0;
        if (endx >= (int)frame->width)
          endx = (int)frame->width - 1;
        for (int x = startx; x <= endx; ++x) {
          float segL = (float)x;
          float segR = (float)(x + 1);
          float cov = R;
          if (cov > segR)
            cov = segR;
          float mn = L;
          if (mn < segL)
            mn = segL;
          cov -= mn; // length of overlap in [0,1]
          if (cov <= 0.0f)
            continue;
          if (cov >= 1.0f) {
            frame->set_pixel(frame, (size_t)x, (size_t)y, color);
          } else {
            if (base_a == 0)
              continue;
            uint8_t a = (uint8_t)lroundf((float)base_a * cov);
            uint32_t mod = (color & 0x00FFFFFFu) | ((uint32_t)a << 24);
            frame->set_pixel(frame, (size_t)x, (size_t)y, mod);
          }
        }
      }
    } else { // VG_FILL_NON_ZERO
      int wcount = 0;
      for (int i = 0; i + 1 < n; ++i) {
        wcount += xis[i].w;
        if (wcount != 0) {
          float L = xis[i].x;
          float R = xis[i + 1].x;
          if (R <= L)
            continue;
          int startx = (int)floorf(L);
          int endx = (int)floorf(R);
          if (startx < cx0)
            startx = cx0;
          if (endx > cx1)
            endx = cx1;
          if (endx < 0 || startx >= (int)frame->width)
            continue;
          if (startx < 0)
            startx = 0;
          if (endx >= (int)frame->width)
            endx = (int)frame->width - 1;
          for (int x = startx; x <= endx; ++x) {
            float segL = (float)x;
            float segR = (float)(x + 1);
            float cov = R;
            if (cov > segR)
              cov = segR;
            float mn = L;
            if (mn < segL)
              mn = segL;
            cov -= mn;
            if (cov <= 0.0f)
              continue;
            if (cov >= 1.0f) {
              frame->set_pixel(frame, (size_t)x, (size_t)y, color);
            } else {
              if (base_a == 0)
                continue;
              uint8_t a = (uint8_t)lroundf((float)base_a * cov);
              uint32_t mod = (color & 0x00FFFFFFu) | ((uint32_t)a << 24);
              frame->set_pixel(frame, (size_t)x, (size_t)y, mod);
            }
          }
        }
      }
    }
  }

  // Buffers are persistent; no frees here.
}

void vg_fill_path(const vg_path_t *path, const vg_transform_t *xform,
                  pix_frame_t *frame, uint32_t color, vg_fill_rule_t rule) {
  if (!frame)
    return;
  vg_fill_path_clipped(path, xform, frame, color, rule, 0, 0,
                       (int)frame->width - 1, (int)frame->height - 1);
}

void vg_fill_shape(const struct vg_shape_t *shape, pix_frame_t *frame,
                   uint32_t color, vg_fill_rule_t rule) {
  if (!shape || !frame)
    return;
  vg_fill_path(&shape->path, shape->transform, frame, color, rule);
}

void vg_canvas_render_fill(const struct vg_canvas_t *canvas, pix_frame_t *frame,
                           uint32_t color, vg_fill_rule_t rule) {
  if (!canvas || !frame)
    return;
  for (size_t i = 0; i < canvas->size; ++i) {
    vg_fill_shape(canvas->shapes[i], frame, color, rule);
  }
}
