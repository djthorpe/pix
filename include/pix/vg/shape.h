#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <vg/path.h>
#include <vg/transform.h>

typedef enum { VG_CAP_BUTT = 0, VG_CAP_SQUARE = 1, VG_CAP_ROUND = 2 } vg_cap_t;
typedef enum {
  VG_JOIN_BEVEL = 0,
  VG_JOIN_ROUND = 1,
  VG_JOIN_MITER = 2
} vg_join_t;

typedef struct vg_shape_t {
  vg_path_t path;
  const struct vg_transform_t *transform; // optional
  uint32_t fill_color;                    // 0xAARRGGBB (AA RR GG BB)
  uint32_t stroke_color;                  // 0xAARRGGBB
  float stroke_width;
  vg_cap_t stroke_cap;
  vg_join_t stroke_join;
  float miter_limit; // reserved for miter joins
  bool has_fill;
  bool has_stroke;
} vg_shape_t;
