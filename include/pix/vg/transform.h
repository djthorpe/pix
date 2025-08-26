#pragma once
#include <stdbool.h>

typedef struct vg_transform_t {
  float m[3][3];
} vg_transform_t;

void vg_transform_identity(vg_transform_t *t);
void vg_transform_translate(vg_transform_t *t, float tx, float ty);
void vg_transform_scale(vg_transform_t *t, float sx, float sy);
void vg_transform_rotate(vg_transform_t *t, float radians);
void vg_transform_multiply(vg_transform_t *out, const vg_transform_t *a,
                           const vg_transform_t *b);
void vg_transform_point(const vg_transform_t *t, float x, float y, float *out_x,
                        float *out_y);

// Apply a 2D affine transform to point (x,y) -> (out_x,out_y)
void vg_transform_point(const vg_transform_t *t, float x, float y, float *out_x,
                        float *out_y);
