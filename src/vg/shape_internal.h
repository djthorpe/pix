#pragma once
#include <vg/vg.h>

typedef enum vg_shape_kind_t {
  VG_SHAPE_PATH = 0,
  VG_SHAPE_IMAGE = 1
} vg_shape_kind_t;

typedef struct vg_image_ref_t {
  const pix_frame_t *frame; /* non-owning */
  pix_point_t src_origin;
  pix_size_t src_size;    /* if {0,0} treat as full frame */
  pix_point_t dst_origin; /* only for axis-aligned copy now */
  unsigned flags;         /* pix_blit_flags_t */
} vg_image_ref_t;

struct vg_shape_t {
  vg_shape_kind_t kind;
  const vg_transform_t *transform; /* not owned */
  union {
    struct {
      vg_path_t path;
      pix_color_t fill_color;
      pix_color_t stroke_color;
      float stroke_width;
      vg_cap_t stroke_cap;
      vg_join_t stroke_join;
      float miter_limit;
      vg_fill_rule_t fill_rule;
    } v;                /* vector */
    vg_image_ref_t img; /* image */
  } data;
};

/* Internal helper used by the canvas pool to lazily initialize defaults. */
void vg__shape_internal_defaults(vg_shape_t *s);
