#include <math.h>
#include <vg/vg.h>

/* Geometry builders only: styling defaults are applied at shape creation. */

bool vg_shape_init_triangle(vg_shape_t *s, pix_point_t a, pix_point_t b,
                            pix_point_t c) {
  if (!s)
    return false;
  vg_shape_path_clear(s, 4);
  vg_path_t *p = vg_shape_path(s);
  vg_path_append(p, &(pix_point_t){a.x, a.y}, &(pix_point_t){b.x, b.y},
                 &(pix_point_t){c.x, c.y}, &(pix_point_t){a.x, a.y}, NULL);
  return true;
}

bool vg_shape_init_rect(vg_shape_t *s, pix_point_t origin, pix_size_t size) {
  if (!s)
    return false;
  vg_shape_path_clear(s, 5);
  vg_path_t *p = vg_shape_path(s);
  int x = origin.x, y = origin.y, w = size.w, h = size.h;
  vg_path_append(p, &(pix_point_t){x, y}, &(pix_point_t){(int16_t)(x + w), y},
                 &(pix_point_t){(int16_t)(x + w), (int16_t)(y + h)},
                 &(pix_point_t){x, (int16_t)(y + h)}, &(pix_point_t){x, y},
                 NULL);
  return true;
}

bool vg_shape_init_circle(vg_shape_t *s, pix_point_t center, pix_scalar_t r,
                          int segments) {
  if (!s)
    return false;
  if (segments < 8)
    segments = 32;
  vg_shape_path_clear(s, (size_t)segments + 1);
  vg_path_t *p = vg_shape_path(s);
  for (int i = 0; i <= segments; ++i) {
    float t = (float)i / (float)segments * 2.0f * (float)M_PI;
    int x = center.x + (int)lroundf(r * cosf(t));
    int y = center.y + (int)lroundf(r * sinf(t));
    vg_path_append(p, &(pix_point_t){x, y}, NULL);
  }
  return true;
}

bool vg_shape_init_ellipse(vg_shape_t *s, pix_point_t center, pix_scalar_t rx,
                           pix_scalar_t ry, int segments) {
  if (!s)
    return false;
  if (segments < 8)
    segments = 32;
  vg_shape_path_clear(s, (size_t)segments + 1);
  vg_path_t *p = vg_shape_path(s);
  for (int i = 0; i <= segments; ++i) {
    float t = (float)i / (float)segments * 2.0f * (float)M_PI;
    int x = center.x + (int)lroundf(rx * cosf(t));
    int y = center.y + (int)lroundf(ry * sinf(t));
    vg_path_append(p, &(pix_point_t){x, y}, NULL);
  }
  return true;
}

bool vg_shape_init_round_rect(vg_shape_t *s, pix_point_t origin,
                              pix_size_t size, pix_scalar_t r,
                              int seg_per_corner) {
  if (!s)
    return false;
  if (seg_per_corner < 2)
    seg_per_corner = 8;
  if (r * 2 > size.w)
    r = size.w / 2;
  if (r * 2 > size.h)
    r = size.h / 2;
  int x = origin.x, y = origin.y, w = size.w, h = size.h;
  int cx0 = x + r, cy0 = y + r;
  int cx1 = x + w - r, cy1 = y + r;
  int cx2 = x + w - r, cy2 = y + h - r;
  int cx3 = x + r, cy3 = y + h - r;
  vg_shape_path_clear(s, (size_t)(seg_per_corner * 4 + 1));
  vg_path_t *p = vg_shape_path(s);
  for (int i = 0; i <= seg_per_corner; ++i) {
    float t = (float)i / (float)seg_per_corner;
    float ang = (float)M_PI + t * (float)M_PI_2;
    int px = cx0 + (int)lroundf(r * cosf(ang));
    int py = cy0 + (int)lroundf(r * sinf(ang));
    vg_path_append(p, &(pix_point_t){px, py}, NULL);
  }
  for (int i = 1; i <= seg_per_corner; ++i) {
    float t = (float)i / (float)seg_per_corner;
    float ang = (float)M_PI_2 * 3.0f + t * (float)M_PI_2;
    int px = cx1 + (int)lroundf(r * cosf(ang));
    int py = cy1 + (int)lroundf(r * sinf(ang));
    vg_path_append(p, &(pix_point_t){px, py}, NULL);
  }
  for (int i = 1; i <= seg_per_corner; ++i) {
    float t = (float)i / (float)seg_per_corner;
    float ang = 0.0f + t * (float)M_PI_2;
    int px = cx2 + (int)lroundf(r * cosf(ang));
    int py = cy2 + (int)lroundf(r * sinf(ang));
    vg_path_append(p, &(pix_point_t){px, py}, NULL);
  }
  for (int i = 1; i <= seg_per_corner; ++i) {
    float t = (float)i / (float)seg_per_corner;
    float ang = (float)M_PI_2 + t * (float)M_PI_2;
    int px = cx3 + (int)lroundf(r * cosf(ang));
    int py = cy3 + (int)lroundf(r * sinf(ang));
    vg_path_append(p, &(pix_point_t){px, py}, NULL);
  }
  pix_point_t first = p->points[0];
  vg_path_append(p, &first, NULL);
  return true;
}
