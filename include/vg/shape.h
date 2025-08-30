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
 * PIX_COLOR_NONE (0). No separate enable flags exist; this keeps state minimal.
 */
#pragma once

#include "path.h"
#include "transform.h"
#include <pix/pix.h>

/**
 * @enum vg_cap_t
 * @ingroup vg
 * @brief Stroke line cap style applied at the start and end of open subpaths.
 */
typedef enum {
  VG_CAP_BUTT = 0,   /**< Flat cap exactly at the end points. */
  VG_CAP_SQUARE = 1, /**< Square cap extends half the stroke width. */
  VG_CAP_ROUND = 2   /**< Semicircular cap with radius = half width. */
} vg_cap_t;

/**
 * @enum vg_join_t
 * @ingroup vg
 * @brief Stroke join style where two path segments meet.
 */
typedef enum {
  VG_JOIN_BEVEL = 0, /**< Beveled (clipped) corner. */
  VG_JOIN_ROUND = 1, /**< Circular arc join. */
  VG_JOIN_MITER = 2  /**< Mitered (sharp) join, limited by miter_limit. */
} vg_join_t;

/**
 * @enum vg_fill_rule_t
 * @ingroup vg
 * @brief Path fill rule determining inside/outside classification.
 */
typedef enum vg_fill_rule_t {
  VG_FILL_EVEN_ODD = 0,     /**< Even-odd (parity) rule with gap bridging. */
  VG_FILL_EVEN_ODD_RAW = 1, /**< Even-odd without gap bridging (exact runs). */
} vg_fill_rule_t;

/**
 * Opaque shape handle. Append a new shape to a canvas with vg_canvas_append
 */
typedef struct vg_shape_t vg_shape_t;

/**
 * @name Path Access
 * Direct access to the internal path for building / mutation. The pointer
 * remains valid until vg_canvas_clear or destroy. Mutating the returned
 * vg_path_t directly is permitted (e.g. vg_path_append). A separate const
 * accessor was removed to keep the API minimal; callers needing read-only
 * access should still use vg_shape_path and avoid mutation.
 * @{ */
/** @ingroup vg */
vg_path_t *vg_shape_path(vg_shape_t *shape);

/** Reset (clear) the shape's path, freeing all segments, then reserve
 *  at least 'reserve' points in the first segment (clamped to minimum 4).
 *  Returns false on allocation failure (shape left with empty path). */
/** @ingroup vg */
bool vg_shape_path_clear(vg_shape_t *shape, size_t reserve);

/** @} */

/**
 * @name Transform
 * A non-owned transform pointer that is applied at render time (may be NULL).
 * Caller manages lifetime; no copy is made.
 * @{ */
/** @ingroup vg */
void vg_shape_set_transform(vg_shape_t *shape, const vg_transform_t *xf);
/** @ingroup vg */
const vg_transform_t *vg_shape_get_transform(const vg_shape_t *shape);
/** @} */

/**
 * @name Colors
 * 0xAARRGGBB packed; PIX_COLOR_NONE disables that paint.
 * @{ */
/** @ingroup vg */
void vg_shape_set_fill_color(vg_shape_t *shape, pix_color_t c);
/** @ingroup vg */
void vg_shape_set_stroke_color(vg_shape_t *shape, pix_color_t c);
/** @ingroup vg */
pix_color_t vg_shape_get_fill_color(const vg_shape_t *shape);
/** @ingroup vg */
pix_color_t vg_shape_get_stroke_color(const vg_shape_t *shape);
/** @} */

/**
 * @name Stroke Parameters
 * @{ */
/** @ingroup vg */
void vg_shape_set_stroke_width(vg_shape_t *shape, float w);
/** @ingroup vg */
float vg_shape_get_stroke_width(const vg_shape_t *shape);
/** @ingroup vg */
void vg_shape_set_stroke_cap(vg_shape_t *shape, vg_cap_t cap);
/** @ingroup vg */
vg_cap_t vg_shape_get_stroke_cap(const vg_shape_t *shape);
/** @ingroup vg */
void vg_shape_set_stroke_join(vg_shape_t *shape, vg_join_t join);
/** @ingroup vg */
vg_join_t vg_shape_get_stroke_join(const vg_shape_t *shape);
/** @ingroup vg */
void vg_shape_set_miter_limit(vg_shape_t *shape, float limit);
/** @ingroup vg */
float vg_shape_get_miter_limit(const vg_shape_t *shape);
/** @} */

/**
 * @name Fill Rule
 * @{ */
/** @ingroup vg */
void vg_shape_set_fill_rule(vg_shape_t *shape, vg_fill_rule_t rule);
/** @ingroup vg */
vg_fill_rule_t vg_shape_get_fill_rule(const vg_shape_t *shape);
/** @} */

/**
 * @name Image Shape Helpers
 * Convert a regular shape into an image blit definition referencing an
 * external source frame. The shape's path is ignored during rendering and
 * the specified source rectangle is copied (optionally blended) into the
 * destination at @p dst_origin. (Phase 1: axis-aligned copy only.)
 * @{ */
/**
 * @brief Configure a shape to render an image region.
 *
 * The shape becomes an image shape referencing @p frame. The provided
 * source rectangle (origin + size) is clipped internally to the frame
 * bounds. A size of {0,0} implies "use full frame". Flags map to
 * pix_blit_flags_t (e.g. PIX_BLIT_ALPHA) and are passed through to the
 * underlying frame copy function.
 *
 * @param shape Target shape.
 * @param frame Source frame (must stay alive while the shape is rendered).
 * @param src_origin Source top-left within frame.
 * @param src_size Source size (0 => full size in that dimension).
 * @param dst_origin Destination top-left in the target frame at render time.
 * @param flags Copy/blit flags (see pix_blit_flags_t).
 */
/** @ingroup vg */
void vg_shape_set_image(vg_shape_t *shape, const struct pix_frame_t *frame,
                        pix_point_t src_origin, pix_size_t src_size,
                        pix_point_t dst_origin, unsigned flags);
/** @} */

/**
 * @name Geometry Utilities
 * @{ */
/**
 * @brief Compute axis-aligned bounds (untransformed) of a single shape.
 *
 * Path shapes: iterates all path segments / points ignoring stroke width
 * expansion and any transform pointer set on the shape.
 *
 * Image shapes: returns the destination rectangle (dst_origin + src size
 * or full frame size if src_size.{w,h} are zero).
 *
 * Empty / NULL / unsupported kinds yield origin (0,0) and size {0,0}.
 *
 * @param shape Shape to inspect (may be NULL).
 * @param origin Output min point (may be NULL).
 * @param size Output size (may be NULL).
 */
/** @ingroup vg */
void vg_shape_bbox(const vg_shape_t *shape, pix_point_t *origin,
                   pix_size_t *size);
/** @} */
