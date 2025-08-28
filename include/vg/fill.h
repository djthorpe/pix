#pragma once
#include "path.h"
#include "transform.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { VG_FILL_EVEN_ODD = 0, VG_FILL_NON_ZERO = 1 } vg_fill_rule_t;

struct vg_path_t;
struct vg_shape_t;
struct vg_canvas_t;
struct pix_frame_t;

// Fill a path onto a frame using scanline rasterization.
//
// The clipped variant allows restricting rasterization to an inclusive
// rectangular region [clip_x0..clip_x1], [clip_y0..clip_y1]. Pass the full
// frame bounds to render normally.
void vg_fill_path(const struct vg_path_t *path,
                  const struct vg_transform_t *xform, struct pix_frame_t *frame,
                  uint32_t color, vg_fill_rule_t rule);

void vg_fill_path_clipped(const struct vg_path_t *path,
                          const struct vg_transform_t *xform,
                          struct pix_frame_t *frame, uint32_t color,
                          vg_fill_rule_t rule, int clip_x0, int clip_y0,
                          int clip_x1, int clip_y1);
void vg_fill_shape(const struct vg_shape_t *shape, struct pix_frame_t *frame,
                   uint32_t color, vg_fill_rule_t rule);
void vg_canvas_render_fill(const struct vg_canvas_t *canvas,
                           struct pix_frame_t *frame, uint32_t color,
                           vg_fill_rule_t rule);
