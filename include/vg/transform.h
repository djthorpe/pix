/**
 * @file transform.h
 * @brief 2D affine transform utilities (column-major 3x3 matrix).
 *
 * The matrix representation is a full 3x3 float array, but only affine
 * transforms are produced by the provided helpers (last row = [0 0 1]).
 * Functions operate in-place unless an explicit output parameter is supplied.
 */
#pragma once

typedef struct vg_transform_t {
  float m[3][3]; /**< Row-major 3x3 matrix (affine: m[2] = {0,0,1}). */
} vg_transform_t;

/** Set matrix to identity. */
void vg_transform_identity(vg_transform_t *t);

/** Set to pure translation (tx, ty). */
void vg_transform_translate(vg_transform_t *t, float tx, float ty);

/** Set to non-uniform scale (sx, sy). */
void vg_transform_scale(vg_transform_t *t, float sx, float sy);

/** Set to rotation about origin by angle in radians (CCW). */
void vg_transform_rotate(vg_transform_t *t, float radians);

/** out = a * b (apply b then a to a column vector). out may alias a or b. */
void vg_transform_multiply(vg_transform_t *out, const vg_transform_t *a,
                           const vg_transform_t *b);

/** Transform (x,y) producing (out_x,out_y). */
void vg_transform_point(const vg_transform_t *t, float x, float y, float *out_x,
                        float *out_y);
