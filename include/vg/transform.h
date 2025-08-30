/**
 * @file transform.h
 * @brief 2D affine transform utilities (row-major 3x3 matrix, affine subset).
 *
 * Storage is a full 3x3 float matrix in row-major order. Only affine forms
 * are generated (final row fixed at [0 0 1]). Helper setters overwrite the
 * destination; multiply composes a*b (apply b then a). All functions accept
 * aliasing of output with inputs unless stated otherwise.
 */
#pragma once

typedef struct vg_transform_t {
  float m[3][3]; /**< Row-major 3x3 matrix (affine: m[2] = {0,0,1}). */
} vg_transform_t;

/** @ingroup vg
 * Set matrix to identity. */
void vg_transform_identity(vg_transform_t *t);

/** @ingroup vg
 * Set to pure translation (tx, ty). */
void vg_transform_translate(vg_transform_t *t, float tx, float ty);

/** @ingroup vg
 * Set to non-uniform scale (sx, sy). */
void vg_transform_scale(vg_transform_t *t, float sx, float sy);

/** @ingroup vg
 * Set to rotation about origin by angle in radians (CCW). */
void vg_transform_rotate(vg_transform_t *t, float radians);

/** @ingroup vg
 * out = a * b (apply b then a to a column vector). out may alias a or b. */
void vg_transform_multiply(vg_transform_t *out, const vg_transform_t *a,
                           const vg_transform_t *b);

/** @ingroup vg
 * Transform (x,y) producing (out_x,out_y). */
void vg_transform_point(const vg_transform_t *t, float x, float y, float *out_x,
                        float *out_y);
