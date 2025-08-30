#pragma once
#include <pix/pix.h>
#include <vg/vg.h>
/* Internal fill API (formerly public). */
void vg_fill_path(const struct vg_path_t *path,
                  const struct vg_transform_t *xform, struct pix_frame_t *frame,
                  pix_color_t color, vg_fill_rule_t rule);
void vg_fill_path_clipped(const struct vg_path_t *path,
                          const struct vg_transform_t *xform,
                          struct pix_frame_t *frame, pix_color_t color,
                          vg_fill_rule_t rule, pix_point_t clip_min,
                          pix_point_t clip_max);
