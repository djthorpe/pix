#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "tiger.h"
#include <vg/font.h>
#include <vg/vg.h>

// Global state definitions
vg_canvas_t g_canvas;            // holds all tiger shapes
vg_transform_t g_tiger_xform;    // shared transform (scale + translate)
static size_t g_shape_count = 0; // number of shapes in canvas
float g_user_scale = 1.0f;       // interactive zoom factor (1 = fit)
float g_user_pan_x = 0.0f; // interactive pan in device pixels (post-scale)
float g_user_pan_y = 0.0f;
float g_user_rotate = 0.0f; // rotation (radians) about artwork center

typedef struct {
  float x, y;
} f2; // minimal helper

// Internal forward
static void build_tiger_shapes(void);

// Parse tiger command stream and build vg shapes (internal)
static void build_tiger_shapes(void) {
  const char *cmds = tigerCommands;
  const float *pts = tigerPoints;
  int c = 0; // index into cmds
  int p = 0; // index into pts
  // First pass: count paths
  int path_count = 0;
  while (c < tigerCommandCount) {
    c += 4; // header flags
    p += 8; // miter, strokeWidth, stroke RGB, fill RGB
    int elements = (int)pts[p++];
    for (int e = 0; e < elements; ++e) {
      char op = cmds[c++];
      switch (op) {
      case 'M':
      case 'L':
        p += 2;
        break;
      case 'C':
        p += 6;
        break;
      case 'E':
        break;
      default:
        break;
      }
    }
    path_count++;
  }
  // Reserve one extra slot for poem text overlay.
  g_shape_count = (size_t)path_count;
  g_canvas = vg_canvas_init(g_shape_count + 1);
  // Second pass: build shapes
  c = 0;
  p = 0;
  size_t si = 0;
  while (c < tigerCommandCount && si < g_shape_count) {
    if (si >= g_canvas.capacity)
      break;
    vg_shape_t *s = vg_canvas_append(&g_canvas);
    if (!s)
      break; // capacity or alloc failure
    // Defaults
    vg_shape_set_transform(s, &g_tiger_xform);
    vg_shape_set_fill_color(s, PIX_COLOR_NONE);
    vg_shape_set_stroke_color(s, PIX_COLOR_NONE);
    vg_shape_set_stroke_width(s, 1.0f);
    vg_shape_set_stroke_cap(s, VG_CAP_BUTT);
    vg_shape_set_stroke_join(s, VG_JOIN_BEVEL);
    vg_shape_set_miter_limit(s, 4.0f);
    vg_shape_set_fill_rule(s, VG_FILL_EVEN_ODD); // default
    // Header: fill rule
    vg_fill_rule_t fill_rule = VG_FILL_EVEN_ODD;
    bool fill = false;
    char h;
    h = cmds[c++];
    if (h == 'F') {
      fill = true;
      fill_rule = VG_FILL_EVEN_ODD;
    } else if (h == 'E') {
      fill = true;
      fill_rule = VG_FILL_EVEN_ODD;
    }
    // Stroke present?
    bool stroke = false;
    h = cmds[c++];
    if (h == 'S')
      stroke = true;
    // Cap
    h = cmds[c++];
    if (h == 'R')
      vg_shape_set_stroke_cap(s, VG_CAP_ROUND);
    else if (h == 'S')
      vg_shape_set_stroke_cap(s, VG_CAP_SQUARE);
    else
      vg_shape_set_stroke_cap(s, VG_CAP_BUTT);
    // Join
    h = cmds[c++];
    if (h == 'R')
      vg_shape_set_stroke_join(s, VG_JOIN_ROUND);
    else if (h == 'M')
      vg_shape_set_stroke_join(s, VG_JOIN_MITER);
    else
      vg_shape_set_stroke_join(s, VG_JOIN_BEVEL);
    // Stroke attrs
    vg_shape_set_miter_limit(s, pts[p++]);
    vg_shape_set_stroke_width(s, pts[p++]);
    // Stroke color RGB
    float sr = pts[p++], sg = pts[p++], sb = pts[p++];
    // Fill color RGB
    float fr = pts[p++], fg = pts[p++], fb = pts[p++];
    // Build colors (opaque)
    if (stroke) {
      vg_shape_set_stroke_color(s, 0xFF000000u |
                                       ((uint32_t)lroundf(sr * 255.0f) << 16) |
                                       ((uint32_t)lroundf(sg * 255.0f) << 8) |
                                       (uint32_t)lroundf(sb * 255.0f));
    }
    if (fill) {
      vg_shape_set_fill_color(s, 0xFF000000u |
                                     ((uint32_t)lroundf(fr * 255.0f) << 16) |
                                     ((uint32_t)lroundf(fg * 255.0f) << 8) |
                                     (uint32_t)lroundf(fb * 255.0f));
      vg_shape_set_fill_rule(s, fill_rule);
    }
    // Elements
    int elements = (int)pts[p++];
    vg_shape_path_clear(s, 128);
    f2 cur = {0, 0};
    f2 start = {0, 0};
    vg_path_t *seg = vg_shape_path(s);
#define EMIT_POINT(X, Y)                                                       \
  do {                                                                         \
    int16_t ix = (int16_t)lroundf((X));                                        \
    int16_t iy = (int16_t)lroundf((Y));                                        \
    if (seg->size == seg->capacity) {                                          \
      vg_path_t *n = (vg_path_t *)VG_MALLOC(sizeof(vg_path_t));                \
      if (n) {                                                                 \
        n->capacity = seg->capacity;                                           \
        n->points = VG_MALLOC(sizeof(pix_point_t) * n->capacity);              \
        if (!n->points) {                                                      \
          VG_FREE(n);                                                          \
        } else {                                                               \
          n->size = 0;                                                         \
          n->next = NULL;                                                      \
          seg->next = n;                                                       \
          seg = n;                                                             \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    if (seg->size < seg->capacity) {                                           \
      seg->points[seg->size++] = (pix_point_t){ix, iy};                        \
    }                                                                          \
  } while (0)
    struct CubicItem {
      f2 a, b, c, d;
      int depth;
    };
    for (int e = 0; e < elements; ++e) {
      char op = cmds[c++];
      if (op == 'M') {
        cur.x = pts[p++];
        cur.y = pts[p++];
        start = cur;
        if (seg->size > 0) {
          vg_path_t *n = (vg_path_t *)VG_MALLOC(sizeof(vg_path_t));
          if (n) {
            n->capacity = seg->capacity; // reuse current segment capacity
            n->points = VG_MALLOC(sizeof(pix_point_t) * n->capacity);
            if (!n->points) {
              VG_FREE(n);
            } else {
              n->size = 0;
              n->next = NULL;
              seg->next = n;
              seg = n;
            }
          }
        }
        EMIT_POINT(cur.x, cur.y); // first point of new subpath
      } else if (op == 'L') {
        cur.x = pts[p++];
        cur.y = pts[p++];
        EMIT_POINT(cur.x, cur.y);
      } else if (op == 'C') {
        f2 c1 = {pts[p], pts[p + 1]};
        p += 2;
        f2 c2 = {pts[p], pts[p + 1]};
        p += 2;
        f2 d = {pts[p], pts[p + 1]};
        p += 2;
        struct CubicItem stack[64];
        int sp = 0;
        stack[sp++] = (struct CubicItem){cur, c1, c2, d, 0};
        while (sp) {
          struct CubicItem it = stack[--sp];
          f2 a = it.a, b = it.b, c2_ = it.c, d_ = it.d;
          float d1 = 0.0f, d2 = 0.0f;
          {
            float vx = d_.x - a.x, vy = d_.y - a.y;
            float wx = b.x - a.x, wy = b.y - a.y;
            float L2 = vx * vx + vy * vy;
            if (L2 < 1e-12f)
              d1 = wx * wx + wy * wy;
            else {
              float t = (wx * vx + wy * vy) / L2;
              if (t < 0)
                t = 0;
              else if (t > 1)
                t = 1;
              float px = a.x + vx * t, py = a.y + vy * t;
              float dx = b.x - px, dy = b.y - py;
              d1 = dx * dx + dy * dy;
            }
          }
          {
            float vx = d_.x - a.x, vy = d_.y - a.y;
            float wx = c2_.x - a.x, wy = c2_.y - a.y;
            float L2 = vx * vx + vy * vy;
            if (L2 < 1e-12f)
              d2 = wx * wx + wy * wy;
            else {
              float t = (wx * vx + wy * vy) / L2;
              if (t < 0)
                t = 0;
              else if (t > 1)
                t = 1;
              float px = a.x + vx * t, py = a.y + vy * t;
              float dx = c2_.x - px, dy = c2_.y - py;
              d2 = dx * dx + dy * dy;
            }
          }
          const float FLATTEN_TOL = 0.25f;
          if ((d1 <= FLATTEN_TOL && d2 <= FLATTEN_TOL) || it.depth > 18) {
            EMIT_POINT(d_.x, d_.y);
            continue;
          }
          f2 q0 = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
          f2 q1 = {(b.x + c2_.x) * 0.5f, (b.y + c2_.y) * 0.5f};
          f2 q2 = {(c2_.x + d_.x) * 0.5f, (c2_.y + d_.y) * 0.5f};
          f2 r0 = {(q0.x + q1.x) * 0.5f, (q0.y + q1.y) * 0.5f};
          f2 r1 = {(q1.x + q2.x) * 0.5f, (q1.y + q2.y) * 0.5f};
          f2 s = {(r0.x + r1.x) * 0.5f, (r0.y + r1.y) * 0.5f};
          if (sp < 63)
            stack[sp++] = (struct CubicItem){s, r1, q2, d_, it.depth + 1};
          if (sp < 63)
            stack[sp++] = (struct CubicItem){a, q0, r0, s, it.depth + 1};
        }
        cur = d;
      } else if (op == 'E') {
        int16_t sx = (int16_t)lroundf(start.x), sy = (int16_t)lroundf(start.y);
        pix_point_t last = seg->points[seg->size - 1];
        if (sx != last.x || sy != last.y)
          EMIT_POINT(start.x, start.y);
        cur = start;
      }
    }
    if (fill) {
      // per-subpath closure handled by 'E' commands
    }
    // path already written into shape
#undef EMIT_POINT
    si++; // already appended
    (void)fill_rule;
  }
  g_shape_count = si;
  // Add poem text that participates in the artwork transform (same as paths)
  const char *poem = "Tyger Tyger, burning bright,\n"
                     "In the forests of the night;\n"
                     "What immortal hand or eye,\n"
                     "Could frame thy fearful symmetry?";
  if (g_canvas.size < g_canvas.capacity) {
    float text_w = 0.0f;
    // Use native 5x7 geometry (pixel_size=7 => scale=1) to avoid double
    // scaling.
    float pixel_size = 7.0f;
    vg_shape_t *text_shape = vg_font_get_text_shape_cached(
        &vg_font_tiny5x7, poem, 0xFF000000u, pixel_size, 1.0f, &text_w);
    if (text_shape) {
      // Retrieve and bake the font's internal scaling transform (if any)
      const vg_transform_t *font_xf =
          vg_shape_get_transform(text_shape); // likely NULL
                                              // Offset location in tiger object
                                              // space (place near top-left)
      float ox = tigerMinX + 10.0f; // integer offsets keep grid alignment
      float oy = tigerMinY + 10.0f;
      // Iterate path segments and scale + translate points into object space
      // First pass: find maximum y (font builds with y increasing downward)
      int16_t max_py = 0;
      for (vg_path_t *s2 = vg_shape_path(text_shape); s2; s2 = s2->next) {
        for (size_t pi = 0; pi < s2->size; ++pi) {
          int16_t py = s2->points[pi].y;
          if (py > max_py)
            max_py = py;
        }
      }
      // We treat max_py as the exclusive overall height (since each rectangle
      // uses bottom-exclusive coordinates). To flip while preserving exact
      // pixel coverage, transform each rectangle's top (inclusive) and bottom
      // (exclusive) separately: top y0 -> (max_py - (y_bot)), bottom y_bot ->
      // (max_py - (y_top)). This keeps rectangles bottom‑exclusive after flip
      // so no rows are lost (fixes clipped descenders / 's').
      vg_path_t *seg_font = vg_shape_path(text_shape);
      while (seg_font) {
        if (seg_font->size ==
            5) { // expected rectangle path: pt0..pt4 (pt4==pt0)
          pix_point_t p0 = seg_font->points[0];
          pix_point_t p2 = seg_font->points[2];
          int16_t y_top = p0.y;
          int16_t y_bot = p2.y; // bottom-exclusive
          int16_t new_y_top = (int16_t)(max_py - y_bot);
          int16_t new_y_bot = (int16_t)(max_py - y_top);
          for (size_t pi = 0; pi < seg_font->size; ++pi) {
            pix_point_t pt = seg_font->points[pi];
            int16_t ny = (pt.y == y_top) ? new_y_top
                                         : (pt.y == y_bot ? new_y_bot : pt.y);
            float fx = (float)pt.x + ox;
            float fy = (float)ny + oy;
            int16_t nx = (int16_t)lroundf(fx);
            int16_t nfy = (int16_t)lroundf(fy);
            seg_font->points[pi] = (pix_point_t){nx, nfy};
          }
        } else {
          for (size_t pi = 0; pi < seg_font->size; ++pi) {
            pix_point_t pt = seg_font->points[pi];
            int16_t ny = (int16_t)(max_py - pt.y);
            float fx = (float)pt.x + ox;
            float fy = (float)ny + oy;
            int16_t nx = (int16_t)lroundf(fx);
            int16_t nfy = (int16_t)lroundf(fy);
            seg_font->points[pi] = (pix_point_t){nx, nfy};
          }
        }
        seg_font = seg_font->next;
      }
      // Free the font scaling transform (we baked it) and reference global xf
      if (font_xf) {
        VG_FREE((void *)font_xf);
      }
      vg_shape_set_transform(text_shape, &g_tiger_xform);
      vg_shape_set_fill_rule(text_shape, VG_FILL_EVEN_ODD_RAW);
      // Manual append (external shape not in pool)
      g_canvas.shapes[g_canvas.size++] = text_shape;
    }
  }
}

void free_tiger_shapes(void) {
  vg_canvas_destroy(&g_canvas);
  g_shape_count = 0;
}

void update_transform(int w, int h) {
  float bw = (tigerMaxX - tigerMinX);
  float bh = (tigerMaxY - tigerMinY);
  if (bw <= 0 || bh <= 0) {
    vg_transform_identity(&g_tiger_xform);
    return;
  }
  float margin = 4.0f;
  float scale_x = (float)(w - 2 * margin) / bw;
  float scale_y = (float)(h - 2 * margin) / bh;
  float base_scale = scale_x < scale_y ? scale_x : scale_y;
  if (base_scale <= 0)
    base_scale = 1.0f;
  float scale = base_scale * g_user_scale;
  if (scale <= 0)
    scale = 1.0f;
  float scaled_w = bw * scale;
  float scaled_h = bh * scale;
  float offset_x = ((float)w - scaled_w) * 0.5f + g_user_pan_x;
  float offset_y = ((float)h - scaled_h) * 0.5f + g_user_pan_y;
  // Build composite: T2 * F * S * C_back * R * C_to_origin * T1
  // Where rotation pivot is artwork center in object space.
  float center_x = (tigerMinX + tigerMaxX) * 0.5f;
  float center_y = (tigerMinY + tigerMaxY) * 0.5f;
  vg_transform_t T1, Cneg, R, Cpos, S, F, T2, tmp1, tmp2, tmp3, tmp4, tmp5;
  vg_transform_translate(&T1, -tigerMinX, -tigerMinY);
  vg_transform_translate(&Cneg, -center_x + tigerMinX, -center_y + tigerMinY);
  // (center relative to min translated to origin) => simpler: translate by
  // -(center-min) Compute (center-min):
  vg_transform_translate(&Cneg, -(center_x - tigerMinX),
                         -(center_y - tigerMinY));
  vg_transform_rotate(&R, g_user_rotate);
  vg_transform_translate(&Cpos, (center_x - tigerMinX), (center_y - tigerMinY));
  vg_transform_scale(&S, scale, scale);
  vg_transform_scale(&F, 1.0f, -1.0f);
  vg_transform_translate(&T2, offset_x, offset_y + scaled_h);
  // tmp1 = C_to_origin * T1
  vg_transform_multiply(&tmp1, &Cneg, &T1);
  // tmp2 = R * tmp1
  vg_transform_multiply(&tmp2, &R, &tmp1);
  // tmp3 = C_back * tmp2
  vg_transform_multiply(&tmp3, &Cpos, &tmp2);
  // tmp4 = S * tmp3
  vg_transform_multiply(&tmp4, &S, &tmp3);
  // tmp5 = F * tmp4
  vg_transform_multiply(&tmp5, &F, &tmp4);
  // final = T2 * tmp5
  vg_transform_multiply(&g_tiger_xform, &T2, &tmp5);
}

vg_canvas_t tiger_build_canvas(vg_shape_t ***shapes_out, size_t *count_out) {
  if (g_canvas.shapes == NULL || g_canvas.size == 0) {
    build_tiger_shapes();
  }
  if (shapes_out)
    *shapes_out = g_canvas.shapes; // expose internal array (owned by canvas)
  if (count_out)
    *count_out = g_shape_count;
  return g_canvas;
}
