// SDL backend integrated into core pix library when SDL2 is found.
#include "frame_internal.h"
#include <SDL2/SDL.h>
#include <pix/pix.h>
#include <pix/sdl.h>
#include <vg/vg.h>

typedef struct pix_frame_sdl_ctx_t {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  int tex_w, tex_h;
  bool owns_texture;
  bool owns_window;
  void *line_cmds; /* sdl_line_cmd_t[] */
  size_t line_count;
  size_t line_capacity;
} pix_frame_sdl_ctx_t;

typedef struct sdl_line_cmd_t {
  int x0, y0, x1, y1;
  uint8_t r, g, b, a;
} sdl_line_cmd_t;

static void sdl_queue_line(pix_frame_sdl_ctx_t *ctx, pix_point_t a,
                           pix_point_t b, pix_color_t color) {
  if (!ctx)
    return;
  if (ctx->line_count == ctx->line_capacity) {
    size_t new_cap = ctx->line_capacity ? ctx->line_capacity * 2 : 64;
    void *new_buf =
        VG_REALLOC(ctx->line_cmds, new_cap * sizeof(sdl_line_cmd_t));
    if (!new_buf)
      return; // drop if OOM
    ctx->line_cmds = new_buf;
    ctx->line_capacity = new_cap;
  }
  sdl_line_cmd_t *cmd = &((sdl_line_cmd_t *)ctx->line_cmds)[ctx->line_count++];
  cmd->x0 = a.x;
  cmd->y0 = a.y;
  cmd->x1 = b.x;
  cmd->y1 = b.y;
  cmd->a = (color >> 24) & 0xFF;
  cmd->r = (color >> 16) & 0xFF;
  cmd->g = (color >> 8) & 0xFF;
  cmd->b = color & 0xFF;
}

static void pix_frame_sdl_draw_line(pix_frame_t *frame, pix_point_t a,
                                    pix_point_t b, pix_color_t color) {
  if (!frame || !frame->user)
    return;
  sdl_queue_line((pix_frame_sdl_ctx_t *)frame->user, a, b, color);
}

static bool pix_frame_sdl_lock(pix_frame_t *frame) {
  if (!frame || !frame->user)
    return false;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)frame->user;
  void *pixels = NULL;
  int pitch = 0;
  if (SDL_LockTexture(ctx->texture, NULL, &pixels, &pitch) != 0)
    return false;
  frame->pixels = pixels;
  frame->stride = (size_t)pitch;
  return true;
}

static void pix_frame_sdl_unlock(pix_frame_t *frame) {
  if (!frame || !frame->user)
    return;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)frame->user;
  SDL_UnlockTexture(ctx->texture);
  SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);
  if (ctx->line_count && ctx->line_cmds) {
    sdl_line_cmd_t *arr = (sdl_line_cmd_t *)ctx->line_cmds;
    for (size_t i = 0; i < ctx->line_count; ++i) {
      sdl_line_cmd_t *cmd = &arr[i];
      SDL_SetRenderDrawBlendMode(ctx->renderer, cmd->a == 255
                                                    ? SDL_BLENDMODE_NONE
                                                    : SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(ctx->renderer, cmd->r, cmd->g, cmd->b, cmd->a);
      SDL_RenderDrawLine(ctx->renderer, cmd->x0, cmd->y0, cmd->x1, cmd->y1);
    }
    ctx->line_count = 0;
  }
  SDL_RenderPresent(ctx->renderer);
}

static void pix_frame_sdl_destroy(pix_frame_t *frame) {
  if (!frame)
    return;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)frame->user;
  if (ctx) {
    SDL_UnlockTexture(ctx->texture);
    if (ctx->owns_texture && ctx->texture)
      SDL_DestroyTexture(ctx->texture);
    if (ctx->owns_window) {
      if (ctx->renderer)
        SDL_DestroyRenderer(ctx->renderer);
      if (ctx->window)
        SDL_DestroyWindow(ctx->window);
      SDL_Quit();
    }
    if (ctx->line_cmds)
      VG_FREE(ctx->line_cmds);
    VG_FREE(ctx);
  }
  VG_FREE(frame);
}

static bool pix_frame_init_sdl(pix_frame_t *frame, SDL_Window *win,
                               SDL_Renderer *ren, SDL_Texture *tex, int w,
                               int h, pix_format_t fmt) {
  if (!frame || !ren || !tex)
    return false;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)VG_MALLOC(sizeof(*ctx));
  if (!ctx)
    return false;
  *ctx = (pix_frame_sdl_ctx_t){0};
  ctx->window = win;
  ctx->renderer = ren;
  ctx->texture = tex;
  ctx->tex_w = w;
  ctx->tex_h = h;
  ctx->owns_texture = false;
  ctx->owns_window = false;
  frame->user = ctx;
  frame->size.w = (uint16_t)w;
  frame->size.h = (uint16_t)h;
  frame->format = fmt;
  frame->lock = pix_frame_sdl_lock;
  frame->unlock = pix_frame_sdl_unlock;
  frame->set_pixel = pix_frame_set_pixel;
  frame->get_pixel = pix_frame_get_pixel;
  frame->copy = pix_frame_copy;
  frame->draw_line = pix_frame_sdl_draw_line;
  frame->pixels = NULL;
  frame->stride = 0;
  frame->destroy = pix_frame_sdl_destroy;
  return true;
}

static bool pix_frame_init_sdl_auto(pix_frame_t *frame, SDL_Window *win,
                                    SDL_Renderer *ren, int w, int h,
                                    pix_format_t fmt) {
  if (!frame || !ren)
    return false;
  Uint32 sdl_fmt = (fmt == PIX_FMT_RGB24) ? SDL_PIXELFORMAT_RGB888
                                          : SDL_PIXELFORMAT_ARGB8888;
  SDL_Texture *tex =
      SDL_CreateTexture(ren, sdl_fmt, SDL_TEXTUREACCESS_STREAMING, w, h);
  if (!tex)
    return false;
  if (!pix_frame_init_sdl(frame, win, ren, tex, w, h, fmt)) {
    SDL_DestroyTexture(tex);
    return false;
  }
  ((pix_frame_sdl_ctx_t *)frame->user)->owns_texture = true;
  return true;
}

pix_frame_t *pixsdl_frame_init(const char *title, pix_size_t size,
                               pix_format_t fmt) {
  int w = (int)size.w, h = (int)size.h;
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    return NULL;
  pix_frame_t *frame = (pix_frame_t *)VG_MALLOC(sizeof(pix_frame_t));
  if (!frame) {
    SDL_Quit();
    return NULL;
  }
  *frame = (pix_frame_t){0};
  SDL_Window *win = NULL;
  SDL_Renderer *ren = NULL;
  if (SDL_CreateWindowAndRenderer(w, h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE,
                                  &win, &ren) != 0) {
    VG_FREE(frame);
    SDL_Quit();
    return NULL;
  }
  if (title && win)
    SDL_SetWindowTitle(win, title);
  if (!pix_frame_init_sdl_auto(frame, win, ren, w, h, fmt)) {
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    VG_FREE(frame);
    return NULL;
  }
  ((pix_frame_sdl_ctx_t *)frame->user)->owns_window = true;
  return frame;
}
