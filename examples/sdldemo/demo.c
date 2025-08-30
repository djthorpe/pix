#include "../../src/pix/frame_internal.h"
#include "../../src/vg/shape_internal.h" /* access to internal create/destroy until refactored */
#include <math.h>
#include <pix/pix.h>
#include <pixsdl/pixsdl.h>
#include <vg/vg.h>

static void free_text_shapes_from(vg_canvas_t *c, size_t start) {
  if (!c)
    return;
  if (start >= c->size)
    return;
  for (size_t i = start; i < c->size; ++i) {
    vg_shape_t *s = c->shapes[i];
    if (s) {
      const vg_transform_t *xf = vg_shape_get_transform(s);
      if (xf)
        VG_FREE((void *)xf);
      vg_shape_path_clear(s, 4);
      vg_shape_destroy(s);
    }
  }
  c->size = start;
}

int main(int argc, char *argv[]) {
  // Flag: --no-text disables text shape rendering
  bool disable_text = false;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--no-text") == 0)
      disable_text = true;
  }
  int width = 640, height = 480;
  // Use RGBA so per-shape alpha is honored by our software blending
  sdl_app_t *app = sdl_app_create(width, height, PIX_FMT_RGBA32, "VG Demo");
  if (!app) {
    return 1;
  }

  // Build a square path centered on screen
  int cx = width / 2;
  int cy = height / 2;
  int side = (width < height ? width : height) / 3;
  int h = side / 2;
  int cr = (width < height ? width : height) / 6; // circle base radius

  // Reserve space for animated shapes + optional text
  vg_canvas_t canvas = vg_canvas_init(256);

  vg_shape_t *shape = vg_canvas_append(&canvas);
  vg_shape_init_rect(shape, (pix_point_t){(int16_t)(cx - h), (int16_t)(cy - h)},
                     (pix_size_t){(uint16_t)side, (uint16_t)side});
  vg_shape_set_transform(shape, NULL); // set below
  vg_shape_set_fill_color(shape, 0xFF0000FF);
  vg_shape_set_stroke_color(shape, 0xFFFFFFFF);
  vg_shape_set_stroke_width(shape, 10.0f);
  vg_shape_set_stroke_cap(shape, VG_CAP_ROUND);
  vg_shape_set_stroke_join(shape, VG_JOIN_ROUND);

  // Circle that scales up/down at center
  vg_shape_t *circle = vg_canvas_append(&canvas);
  vg_shape_init_circle(circle, (pix_point_t){(int16_t)cx, (int16_t)cy},
                       (pix_scalar_t)cr, 64);
  vg_shape_set_transform(circle, NULL);
  vg_shape_set_fill_color(circle, 0x80FF0000);
  vg_shape_set_stroke_color(circle, 0xA0FFFFFF);
  vg_shape_set_stroke_width(circle, 8.0f);
  vg_shape_set_stroke_cap(circle, VG_CAP_ROUND);
  vg_shape_set_stroke_join(circle, VG_JOIN_ROUND);

  // Green triangle rotating opposite direction
  vg_shape_t *tri = vg_canvas_append(&canvas);
  int tx0 = cx, ty0 = cy - h;     // top
  int tx1 = cx - h, ty1 = cy + h; // bottom-left
  int tx2 = cx + h, ty2 = cy + h; // bottom-right
  vg_shape_init_triangle(tri, (pix_point_t){(int16_t)tx0, (int16_t)ty0},
                         (pix_point_t){(int16_t)tx1, (int16_t)ty1},
                         (pix_point_t){(int16_t)tx2, (int16_t)ty2});
  vg_shape_set_transform(tri, NULL);
  vg_shape_set_fill_color(tri, 0x8000FF00);
  vg_shape_set_stroke_color(tri, 0xFFFFFFFF);
  vg_shape_set_stroke_width(tri, 6.0f);
  vg_shape_set_stroke_cap(tri, VG_CAP_ROUND);
  vg_shape_set_stroke_join(tri, VG_JOIN_ROUND);

  // Stroke join showcase: three 5-point stars (bevel, round, miter)
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
  vg_shape_t *s_bevel = vg_canvas_append(&canvas);
  vg_shape_t *s_round = vg_canvas_append(&canvas);
  vg_shape_t *s_miter = vg_canvas_append(&canvas);
  // Build stars
  {
    // Bevel star
    vg_shape_path_clear(s_bevel, 11);
    for (int i = 0; i < 10; ++i) {
      float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
      float rad = (i % 2 == 0) ? (float)R : (float)r;
      int px = c1x + (int)lroundf(rad * cosf(ang));
      int py = cy0 + (int)lroundf(rad * sinf(ang));
      vg_path_append(vg_shape_path(s_bevel),
                     &(pix_point_t){(int16_t)px, (int16_t)py}, NULL);
    }
    pix_point_t first1 = {
        (int16_t)(c1x + (int)lroundf((float)R * cosf(-M_PI_2))),
        (int16_t)(cy0 + (int)lroundf((float)R * sinf(-M_PI_2)))};
    vg_path_append(vg_shape_path(s_bevel), &first1, NULL);

    // Round star
    vg_shape_path_clear(s_round, 11);
    for (int i = 0; i < 10; ++i) {
      float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
      float rad = (i % 2 == 0) ? (float)R : (float)r;
      int px = c2x + (int)lroundf(rad * cosf(ang));
      int py = cy0 + (int)lroundf(rad * sinf(ang));
      vg_path_append(vg_shape_path(s_round),
                     &(pix_point_t){(int16_t)px, (int16_t)py}, NULL);
    }
    pix_point_t first2 = {
        (int16_t)(c2x + (int)lroundf((float)R * cosf(-M_PI_2))),
        (int16_t)(cy0 + (int)lroundf((float)R * sinf(-M_PI_2)))};
    vg_path_append(vg_shape_path(s_round), &first2, NULL);

    // Miter star
    vg_shape_path_clear(s_miter, 11);
    for (int i = 0; i < 10; ++i) {
      float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
      float rad = (i % 2 == 0) ? (float)R : (float)r;
      int px = c3x + (int)lroundf(rad * cosf(ang));
      int py = cy0 + (int)lroundf(rad * sinf(ang));
      vg_path_append(vg_shape_path(s_miter),
                     &(pix_point_t){(int16_t)px, (int16_t)py}, NULL);
    }
    pix_point_t first3 = {
        (int16_t)(c3x + (int)lroundf((float)R * cosf(-M_PI_2))),
        (int16_t)(cy0 + (int)lroundf((float)R * sinf(-M_PI_2)))};
    vg_path_append(vg_shape_path(s_miter), &first3, NULL);
  }
  vg_shape_set_fill_color(s_bevel, PIX_COLOR_NONE);
  vg_shape_set_stroke_color(s_bevel, 0xFF66FF66);
  vg_shape_set_stroke_width(s_bevel, 30.0f);
  vg_shape_set_stroke_cap(s_bevel, VG_CAP_BUTT);
  vg_shape_set_stroke_join(s_bevel, VG_JOIN_BEVEL);
  vg_shape_set_miter_limit(s_bevel, 4.0f);
  vg_shape_set_transform(s_bevel, NULL);
  vg_shape_set_fill_color(s_round, PIX_COLOR_NONE);
  vg_shape_set_stroke_color(s_round, 0xFF66AAFF);
  vg_shape_set_stroke_width(s_round, 30.0f);
  vg_shape_set_stroke_cap(s_round, VG_CAP_BUTT);
  vg_shape_set_stroke_join(s_round, VG_JOIN_ROUND);
  vg_shape_set_miter_limit(s_round, 4.0f);
  vg_shape_set_transform(s_round, NULL);
  // Miter star (with high limit)
  vg_shape_set_fill_color(s_miter, PIX_COLOR_NONE);
  vg_shape_set_stroke_color(s_miter, 0xFFFF6666);
  vg_shape_set_stroke_width(s_miter, 30.0f);
  vg_shape_set_stroke_cap(s_miter, VG_CAP_BUTT);
  vg_shape_set_stroke_join(s_miter, VG_JOIN_MITER);
  vg_shape_set_miter_limit(s_miter, 10.0f);
  vg_shape_set_transform(s_miter, NULL);

  // Remember count before appending text
  size_t base_shapes = canvas.size;

  // Append centered demo text
  const char *demo_text = "PIX VECTOR FONT DEMO";
  float text_px = 48.0f; // target pixel height
  float txt_w = 0.0f;
  vg_shape_t *text_shape_center = NULL; // owned clone we append (centered)
  vg_shape_t *text_shape = NULL;
  if (!disable_text)
    text_shape = vg_font_get_text_shape_cached(
        &vg_font_tiny5x7, demo_text, 0xFFFFFFFFu, text_px, 1.0f, &txt_w);
  if (text_shape) {
    text_shape_center = vg_shape_create(); /* internal */
    if (text_shape_center) {
      *vg_shape_path(text_shape_center) =
          *vg_shape_path(text_shape); // shallow path copy
      // Centering transform = translate * (glyph scale)
      vg_transform_t *xf = (vg_transform_t *)VG_MALLOC(sizeof(vg_transform_t));
      if (xf) {
        vg_transform_t tr;
        float scale_text = text_px / 7.0f;      // glyph EM height = 7
        float baseline = (float)height - 20.0f; // bottom margin
        float ty = baseline - scale_text * vg_font_tiny5x7.ascent;
        vg_transform_translate(&tr, (width - txt_w) * 0.5f, ty);
        const vg_transform_t *orig_xf = vg_shape_get_transform(text_shape);
        if (orig_xf) {
          vg_transform_multiply(xf, &tr, orig_xf); // T * S
        } else {
          *xf = tr;
        }
        vg_shape_set_transform(text_shape_center, xf);
        if (canvas.size < canvas.capacity) {
          canvas.shapes[canvas.size++] =
              text_shape_center; // adopt external shape
        }
      } else {
        vg_shape_path_clear(text_shape_center, 4);
        vg_shape_destroy(text_shape_center); /* internal */
        text_shape_center = NULL;
      }
    }
  }

  pix_frame_t *frame = sdl_app_get_frame(app);

  // Animation state
  vg_transform_t transform, t_center, t_neg_center, rot, rot_neg, tmp;
  vg_transform_t circle_xform, tri_xform, scl; // reuse centers for both
  vg_transform_translate(&t_center, (float)cx, (float)cy);
  vg_transform_translate(&t_neg_center, (float)-cx, (float)-cy);
  vg_shape_set_transform(shape, &transform);
  vg_shape_set_transform(circle, &circle_xform);
  vg_shape_set_transform(tri, &tri_xform);

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
      // Window resized: recompute geometry & transforms
      cx = width / 2;
      cy = height / 2;
      side = (width < height ? width : height) / 3;
      h = side / 2;
      cr = (width < height ? width : height) / 6;
      vg_transform_translate(&t_center, (float)cx, (float)cy);
      vg_transform_translate(&t_neg_center, (float)-cx, (float)-cy);
      // Rebuild square at new size
      vg_shape_path_clear(shape, 5);
      vg_shape_init_rect(shape,
                         (pix_point_t){(int16_t)(cx - h), (int16_t)(cy - h)},
                         (pix_size_t){(uint16_t)side, (uint16_t)side});
      vg_shape_set_fill_color(shape, 0xFF0000FF);
      vg_shape_set_stroke_color(shape, 0xFFFFFFFF);
      vg_shape_set_stroke_width(shape, 10.0f);
      vg_shape_set_stroke_cap(shape, VG_CAP_ROUND);
      vg_shape_set_stroke_join(shape, VG_JOIN_ROUND);
      vg_shape_set_transform(shape, &transform); // rebind transform
                                                 // Rebuild circle at new size
      vg_shape_path_clear(circle, 65);
      vg_shape_init_circle(circle, (pix_point_t){(int16_t)cx, (int16_t)cy},
                           (pix_scalar_t)cr, 64);
      vg_shape_set_fill_color(circle, 0x80FF0000);
      vg_shape_set_stroke_color(circle, 0xA0FFFFFF);
      vg_shape_set_stroke_width(circle, 8.0f);
      vg_shape_set_stroke_cap(circle, VG_CAP_ROUND);
      vg_shape_set_stroke_join(circle, VG_JOIN_ROUND);
      vg_shape_set_transform(circle, &circle_xform);
      // Rebuild triangle at new size
      vg_shape_path_clear(tri, 4);
      tx0 = cx;
      ty0 = cy - h;
      tx1 = cx - h;
      ty1 = cy + h;
      tx2 = cx + h;
      ty2 = cy + h;
      vg_shape_init_triangle(tri, (pix_point_t){(int16_t)tx0, (int16_t)ty0},
                             (pix_point_t){(int16_t)tx1, (int16_t)ty1},
                             (pix_point_t){(int16_t)tx2, (int16_t)ty2});
      vg_shape_set_fill_color(tri, 0x8000FF00);
      vg_shape_set_stroke_color(tri, 0xFFFFFFFF);
      vg_shape_set_stroke_width(tri, 6.0f);
      vg_shape_set_stroke_cap(tri, VG_CAP_ROUND);
      vg_shape_set_stroke_join(tri, VG_JOIN_ROUND);
      vg_shape_set_transform(tri, &tri_xform);

      // Rebuild join demo stars
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
      vg_shape_path_clear(s_bevel, 11);
      for (int i = 0; i < 10; ++i) {
        float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
        float rad = (i % 2 == 0) ? (float)R : (float)r;
        int px = c1x + (int)lroundf(rad * cosf(ang));
        int py = cy0 + (int)lroundf(rad * sinf(ang));
        vg_path_append(vg_shape_path(s_bevel),
                       &(pix_point_t){(int16_t)px, (int16_t)py}, NULL);
      }
      {
        int32_t first =
            ((int32_t)(((c1x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                       << 16) |
             (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) &
                        0xFFFF)));
        vg_path_append(vg_shape_path(s_bevel),
                       &(pix_point_t){(int16_t)((first >> 16) & 0xFFFF),
                                      (int16_t)(first & 0xFFFF)},
                       NULL);
      }
      vg_shape_set_fill_color(s_bevel, PIX_COLOR_NONE);
      vg_shape_set_stroke_color(s_bevel, 0xFF66FF66);
      vg_shape_set_stroke_width(s_bevel, 30.0f);
      vg_shape_set_stroke_cap(s_bevel, VG_CAP_BUTT);
      vg_shape_set_stroke_join(s_bevel, VG_JOIN_BEVEL);
      vg_shape_set_miter_limit(s_bevel, 4.0f);
      vg_shape_set_transform(s_bevel, NULL);
      vg_shape_path_clear(s_round, 11);
      for (int i = 0; i < 10; ++i) {
        float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
        float rad = (i % 2 == 0) ? (float)R : (float)r;
        int px = c2x + (int)lroundf(rad * cosf(ang));
        int py = cy0 + (int)lroundf(rad * sinf(ang));
        vg_path_append(vg_shape_path(s_round),
                       &(pix_point_t){(int16_t)px, (int16_t)py}, NULL);
      }
      {
        int32_t first =
            ((int32_t)(((c2x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                       << 16) |
             (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) &
                        0xFFFF)));
        vg_path_append(vg_shape_path(s_round),
                       &(pix_point_t){(int16_t)((first >> 16) & 0xFFFF),
                                      (int16_t)(first & 0xFFFF)},
                       NULL);
      }
      vg_shape_set_fill_color(s_round, PIX_COLOR_NONE);
      vg_shape_set_stroke_color(s_round, 0xFF66AAFF);
      vg_shape_set_stroke_width(s_round, 30.0f);
      vg_shape_set_stroke_cap(s_round, VG_CAP_BUTT);
      vg_shape_set_stroke_join(s_round, VG_JOIN_ROUND);
      vg_shape_set_miter_limit(s_round, 4.0f);
      vg_shape_set_transform(s_round, NULL);
      vg_shape_path_clear(s_miter, 11);
      for (int i = 0; i < 10; ++i) {
        float ang = (float)i * (float)M_PI / 5.0f - (float)M_PI_2;
        float rad = (i % 2 == 0) ? (float)R : (float)r;
        int px = c3x + (int)lroundf(rad * cosf(ang));
        int py = cy0 + (int)lroundf(rad * sinf(ang));
        vg_path_append(vg_shape_path(s_miter),
                       &(pix_point_t){(int16_t)px, (int16_t)py}, NULL);
      }
      {
        int32_t first =
            ((int32_t)(((c3x + (int)lroundf((float)R * cosf(-M_PI_2))) & 0xFFFF)
                       << 16) |
             (int32_t)(((cy0 + (int)lroundf((float)R * sinf(-M_PI_2))) &
                        0xFFFF)));
        vg_path_append(vg_shape_path(s_miter),
                       &(pix_point_t){(int16_t)((first >> 16) & 0xFFFF),
                                      (int16_t)(first & 0xFFFF)},
                       NULL);
      }
      vg_shape_set_fill_color(s_miter, PIX_COLOR_NONE);
      vg_shape_set_stroke_color(s_miter, 0xFFFF6666);
      vg_shape_set_stroke_width(s_miter, 30.0f);
      vg_shape_set_stroke_cap(s_miter, VG_CAP_BUTT);
      vg_shape_set_stroke_join(s_miter, VG_JOIN_MITER);
      vg_shape_set_miter_limit(s_miter, 10.0f);
      vg_shape_set_transform(s_miter, NULL);

      // Recenter existing text (preserve scale)
      if (text_shape_center) {
        const vg_transform_t *old_xf =
            vg_shape_get_transform(text_shape_center);
        if (old_xf)
          VG_FREE((void *)old_xf);
        vg_transform_t *xf =
            (vg_transform_t *)VG_MALLOC(sizeof(vg_transform_t));
        if (xf) {
          vg_transform_t tr;
          float scale_text = text_px / 7.0f;
          float baseline = (float)height - 20.0f;
          float ty = baseline - scale_text * vg_font_tiny5x7.ascent;
          vg_transform_translate(&tr, (width - txt_w) * 0.5f, ty);
          if (text_shape && vg_shape_get_transform(text_shape)) {
            vg_transform_multiply(xf, &tr, vg_shape_get_transform(text_shape));
          } else {
            *xf = tr;
          }
          vg_shape_set_transform(text_shape_center, xf);
        } else {
          vg_shape_set_transform(text_shape_center, NULL);
        }
      }
    }

    // Animate transforms
    float t = (sdl_app_ticks(app) - start) / 1000.0f;
    vg_transform_rotate(&rot, t);
    vg_transform_multiply(&tmp, &rot, &t_neg_center);
    vg_transform_multiply(&transform, &t_center, &tmp);
    // Triangle rotates opposite direction
    vg_transform_rotate(&rot_neg, -t);
    vg_transform_multiply(&tmp, &rot_neg, &t_neg_center);
    vg_transform_multiply(&tri_xform, &t_center, &tmp);

    // Circle pulsates
    float s = 1.0f + 0.4f * sinf(t * 2.0f);
    vg_transform_scale(&scl, s, s);
    vg_transform_multiply(&tmp, &scl, &t_neg_center);
    vg_transform_multiply(&circle_xform, &t_center, &tmp);

    // Render frame
    if (!frame->lock(frame)) {
      break;
    }
    pix_frame_clear(frame, clear_color);
    vg_canvas_render(&canvas, frame);
    // (Vector text only; debug bitmap path removed)
    frame->unlock(frame);

    sdl_app_delay(app, 16); // ~60 FPS
  }

  // Cleanup
  /* Paths cleaned up by shape destruction */
  // Free text glyph shapes
  free_text_shapes_from(&canvas, base_shapes);
  sdl_app_destroy(app);
  return 0;
}
