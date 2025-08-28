#include <math.h>
#include <vg/vg.h>

static inline int32_t pack_i16(int x, int y) {
  return ((int32_t)(x & 0xFFFF) << 16) | (int32_t)(y & 0xFFFF);
}

static inline void shape_defaults(vg_shape_t *s) {
  s->transform = NULL;
  s->fill_color = VG_COLOR_NONE;
  s->stroke_color = VG_COLOR_NONE;
  s->stroke_width = 1.0f;
  s->stroke_cap = VG_CAP_BUTT;
  s->stroke_join = VG_JOIN_BEVEL;
  s->miter_limit = 4.0f;
  s->has_fill = true;
  s->has_stroke = true;
}

vg_path_t vg_path_make_rect(int x, int y, int w, int h) {
  vg_path_t p = vg_path_init(5);
  vg_path_append(&p, pack_i16(x, y));
  vg_path_append(&p, pack_i16(x + w, y));
  vg_path_append(&p, pack_i16(x + w, y + h));
  vg_path_append(&p, pack_i16(x, y + h));
  vg_path_append(&p, pack_i16(x, y));
  return p;
}

vg_path_t vg_path_make_circle(int cx, int cy, int r, int segments) {
  if (segments < 8)
    segments = 32;
  vg_path_t p = vg_path_init((size_t)segments + 1);
  for (int i = 0; i <= segments; ++i) {
    float t = (float)i / (float)segments * 2.0f * (float)M_PI;
    int x = cx + (int)lroundf(r * cosf(t));
    int y = cy + (int)lroundf(r * sinf(t));
    vg_path_append(&p, pack_i16(x, y));
  }
  return p;
}

vg_path_t vg_path_make_ellipse(int cx, int cy, int rx, int ry, int segments) {
  if (segments < 8)
    segments = 32;
  vg_path_t p = vg_path_init((size_t)segments + 1);
  for (int i = 0; i <= segments; ++i) {
    float t = (float)i / (float)segments * 2.0f * (float)M_PI;
    int x = cx + (int)lroundf(rx * cosf(t));
    int y = cy + (int)lroundf(ry * sinf(t));
    vg_path_append(&p, pack_i16(x, y));
  }
  return p;
}

vg_path_t vg_path_make_round_rect(int x, int y, int w, int h, int r,
                                  int seg_per_corner) {
  if (seg_per_corner < 2)
    seg_per_corner = 8;
  int cx0 = x + r, cy0 = y + r;         // top-left arc center
  int cx1 = x + w - r, cy1 = y + r;     // top-right
  int cx2 = x + w - r, cy2 = y + h - r; // bottom-right
  int cx3 = x + r, cy3 = y + h - r;     // bottom-left
  vg_path_t p = vg_path_init((size_t)(seg_per_corner * 4 + 1));
  // Start at top-left corner (moving right)
  // Top-left corner arc from 180->270 deg
  for (int i = 0; i <= seg_per_corner; ++i) {
    float t = (float)i / (float)seg_per_corner;
    float ang = (float)M_PI + t * (float)M_PI_2; // 180 to 270
    int px = cx0 + (int)lroundf(r * cosf(ang));
    int py = cy0 + (int)lroundf(r * sinf(ang));
    vg_path_append(&p, pack_i16(px, py));
  }
  // Top edge to top-right arc start is implicit by arc end point
  // Top-right arc 270->360
  for (int i = 1; i <= seg_per_corner; ++i) {
    float t = (float)i / (float)seg_per_corner;
    float ang = (float)M_PI_2 * 3.0f + t * (float)M_PI_2; // 270 to 360
    int px = cx1 + (int)lroundf(r * cosf(ang));
    int py = cy1 + (int)lroundf(r * sinf(ang));
    vg_path_append(&p, pack_i16(px, py));
  }
  // Bottom-right arc 0->90
  for (int i = 1; i <= seg_per_corner; ++i) {
    float t = (float)i / (float)seg_per_corner;
    float ang = 0.0f + t * (float)M_PI_2; // 0 to 90
    int px = cx2 + (int)lroundf(r * cosf(ang));
    int py = cy2 + (int)lroundf(r * sinf(ang));
    vg_path_append(&p, pack_i16(px, py));
  }
  // Bottom-left arc 90->180
  for (int i = 1; i <= seg_per_corner; ++i) {
    float t = (float)i / (float)seg_per_corner;
    float ang = (float)M_PI_2 + t * (float)M_PI_2; // 90 to 180
    int px = cx3 + (int)lroundf(r * cosf(ang));
    int py = cy3 + (int)lroundf(r * sinf(ang));
    vg_path_append(&p, pack_i16(px, py));
  }
  // Close path back to start
  // Ensure last point equals first to close polyline visually
  vg_point_t first = p.points[0];
  vg_path_append(&p, first);
  return p;
}

static vg_path_t vg_path_make_triangle_points(int x0, int y0, int x1, int y1,
                                              int x2, int y2) {
  vg_path_t p = vg_path_init(4);
  vg_path_append(&p, pack_i16(x0, y0));
  vg_path_append(&p, pack_i16(x1, y1));
  vg_path_append(&p, pack_i16(x2, y2));
  vg_path_append(&p, pack_i16(x0, y0));
  return p;
}

bool vg_shape_init_triangle(vg_shape_t *s, int x0, int y0, int x1, int y1,
                            int x2, int y2) {
  if (!s)
    return false;
  s->path = vg_path_make_triangle_points(x0, y0, x1, y1, x2, y2);
  shape_defaults(s);
  return true;
}

bool vg_shape_init_rect(vg_shape_t *s, int x, int y, int w, int h) {
  if (!s)
    return false;
  s->path = vg_path_make_rect(x, y, w, h);
  shape_defaults(s);
  return true;
}

bool vg_shape_init_circle(vg_shape_t *s, int cx, int cy, int r, int segments) {
  if (!s)
    return false;
  s->path = vg_path_make_circle(cx, cy, r, segments);
  shape_defaults(s);
  return true;
}

bool vg_shape_init_ellipse(vg_shape_t *s, int cx, int cy, int rx, int ry,
                           int segments) {
  if (!s)
    return false;
  s->path = vg_path_make_ellipse(cx, cy, rx, ry, segments);
  shape_defaults(s);
  return true;
}

bool vg_shape_init_round_rect(vg_shape_t *s, int x, int y, int w, int h, int r,
                              int seg_per_corner) {
  if (!s)
    return false;
  if (r < 0)
    r = 0;
  if (r * 2 > w)
    r = w / 2;
  if (r * 2 > h)
    r = h / 2;
  s->path = vg_path_make_round_rect(x, y, w, h, r, seg_per_corner);
  shape_defaults(s);
  return true;
}
