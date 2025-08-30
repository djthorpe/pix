#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/pix/frame_internal.h"
#include "canvas.h"
#include "tiger.h"
#include <SDL2/SDL.h>
#include <pix/pix.h>
#include <pixsdl/pixsdl.h>
#include <vg/vg.h>


int main(void) {
  int win_w = 640, win_h = (int)(640.0f * (tigerMaxY / tigerMaxX));
  sdl_app_t *app = sdl_app_create(win_w, win_h, PIX_FMT_RGBA32, "Tiger (VG)");
  if (!app)
    return 1;
  // Build tiger canvas (idempotent); ignore returned value since globals used.
  tiger_build_canvas(NULL, NULL); // idempotent
  update_transform(win_w, win_h);
  pix_frame_t *frame = sdl_app_get_frame(app);
  uint32_t clear = 0xFFFFFFFFu; // white background

  // Debug environment controls removed.

  int running = 1;
  int dragging = 0;
  int drag_start_x = 0, drag_start_y = 0;
  float drag_origin_pan_x = 0.f, drag_origin_pan_y = 0.f;
  while (running) {
    // Event / keyboard input
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT)
        running = 0;
      if (ev.type == SDL_KEYDOWN) {
        SDL_Keycode kc = ev.key.keysym.sym;
        if (kc == SDLK_ESCAPE) {
          running = 0;
        } else if (kc == SDLK_MINUS || kc == SDLK_KP_MINUS) { // zoom out
          g_user_scale /= 1.1f;
          if (g_user_scale < 0.05f)
            g_user_scale = 0.05f;
          update_transform(win_w, win_h);
        } else if (kc == SDLK_EQUALS || kc == SDLK_PLUS ||
                   kc == SDLK_KP_PLUS) { // zoom in ('=' shares key with '+')
          g_user_scale *= 1.1f;
          if (g_user_scale > 40.0f)
            g_user_scale = 40.0f;
          update_transform(win_w, win_h);
        } else if (kc == SDLK_0) { // reset scale & pan & rotation
          g_user_scale = 1.0f;
          g_user_pan_x = 0.0f;
          g_user_pan_y = 0.0f;
          g_user_rotate = 0.0f;
          update_transform(win_w, win_h);
        } else if (kc == SDLK_LEFT) {               // rotate CCW
          float step = 5.0f * (float)M_PI / 180.0f; // 5 degrees
          g_user_rotate += step;
          update_transform(win_w, win_h);
        } else if (kc == SDLK_RIGHT) { // rotate CW
          float step = 5.0f * (float)M_PI / 180.0f;
          g_user_rotate -= step;
          update_transform(win_w, win_h);
        } else if (kc == SDLK_UP) {
          float step = 40.0f;
          g_user_pan_y -= step; // negative moves artwork up
          update_transform(win_w, win_h);
        } else if (kc == SDLK_DOWN) {
          float step = 40.0f;
          g_user_pan_y += step;
          update_transform(win_w, win_h);
        } else if (kc == SDLK_r) { // reset pan only
          g_user_pan_x = 0.0f;
          g_user_pan_y = 0.0f;
          g_user_rotate = 0.0f;
          update_transform(win_w, win_h);
        }
      } else if (ev.type == SDL_MOUSEBUTTONDOWN &&
                 ev.button.button == SDL_BUTTON_LEFT) {
        dragging = 1;
        drag_start_x = ev.button.x;
        drag_start_y = ev.button.y;
        drag_origin_pan_x = g_user_pan_x;
        drag_origin_pan_y = g_user_pan_y;
      } else if (ev.type == SDL_MOUSEBUTTONUP &&
                 ev.button.button == SDL_BUTTON_LEFT) {
        dragging = 0;
      } else if (ev.type == SDL_MOUSEMOTION && dragging) {
        int dx = ev.motion.x - drag_start_x;
        int dy = ev.motion.y - drag_start_y;
        g_user_pan_x = drag_origin_pan_x + (float)dx;
        g_user_pan_y = drag_origin_pan_y + (float)dy;
        update_transform(win_w, win_h);
      } else if (ev.type == SDL_MOUSEWHEEL) {
        // SDL wheel: y>0 up (zoom in), y<0 down (zoom out)
        int steps = ev.wheel.y;
        if (steps == 0 && ev.wheel.preciseY != 0.0f) {
          // Fallback to precise delta rounding
          steps = (ev.wheel.preciseY > 0.f) ? 1 : -1;
        }
        if (steps != 0) {
          float prev_scale = g_user_scale;
          float factor = powf(1.1f, (float)steps);
          g_user_scale *= factor;
          if (g_user_scale < 0.05f)
            g_user_scale = 0.05f;
          if (g_user_scale > 40.0f)
            g_user_scale = 40.0f;
          // Optional: keep zoom centered on cursor by adjusting pan.
          // For now, simple center zoom (pan unchanged). Future enhancement
          // could compute anchor point in object space and preserve it.
          if (g_user_scale != prev_scale)
            update_transform(win_w, win_h);
        }
      }
    }
    int nw, nh;
    sdl_app_get_size(app, &nw, &nh);
    if (nw != win_w || nh != win_h) {
      win_w = nw;
      win_h = nh;
      update_transform(win_w, win_h);
    }
    if (!frame->lock(frame))
      break;
    pix_frame_clear(frame, clear);
    vg_canvas_render(&g_canvas, frame); // always render full canvas
    frame->unlock(frame);
    sdl_app_delay(app, 16);
  }

  free_tiger_shapes();
  sdl_app_destroy(app);
  return 0;
}
