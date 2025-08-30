/**
 * @file shape.h
 * @brief Opaque vector shape object and styling API.
 *
 * A vg_shape_t bundles a path (one or more linked path segments) together
 * with styling information (fill / stroke colors, stroke parameters, fill
 * rule, optional transform). The renderer iterates shapes on a canvas and
 * uses only public accessors; the concrete struct layout is private.
 *
 * Fill / stroke presence: a paint is considered disabled when its color is
 * VG_COLOR_NONE (0). No separate enable flags exist; this keeps state minimal.
 */
#pragma once

#include "path.h"
#include "transform.h"
#include <pix/pix.h>

/** Stroke line cap style. */
typedef enum {
  VG_CAP_BUTT = 0,   /**< Flat cap exactly at the end points. */
  VG_CAP_SQUARE = 1, /**< Square cap extends half the stroke width. */
  VG_CAP_ROUND = 2   /**< Semicircular cap with radius = half width. */
} vg_cap_t;

/** Stroke join style between consecutive segments. */
typedef enum {
  VG_JOIN_BEVEL = 0, /**< Beveled (clipped) corner. */
  VG_JOIN_ROUND = 1, /**< Circular arc join. */
  VG_JOIN_MITER = 2  /**< Mitered (sharp) join, limited by miter_limit. */
} vg_join_t;

/** Path fill rule determining inside/outside classification & post-pass. */
typedef enum vg_fill_rule_t {
  VG_FILL_EVEN_ODD = 0,     /**< Even-odd (parity) rule with gap bridging. */
  VG_FILL_EVEN_ODD_RAW = 1, /**< Even-odd without gap bridging (exact runs). */
} vg_fill_rule_t;

/**
 * Opaque shape handle. Allocate with vg_shape_create (heap) or via
 * vg_canvas_append (preferred pooled allocation). Do NOT call
 * vg_shape_destroy on shapes obtained from a canvas pool.
 */
typedef struct vg_shape_t vg_shape_t;

/**
 * @name Lifecycle
 * @{ */
/** Allocate a new shape with default path (empty) and style defaults. */
vg_shape_t *vg_shape_create(void);
/** Destroy a shape you explicitly created (frees internal path storage). */
void vg_shape_destroy(vg_shape_t *shape);
/** @} */

/**
 * @name Path Access
 * Direct access to the internal path for building / mutation. The pointer
 * remains valid until vg_shape_destroy. Mutating the returned vg_path_t
 * directly is permitted (e.g. vg_path_append). For read-only usage prefer
 * vg_shape_const_path.
 * @{ */
vg_path_t *vg_shape_path(vg_shape_t *shape);
const vg_path_t *vg_shape_const_path(const vg_shape_t *shape);
/** @} */

/**
 * @name Transform
 * A non-owned transform pointer that is applied at render time (may be NULL).
 * Caller manages lifetime; no copy is made.
 * @{ */
void vg_shape_set_transform(vg_shape_t *shape, const vg_transform_t *xf);
const vg_transform_t *vg_shape_get_transform(const vg_shape_t *shape);
/** @} */

/**
 * @name Colors
 * 0xAARRGGBB packed; VG_COLOR_NONE disables that paint.
 * @{ */
void vg_shape_set_fill_color(vg_shape_t *shape, pix_color_t c);
void vg_shape_set_stroke_color(vg_shape_t *shape, pix_color_t c);
pix_color_t vg_shape_get_fill_color(const vg_shape_t *shape);
pix_color_t vg_shape_get_stroke_color(const vg_shape_t *shape);
/** @} */

/**
 * @name Stroke Parameters
 * @{ */
void vg_shape_set_stroke_width(vg_shape_t *shape, float w);
float vg_shape_get_stroke_width(const vg_shape_t *shape);
void vg_shape_set_stroke_cap(vg_shape_t *shape, vg_cap_t cap);
vg_cap_t vg_shape_get_stroke_cap(const vg_shape_t *shape);
void vg_shape_set_stroke_join(vg_shape_t *shape, vg_join_t join);
vg_join_t vg_shape_get_stroke_join(const vg_shape_t *shape);
void vg_shape_set_miter_limit(vg_shape_t *shape, float limit);
float vg_shape_get_miter_limit(const vg_shape_t *shape);
/** @} */

/**
 * @name Fill Rule
 * @{ */
void vg_shape_set_fill_rule(vg_shape_t *shape, vg_fill_rule_t rule);
vg_fill_rule_t vg_shape_get_fill_rule(const vg_shape_t *shape);
/** @} */

/** Image shape helpers (Phase 1: axis-aligned copy only) */
/* Turn an existing (default path) shape into an image shape referencing a
 * frame. */
void vg_shape_set_image(vg_shape_t *shape, const struct pix_frame_t *frame,
                        pix_point_t src_origin, pix_size_t src_size,
                        pix_point_t dst_origin, unsigned flags);
