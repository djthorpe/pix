#include <SDL2/SDL.h>
#include <sdl/frame_sdl.h>
#include <sdl/sdl_app.h>
#include <stdlib.h>

struct sdl_app_t {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  pix_frame_t frame;
  int width;
  int height;
};

sdl_app_t *sdl_app_create(int width, int height, pix_format_t fmt,
                          const char *title) {
  sdl_app_t *app = (sdl_app_t *)malloc(sizeof(*app));
  if (!app)
    return NULL;
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    free(app);
    return NULL;
  }
  if (SDL_CreateWindowAndRenderer(width, height,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE,
                                  &app->window, &app->renderer) != 0) {
    SDL_Quit();
    free(app);
    return NULL;
  }
  if (title)
    SDL_SetWindowTitle(app->window, title);

  Uint32 sdl_fmt = (fmt == PIX_FMT_RGB888) ? SDL_PIXELFORMAT_RGB24
                                           : SDL_PIXELFORMAT_ABGR8888;
  app->texture = SDL_CreateTexture(app->renderer, sdl_fmt,
                                   SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!app->texture) {
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_Quit();
    free(app);
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
    free(app);
    return NULL;
  }
  // We do our own blending when RGBA; keep SDL blending off when we present
  SDL_SetTextureBlendMode(app->texture, SDL_BLENDMODE_NONE);
  return app;
}

void sdl_app_destroy(sdl_app_t *app) {
  if (!app)
    return;
  pix_frame_destroy_sdl(&app->frame);
  if (app->texture)
    SDL_DestroyTexture(app->texture);
  if (app->renderer)
    SDL_DestroyRenderer(app->renderer);
  if (app->window)
    SDL_DestroyWindow(app->window);
  SDL_Quit();
  free(app);
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
      int new_w = e.window.data1;
      int new_h = e.window.data2;
      Uint32 sdl_fmt = (app->frame.format == PIX_FMT_RGB888)
                           ? SDL_PIXELFORMAT_RGB24
                           : SDL_PIXELFORMAT_ABGR8888;
      SDL_Texture *new_tex = SDL_CreateTexture(
          app->renderer, sdl_fmt, SDL_TEXTUREACCESS_STREAMING, new_w, new_h);
      if (new_tex) {
        if (app->texture)
          SDL_DestroyTexture(app->texture);
        app->texture = new_tex;
        SDL_SetTextureBlendMode(app->texture, SDL_BLENDMODE_NONE);
        pix_frame_rebind_sdl(&app->frame, app->texture, new_w, new_h);
        app->width = new_w;
        app->height = new_h;
      }
    }
  }
  return false;
}

void sdl_app_get_size(sdl_app_t *app, int *out_w, int *out_h) {
  if (!app)
    return;
  if (out_w)
    *out_w = app->width;
  if (out_h)
    *out_h = app->height;
}
