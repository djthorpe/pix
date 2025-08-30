#pragma once
#include "path.h"
#include "shape.h"

/**
 * @file vg/primitives.h
 * @brief Convenience geometry builders for common path outlines.
 *
 * These helpers construct or replace only the path geometry of an existing
 * shape; styling (fill / stroke / transform / fill rule) is left untouched.
 * This separation keeps creation predictable and avoids implicit side effects.
 *
 * Usage pattern:
 * @code
 *   vg_shape_t *s = vg_canvas_append(&canvas); // default style applied
 *   vg_shape_init_rect(s, origin, size);       // define geometry
 *   vg_shape_set_fill_color(s, 0xFF3366FF);    // customize styling
 * @endcode
 *
 * Re-invoking a vg_shape_init_* on the same shape discards its previous path
 * (memory is freed) and installs the new one, preserving style fields.
 *
 * Coordinate constraints: positions use 16-bit signed; sizes and radii use
 * 16-bit unsigned storage. Larger inputs are truncated. Passing a NULL shape
 * returns false without side effects.
 */

/** @ingroup vg
 * Initialize an axis-aligned rectangle at origin with extent size. */
bool vg_shape_init_rect(vg_shape_t *s, pix_point_t origin, pix_size_t size);

/** @ingroup vg
 * Initialize a regular polygonal approximation of a circle.
 * @param center Center point.
 * @param radius Circle radius (pixels).
 * @param segments Requested segment count (clamped to minimum 8).
 */
bool vg_shape_init_circle(vg_shape_t *s, pix_point_t center,
                          pix_scalar_t radius, int segments);

/** @ingroup vg
 * Initialize a polygonal approximation of an ellipse.
 * @param center Center point.
 * @param rx Horizontal radius.
 * @param ry Vertical radius.
 * @param segments Requested segment count (clamped to minimum 8).
 */
bool vg_shape_init_ellipse(vg_shape_t *s, pix_point_t center, pix_scalar_t rx,
                           pix_scalar_t ry, int segments);

/** @ingroup vg
 * Initialize a rounded rectangle.
 * Radius is clamped to at most half width/height. seg_per_corner < 2 => 8.
 */
bool vg_shape_init_round_rect(vg_shape_t *s, pix_point_t origin,
                              pix_size_t size, pix_scalar_t radius,
                              int seg_per_corner);

/** @ingroup vg
 * Initialize a triangle with vertices a,b,c (automatically closed). */
bool vg_shape_init_triangle(vg_shape_t *s, pix_point_t a, pix_point_t b,
                            pix_point_t c);
