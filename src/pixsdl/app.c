#include <SDL2/SDL.h>
#include <pix/pix.h>
#include <pixsdl/pixsdl.h>
#include <stdlib.h>
#include <vg/vg.h>

struct sdl_app_t {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  pix_frame_t frame;
  int width;
  int height;
};

// Internal helper to (re)create streaming texture & rebind frame after a
// window size change. Keeps blend mode consistent and updates stored width/
// height. Returns true on success.
static bool sdl_app_resize_texture(sdl_app_t *app, int new_w, int new_h) {
  if (!app || new_w <= 0 || new_h <= 0)
    return false;
  Uint32 sdl_fmt = (app->frame.format == PIX_FMT_RGB24)
                       ? SDL_PIXELFORMAT_RGB24
                       : SDL_PIXELFORMAT_ARGB8888;
  SDL_Texture *new_tex = SDL_CreateTexture(
      app->renderer, sdl_fmt, SDL_TEXTUREACCESS_STREAMING, new_w, new_h);
  if (!new_tex)
    return false;
  if (app->texture)
    SDL_DestroyTexture(app->texture);
  app->texture = new_tex;
  SDL_SetTextureBlendMode(app->texture, SDL_BLENDMODE_NONE);
  // Rebind frame to new texture (frame does not own texture in this model)
  pix_frame_rebind_sdl(&app->frame, app->texture, new_w, new_h);
  app->width = new_w;
  app->height = new_h;
  return true;
}

sdl_app_t *sdl_app_create(int width, int height, pix_format_t fmt,
                          const char *title) {
  sdl_app_t *app = (sdl_app_t *)VG_MALLOC(sizeof(*app));
  if (!app)
    return NULL;
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    VG_FREE(app);
    return NULL;
  }
  if (SDL_CreateWindowAndRenderer(width, height,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE,
                                  &app->window, &app->renderer) != 0) {
    SDL_Quit();
    VG_FREE(app);
    return NULL;
  }
  if (title)
    SDL_SetWindowTitle(app->window, title);

  Uint32 sdl_fmt =
      (fmt == PIX_FMT_RGB24) ? SDL_PIXELFORMAT_RGB24 : SDL_PIXELFORMAT_ARGB8888;
  app->texture = SDL_CreateTexture(app->renderer, sdl_fmt,
                                   SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!app->texture) {
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_Quit();
    VG_FREE(app);
    return NULL;
  }

  app->width = width;
  app->height = height;
  if (!pix_frame_init_sdl(&app->frame, app->renderer, app->texture, width,
                          height, fmt)) {
    SDL_DestroyTexture(app->texture);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_Quit();
    VG_FREE(app);
    return NULL;
  }
  // We do our own blending when RGBA; keep SDL blending off when we present
  SDL_SetTextureBlendMode(app->texture, SDL_BLENDMODE_NONE);
  return app;
}

void sdl_app_destroy(sdl_app_t *app) {
  if (!app)
    return;
  // Use the finalize hook (backend provided) instead of direct destroy.
  // This keeps sdl_app agnostic of the concrete frame backend cleanup.
  if (app->frame.finalize) {
    app->frame.finalize(&app->frame);
  }
  if (app->texture)
    SDL_DestroyTexture(app->texture);
  if (app->renderer)
    SDL_DestroyRenderer(app->renderer);
  if (app->window)
    SDL_DestroyWindow(app->window);
  SDL_Quit();
  VG_FREE(app);
}

pix_frame_t *sdl_app_get_frame(sdl_app_t *app) {
  return app ? &app->frame : NULL;
}

uint32_t sdl_app_ticks(sdl_app_t *app) {
  (void)app;
  return SDL_GetTicks();
}

void sdl_app_delay(sdl_app_t *app, uint32_t ms) {
  (void)app;
  SDL_Delay(ms);
}

bool sdl_app_poll_should_close(sdl_app_t *app) {
  if (!app)
    return true;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT)
      return true;
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
      return true;
    if (e.type == SDL_WINDOWEVENT &&
        (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         e.window.event == SDL_WINDOWEVENT_RESIZED)) {
      sdl_app_resize_texture(app, e.window.data1, e.window.data2);
    }
  }
  return false;
}

void sdl_app_get_size(sdl_app_t *app, int *out_w, int *out_h) {
  if (!app)
    return;
  // Query actual window size; if SDL was resized but events handled elsewhere
  // (custom loop), we still catch it here and refresh the texture.
  int real_w = 0, real_h = 0;
  SDL_GetWindowSize(app->window, &real_w, &real_h);
  if ((real_w > 0 && real_h > 0) &&
      (real_w != app->width || real_h != app->height)) {
    // Attempt resize; ignore failure (keep old texture)
    sdl_app_resize_texture(app, real_w, real_h);
  }
  if (out_w)
    *out_w = app->width;
  if (out_h)
    *out_h = app->height;
}
