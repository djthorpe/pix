#pragma once
#include <stdbool.h>
#include <vg/path.h>
#include <vg/shape.h>

vg_path_t vg_path_make_rect(int x, int y, int w, int h);
vg_path_t vg_path_make_circle(int cx, int cy, int r, int segments);
vg_path_t vg_path_make_ellipse(int cx, int cy, int rx, int ry, int segments);
vg_path_t vg_path_make_round_rect(int x, int y, int w, int h, int r,
                                  int seg_per_corner);

bool vg_shape_init_rect(vg_shape_t *s, int x, int y, int w, int h);
bool vg_shape_init_circle(vg_shape_t *s, int cx, int cy, int r, int segments);
bool vg_shape_init_ellipse(vg_shape_t *s, int cx, int cy, int rx, int ry,
                           int segments);
bool vg_shape_init_round_rect(vg_shape_t *s, int x, int y, int w, int h, int r,
                              int seg_per_corner);
bool vg_shape_init_triangle(vg_shape_t *s, int x0, int y0, int x1, int y1,
                            int x2, int y2);
