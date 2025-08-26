#include <math.h>
#include <pix/frame.h>
#include <sdl/sdl_app.h>
#include <vg/canvas.h>
#include <vg/fill.h>
#include <vg/path.h>
#include <vg/primitives.h>
#include <vg/shape.h>
#include <vg/transform.h>
#include <vg/vg.h>

// no longer needed to pack points manually; we use primitives

int main(int argc, char *argv[]) {
  int width = 640, height = 480;
  // Use RGBA so per-shape alpha is honored by our software blending
  sdl_app_t *app = sdl_app_create(width, height, PIX_FMT_RGBA8888, "VG Demo");
  if (!app) {
    return 1;
  }

  // Build a square path centered on screen
  int cx = width / 2;
  int cy = height / 2;
  int side = (width < height ? width : height) / 3;
  int h = side / 2;
  int cr = (width < height ? width : height) / 6; // circle base radius

  vg_shape_t shape;
  vg_shape_init_rect(&shape, cx - h, cy - h, side, side);
  shape.transform = NULL;          // set below
  shape.fill_color = 0xFF0000FF;   // opaque blue fill (0xAARRGGBB)
  shape.stroke_color = 0xFFFFFFFF; // opaque white stroke
  shape.stroke_width = 10.0f;      // thicker stroke for visibility
  shape.stroke_cap = VG_CAP_ROUND;
  shape.stroke_join = VG_JOIN_ROUND;
  shape.has_fill = true;
  shape.has_stroke = true;

  // Circle that scales up/down at center
  vg_shape_t circle;
  vg_shape_init_circle(&circle, cx, cy, cr, 64);
  circle.transform = NULL;          // set below
  circle.fill_color = 0x80FF0000;   // 50% alpha red fill (0xAARRGGBB)
  circle.stroke_color = 0xA0FFFFFF; // ~63% alpha white stroke
  circle.stroke_width = 8.0f;
  circle.stroke_cap = VG_CAP_ROUND;
  circle.stroke_join = VG_JOIN_ROUND;
  circle.has_fill = true;
  circle.has_stroke = true;

  vg_canvas_t canvas = vg_canvas_init(8);
  vg_canvas_append(&canvas, &shape);
  vg_canvas_append(&canvas, &circle);
  // Green triangle rotating opposite direction
  vg_shape_t tri;
  int tx0 = cx, ty0 = cy - h;     // top
  int tx1 = cx - h, ty1 = cy + h; // bottom-left
  int tx2 = cx + h, ty2 = cy + h; // bottom-right
  vg_shape_init_triangle(&tri, tx0, ty0, tx1, ty1, tx2, ty2);
  tri.transform = NULL;          // set below
  tri.fill_color = 0x8000FF00;   // 50% alpha green
  tri.stroke_color = 0xFFFFFFFF; // white stroke
  tri.stroke_width = 6.0f;
  tri.stroke_cap = VG_CAP_ROUND;
  tri.stroke_join = VG_JOIN_ROUND;
  tri.has_fill = true;
  tri.has_stroke = true;
  vg_canvas_append(&canvas, &tri);

  // Showcase stroke joins: three thick 5-point stars (bevel, round, miter)
  int demo_w = (width < height ? width : height);
  int rx = (width - (demo_w)) / 2; // center group horizontally
  int margin_top = 40;
  int R = demo_w / 8;            // outer radius
  int r = R / 2;                 // inner radius
  int gap = R / 2;               // spacing between stars
  int c1x = rx + R;              // first center x
  int c2x = c1x + (2 * R + gap); // second center x
  int c3x = c2x + (2 * R + gap); // third center x
  int cy0 = margin_top + R;      // common center y
  vg_shape_t s_bevel, s_round, s_miter;
  // Build stars
  {
    // Bevel star
    s_bevel.path = vg_path_init(11);
    for (int i = 0; i < 10; ++i) {
      float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
      float rad = (i % 2 == 0) ? (float)R : (float)r;
      int px = c1x + (int)lroundf(rad * cosf(ang));
      int py = cy0 + (int)lroundf(rad * sinf(ang));
      vg_path_append(&s_bevel.path,
                     ((int32_t)(px & 0xFFFF) << 16) | (int32_t)(py & 0xFFFF));
    }
    vg_point_t first1 =
        ((int32_t)(((c1x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                   << 16) |
         (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) & 0xFFFF)));
    vg_path_append(&s_bevel.path, first1);

    // Round star
    s_round.path = vg_path_init(11);
    for (int i = 0; i < 10; ++i) {
      float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
      float rad = (i % 2 == 0) ? (float)R : (float)r;
      int px = c2x + (int)lroundf(rad * cosf(ang));
      int py = cy0 + (int)lroundf(rad * sinf(ang));
      vg_path_append(&s_round.path,
                     ((int32_t)(px & 0xFFFF) << 16) | (int32_t)(py & 0xFFFF));
    }
    vg_point_t first2 =
        ((int32_t)(((c2x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                   << 16) |
         (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) & 0xFFFF)));
    vg_path_append(&s_round.path, first2);

    // Miter star
    s_miter.path = vg_path_init(11);
    for (int i = 0; i < 10; ++i) {
      float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
      float rad = (i % 2 == 0) ? (float)R : (float)r;
      int px = c3x + (int)lroundf(rad * cosf(ang));
      int py = cy0 + (int)lroundf(rad * sinf(ang));
      vg_path_append(&s_miter.path,
                     ((int32_t)(px & 0xFFFF) << 16) | (int32_t)(py & 0xFFFF));
    }
    vg_point_t first3 =
        ((int32_t)(((c3x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                   << 16) |
         (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) & 0xFFFF)));
    vg_path_append(&s_miter.path, first3);
  }
  s_bevel.fill_color = VG_COLOR_NONE;
  s_bevel.stroke_color = 0xFF66FF66; // green-ish
  s_bevel.stroke_width = 30.0f;
  s_bevel.stroke_cap = VG_CAP_BUTT;
  s_bevel.stroke_join = VG_JOIN_BEVEL;
  s_bevel.miter_limit = 4.0f;
  s_bevel.transform = NULL;
  s_bevel.has_fill = false;
  s_bevel.has_stroke = true;
  s_round.fill_color = VG_COLOR_NONE;
  s_round.stroke_color = 0xFF66AAFF; // blue-ish
  s_round.stroke_width = 30.0f;
  s_round.stroke_cap = VG_CAP_BUTT;
  s_round.stroke_join = VG_JOIN_ROUND;
  s_round.miter_limit = 4.0f;
  s_round.transform = NULL;
  s_round.has_fill = false;
  s_round.has_stroke = true;
  // Miter star (with high limit)
  s_miter.fill_color = VG_COLOR_NONE;
  s_miter.stroke_color = 0xFFFF6666; // red-ish
  s_miter.stroke_width = 30.0f;
  s_miter.stroke_cap = VG_CAP_BUTT;
  s_miter.stroke_join = VG_JOIN_MITER;
  s_miter.miter_limit = 10.0f; // allow very pointy tips
  s_miter.transform = NULL;
  s_miter.has_fill = false;
  s_miter.has_stroke = true;
  vg_canvas_append(&canvas, &s_bevel);
  vg_canvas_append(&canvas, &s_round);
  vg_canvas_append(&canvas, &s_miter);

  pix_frame_t *frame = sdl_app_get_frame(app);

  // Animation state
  vg_transform_t transform, t_center, t_neg_center, rot, rot_neg, tmp;
  vg_transform_t circle_xform, tri_xform, scl; // reuse centers for both
  vg_transform_translate(&t_center, (float)cx, (float)cy);
  vg_transform_translate(&t_neg_center, (float)-cx, (float)-cy);
  shape.transform = &transform;
  circle.transform = &circle_xform;
  tri.transform = &tri_xform;

  uint32_t clear_color = 0x00000000; // transparent black background

  int running = 1;
  uint32_t start = sdl_app_ticks(app);
  while (running) {
    if (sdl_app_poll_should_close(app)) {
      running = 0;
    }

    int new_w, new_h;
    sdl_app_get_size(app, &new_w, &new_h);
    if (new_w != width || new_h != height) {
      width = new_w;
      height = new_h;
      // Recompute center-based transforms
      cx = width / 2;
      cy = height / 2;
      side = (width < height ? width : height) / 3;
      h = side / 2;
      cr = (width < height ? width : height) / 6;
      vg_transform_translate(&t_center, (float)cx, (float)cy);
      vg_transform_translate(&t_neg_center, (float)-cx, (float)-cy);
      // Rebuild square at new size
      vg_path_finish(&shape.path);
      vg_shape_init_rect(&shape, cx - h, cy - h, side, side);
      shape.fill_color = 0xFF0000FF;   // opaque blue fill
      shape.stroke_color = 0xFFFFFFFF; // opaque white stroke
      shape.stroke_width = 10.0f;      // thicker stroke for visibility
      shape.stroke_cap = VG_CAP_ROUND;
      shape.stroke_join = VG_JOIN_ROUND;
      shape.has_fill = true;
      shape.has_stroke = true;
      shape.transform = &transform; // rebind transform after re-init
      // Rebuild circle at new size
      vg_path_finish(&circle.path);
      vg_shape_init_circle(&circle, cx, cy, cr, 64);
      circle.fill_color = 0x80FF0000;
      circle.stroke_color = 0xA0FFFFFF;
      circle.stroke_width = 8.0f;
      circle.stroke_cap = VG_CAP_ROUND;
      circle.stroke_join = VG_JOIN_ROUND;
      circle.has_fill = true;
      circle.has_stroke = true;
      circle.transform = &circle_xform; // rebind transform after re-init
                                        // Rebuild triangle at new size
      vg_path_finish(&tri.path);
      tx0 = cx;
      ty0 = cy - h;
      tx1 = cx - h;
      ty1 = cy + h;
      tx2 = cx + h;
      ty2 = cy + h;
      vg_shape_init_triangle(&tri, tx0, ty0, tx1, ty1, tx2, ty2);
      tri.fill_color = 0x8000FF00;
      tri.stroke_color = 0xFFFFFFFF;
      tri.stroke_width = 6.0f;
      tri.stroke_cap = VG_CAP_ROUND;
      tri.stroke_join = VG_JOIN_ROUND;
      tri.has_fill = true;
      tri.has_stroke = true;
      tri.transform = &tri_xform; // rebind transform after re-init

      // Recompute join demo stars on resize
      demo_w = (width < height ? width : height);
      rx = (width - (demo_w)) / 2;
      margin_top = 40;
      R = demo_w / 8;
      r = R / 2;
      gap = R / 2;
      c1x = rx + R;
      c2x = c1x + (2 * R + gap);
      c3x = c2x + (2 * R + gap);
      cy0 = margin_top + R;
      // Rebuild each star
      vg_path_finish(&s_bevel.path);
      s_bevel.path = vg_path_init(11);
      for (int i = 0; i < 10; ++i) {
        float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
        float rad = (i % 2 == 0) ? (float)R : (float)r;
        int px = c1x + (int)lroundf(rad * cosf(ang));
        int py = cy0 + (int)lroundf(rad * sinf(ang));
        vg_path_append(&s_bevel.path,
                       ((int32_t)(px & 0xFFFF) << 16) | (int32_t)(py & 0xFFFF));
      }
      {
        vg_point_t first =
            ((int32_t)(((c1x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                       << 16) |
             (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) &
                        0xFFFF)));
        vg_path_append(&s_bevel.path, first);
      }
      s_bevel.fill_color = VG_COLOR_NONE;
      s_bevel.stroke_color = 0xFF66FF66;
      s_bevel.stroke_width = 30.0f;
      s_bevel.stroke_cap = VG_CAP_BUTT;
      s_bevel.stroke_join = VG_JOIN_BEVEL;
      s_bevel.miter_limit = 4.0f;
      s_bevel.transform = NULL;
      s_bevel.has_fill = false;
      s_bevel.has_stroke = true;
      vg_path_finish(&s_round.path);
      s_round.path = vg_path_init(11);
      for (int i = 0; i < 10; ++i) {
        float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
        float rad = (i % 2 == 0) ? (float)R : (float)r;
        int px = c2x + (int)lroundf(rad * cosf(ang));
        int py = cy0 + (int)lroundf(rad * sinf(ang));
        vg_path_append(&s_round.path,
                       ((int32_t)(px & 0xFFFF) << 16) | (int32_t)(py & 0xFFFF));
      }
      {
        vg_point_t first =
            ((int32_t)(((c2x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                       << 16) |
             (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) &
                        0xFFFF)));
        vg_path_append(&s_round.path, first);
      }
      s_round.fill_color = VG_COLOR_NONE;
      s_round.stroke_color = 0xFF66AAFF;
      s_round.stroke_width = 30.0f;
      s_round.stroke_cap = VG_CAP_BUTT;
      s_round.stroke_join = VG_JOIN_ROUND;
      s_round.miter_limit = 4.0f;
      s_round.transform = NULL;
      s_round.has_fill = false;
      s_round.has_stroke = true;
      vg_path_finish(&s_miter.path);
      s_miter.path = vg_path_init(11);
      for (int i = 0; i < 10; ++i) {
        float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
        float rad = (i % 2 == 0) ? (float)R : (float)r;
        int px = c3x + (int)lroundf(rad * cosf(ang));
        int py = cy0 + (int)lroundf(rad * sinf(ang));
        vg_path_append(&s_miter.path,
                       ((int32_t)(px & 0xFFFF) << 16) | (int32_t)(py & 0xFFFF));
      }
      {
        vg_point_t first =
            ((int32_t)(((c3x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                       << 16) |
             (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) &
                        0xFFFF)));
        vg_path_append(&s_miter.path, first);
      }
      s_miter.fill_color = VG_COLOR_NONE;
      s_miter.stroke_color = 0xFFFF6666;
      s_miter.stroke_width = 30.0f;
      s_miter.stroke_cap = VG_CAP_BUTT;
      s_miter.stroke_join = VG_JOIN_MITER;
      s_miter.miter_limit = 10.0f;
      s_miter.transform = NULL;
      s_miter.has_fill = false;
      s_miter.has_stroke = true;
    }

    // time-based rotation (square) and scaling (circle)
    float t = (sdl_app_ticks(app) - start) / 1000.0f;
    vg_transform_rotate(&rot, t);
    vg_transform_multiply(&tmp, &rot, &t_neg_center);
    vg_transform_multiply(&transform, &t_center, &tmp);
    // Triangle rotates opposite direction
    vg_transform_rotate(&rot_neg, -t);
    vg_transform_multiply(&tmp, &rot_neg, &t_neg_center);
    vg_transform_multiply(&tri_xform, &t_center, &tmp);

    // Circle pulsates: scale between 0.6 and 1.4
    float s = 1.0f + 0.4f * sinf(t * 2.0f);
    vg_transform_scale(&scl, s, s);
    vg_transform_multiply(&tmp, &scl, &t_neg_center);
    vg_transform_multiply(&circle_xform, &t_center, &tmp);

    // Lock -> draw -> unlock/present
    if (!frame->lock(frame)) {
      break;
    }
    pix_frame_clear(frame, clear_color);
    vg_canvas_render_all(&canvas, frame, 0xFFFFFFFFu, /* no fallback fill */
                         0xFFFFFFFFu, /* fallback stroke white */
                         VG_FILL_EVEN_ODD);
    frame->unlock(frame);

    sdl_app_delay(app, 16); // ~60 FPS
  }

  // Cleanup
  vg_path_finish(&shape.path);
  vg_path_finish(&circle.path);
  vg_path_finish(&s_bevel.path);
  vg_path_finish(&s_round.path);
  vg_path_finish(&s_miter.path);
  sdl_app_destroy(app);
  return 0;
}
