// Tiger example canvas/shape construction & transform API
#ifndef TIGER_CANVAS_H
#define TIGER_CANVAS_H

#include <stddef.h>
#include <vg/vg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Global tiger canvas (list of shapes) and shared transform
extern vg_canvas_t g_canvas;
extern vg_transform_t g_tiger_xform;

// Interactive view state (scale relative to fit, and pan in device px)
extern float g_user_scale;
extern float g_user_pan_x;
extern float g_user_pan_y;
// Interactive rotation in radians (CCW, applied about artwork center)
extern float g_user_rotate;

// Build (idempotent) tiger canvas. Optionally returns shape array & count.
// Returns the canvas by value (small struct). shapes_out/count_out may be NULL.
vg_canvas_t tiger_build_canvas(vg_shape_t ***shapes_out, size_t *count_out);

// Update shared transform for current window size + interactive view state.
void update_transform(int w, int h);

// Free all tiger shapes & canvas storage.
void free_tiger_shapes(void);

#ifdef __cplusplus
}
#endif

#endif // TIGER_CANVAS_H
