#pragma once
#include "path.h"
#include "shape.h"

/* Primitive shape initializers.
 *
 * These helpers build common geometric paths directly into an existing
 * vg_shape_t. They REPLACE the shape's current path (freeing its previous
 * storage) but DO NOT modify any other shape properties (fill/stroke colors,
 * stroke params, transform, fill rule). This keeps responsibilities separated:
 *  - vg_canvas_append / vg_shape_create : allocate + apply defaults
 *  - vg_shape_init_*                    : set geometry only
 *
 * Typical usage:
 *   vg_shape_t *s = vg_canvas_append(&canvas); // has default colors/etc
 *   vg_shape_init_rect(s, origin, size);       // geometry only
 *   vg_shape_set_fill_color(s, 0xFF....);      // customize style
 *
 * Reusing a shape: calling another vg_shape_init_* will discard the old path
 * (cleanly) and build the new one while preserving styling state.
 *
 * All coordinates use 16‑bit signed (pix_point_t) and sizes / radii use
 * 16‑bit unsigned (pix_size_t / pix_scalar_t). Supplying larger values will
 * truncate. Passing NULL shape returns false and leaves state unchanged.
 */

/** Initialize an axis-aligned rectangle at origin with extent size. */
bool vg_shape_init_rect(vg_shape_t *s, pix_point_t origin, pix_size_t size);

/** Initialize a regular polygonal approximation of a circle.
 * @param center Center point.
 * @param radius Circle radius (pixels).
 * @param segments Number of line segments (minimum enforced internally: 8).
 */
bool vg_shape_init_circle(vg_shape_t *s, pix_point_t center,
                          pix_scalar_t radius, int segments);

/** Initialize a polygonal approximation of an ellipse.
 * @param center Center point.
 * @param rx Horizontal radius.
 * @param ry Vertical radius.
 * @param segments Number of line segments (minimum enforced internally: 8).
 */
bool vg_shape_init_ellipse(vg_shape_t *s, pix_point_t center, pix_scalar_t rx,
                           pix_scalar_t ry, int segments);

/** Initialize a rounded rectangle.
 * Radius is clamped so it never exceeds half the width/height. If
 * seg_per_corner < 2 it is promoted to 8 for smoother corners.
 */
bool vg_shape_init_round_rect(vg_shape_t *s, pix_point_t origin,
                              pix_size_t size, pix_scalar_t radius,
                              int seg_per_corner);

/** Initialize a triangle with vertices a,b,c (automatically closed). */
bool vg_shape_init_triangle(vg_shape_t *s, pix_point_t a, pix_point_t b,
                            pix_point_t c);
