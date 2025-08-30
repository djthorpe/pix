/**
 * @file vg/fill.h
 * @brief Polygon fill interface (software raster, integer pixel grid).
 *
 * The fill routines rasterize one or more linked path segments into a
 * destination frame using a scanline algorithm. Coordinates are taken from
 * packed 16-bit integer point records inside vg_path_t. An optional transform
 * may be supplied to map path coordinates to device space (applied in float).
 *
 * Supported fill rules:
 *  - VG_FILL_EVEN_ODD: parity rule with short gap bridging (for smoother fills)
 *  - VG_FILL_EVEN_ODD_RAW: parity rule without bridging (crisp bitmap text)
 */
#pragma once
#include "path.h"
#include "shape.h"
#include "transform.h"
#include <pix/pix.h>

/**
 * Fill a path (optionally transformed) into the full target frame.
 */
void vg_fill_path(const struct vg_path_t *path,
                  const struct vg_transform_t *xform, struct pix_frame_t *frame,
                  pix_color_t color, vg_fill_rule_t rule);

/**
 * Fill with an explicit inclusive clip rectangle [clip_x0,clip_x1] ×
 * [clip_y0,clip_y1] in device pixels. Out-of-range clip silently clamps.
 */
void vg_fill_path_clipped(const struct vg_path_t *path,
                          const struct vg_transform_t *xform,
                          struct pix_frame_t *frame, pix_color_t color,
                          vg_fill_rule_t rule, pix_point_t clip_min,
                          pix_point_t clip_max);
