#pragma once
#include "fill.h"
#include "shape.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct vg_canvas_t {
  struct vg_shape_t **shapes;
  size_t size;
  size_t capacity;
} vg_canvas_t;

vg_canvas_t vg_canvas_init(size_t capacity);
bool vg_canvas_append(vg_canvas_t *canvas, struct vg_shape_t *shape);
void vg_canvas_clear(vg_canvas_t *canvas);
void vg_canvas_destroy(vg_canvas_t *canvas);
void vg_canvas_render(const vg_canvas_t *canvas, struct pix_frame_t *frame,
                      uint32_t fallback_stroke_color);
void vg_canvas_render_all(const vg_canvas_t *canvas, struct pix_frame_t *frame,
                          uint32_t fallback_fill_color,
                          uint32_t fallback_stroke_color, vg_fill_rule_t rule);
