#include <pix/pix.h>
#include <pixsdl/pixsdl.h>
#include <stdlib.h>

typedef struct sdl_line_cmd_t {
  int x0, y0, x1, y1;
  uint8_t r, g, b, a; // 0xAARRGGBB order mapped to SDL_SetRenderDrawColor RGBA
} sdl_line_cmd_t;

static void sdl_queue_line(pix_frame_sdl_ctx_t *ctx, pix_point_t a,
                           pix_point_t b, pix_color_t color) {
  if (!ctx)
    return;
  if (ctx->line_count == ctx->line_capacity) {
    size_t new_cap = ctx->line_capacity ? ctx->line_capacity * 2 : 64;
    void *new_buf = realloc(ctx->line_cmds, new_cap * sizeof(sdl_line_cmd_t));
    if (!new_buf)
      return; // drop if OOM
    ctx->line_cmds = new_buf;
    ctx->line_capacity = new_cap;
  }
  sdl_line_cmd_t *arr = (sdl_line_cmd_t *)ctx->line_cmds;
  sdl_line_cmd_t *cmd = &arr[ctx->line_count++];
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
  if (!frame || !frame->user) {
    return;
  }
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)frame->user;
  // Queue for GPU draw; coordinates are in window space since we render after
  // copying the texture to the renderer with identity transform
  sdl_queue_line(ctx, a, b, color);
}

static bool pix_frame_sdl_lock(pix_frame_t *frame) {
  if (!frame || !frame->user)
    return false;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)frame->user;
  void *pixels = NULL;
  int pitch = 0;
  if (SDL_LockTexture(ctx->texture, NULL, &pixels, &pitch) != 0) {
    return false;
  }
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
  // Flush queued lines using GPU
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
    ctx->line_count = 0; // keep capacity, reuse buffer
  }
  SDL_RenderPresent(ctx->renderer);
}

bool pix_frame_init_sdl(pix_frame_t *frame, SDL_Renderer *ren, SDL_Texture *tex,
                        int w, int h, pix_format_t fmt) {
  if (!frame || !ren || !tex)
    return false;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)malloc(sizeof(*ctx));
  if (!ctx)
    return false;
  ctx->renderer = ren;
  ctx->texture = tex;
  ctx->tex_w = w;
  ctx->tex_h = h;
  ctx->owns_texture = false;
  // Initialize GPU line queue
  ctx->line_cmds = NULL;
  ctx->line_count = 0;
  ctx->line_capacity = 0;

  frame->user = ctx;
  frame->size.w = (uint16_t)w;
  frame->size.h = (uint16_t)h;
  frame->format = fmt;
  frame->lock = pix_frame_sdl_lock;
  frame->unlock = pix_frame_sdl_unlock;
  frame->set_pixel = pix_frame_set_pixel;
  // Override draw_line to use hardware if available
  frame->draw_line = pix_frame_sdl_draw_line;
  frame->pixels = NULL;
  frame->stride = 0;
  // Provide cleanup via finalize hook so callers can just call frame->finalize.
  frame->finalize = pix_frame_destroy_sdl; // legacy name retained for now
  return true;
}

bool pix_frame_init_sdl_auto(pix_frame_t *frame, SDL_Renderer *ren, int w,
                             int h, pix_format_t fmt) {
  if (!frame || !ren)
    return false;
  Uint32 sdl_fmt = (fmt == PIX_FMT_RGB24) ? SDL_PIXELFORMAT_RGB888
                                          : SDL_PIXELFORMAT_ARGB8888;
  SDL_Texture *tex =
      SDL_CreateTexture(ren, sdl_fmt, SDL_TEXTUREACCESS_STREAMING, w, h);
  if (!tex)
    return false;
  if (!pix_frame_init_sdl(frame, ren, tex, w, h, fmt)) {
    SDL_DestroyTexture(tex);
    return false;
  }
  ((pix_frame_sdl_ctx_t *)frame->user)->owns_texture = true;
  return true;
}

bool pix_frame_resize_sdl(pix_frame_t *frame, int w, int h) {
  if (!frame || !frame->user)
    return false;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)frame->user;
  if (!ctx->owns_texture)
    return false;
  // Ensure unlocked before resize
  SDL_UnlockTexture(ctx->texture);
  SDL_DestroyTexture(ctx->texture);
  Uint32 sdl_fmt = (frame->format == PIX_FMT_RGB24) ? SDL_PIXELFORMAT_RGB888
                                                    : SDL_PIXELFORMAT_ARGB8888;
  SDL_Texture *tex = SDL_CreateTexture(ctx->renderer, sdl_fmt,
                                       SDL_TEXTUREACCESS_STREAMING, w, h);
  if (!tex)
    return false;
  ctx->texture = tex;
  ctx->tex_w = w;
  ctx->tex_h = h;
  frame->size.w = (uint16_t)w;
  frame->size.h = (uint16_t)h;
  frame->pixels = NULL;
  frame->stride = 0;
  // Reset any queued GPU lines (coordinates no longer valid)
  ctx->line_count = 0;
  return true;
}

bool pix_frame_rebind_sdl(pix_frame_t *frame, SDL_Texture *tex, int w, int h) {
  if (!frame || !frame->user || !tex)
    return false;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)frame->user;
  if (ctx->owns_texture && ctx->texture) {
    SDL_DestroyTexture(ctx->texture);
  }
  ctx->texture = tex;
  ctx->tex_w = w;
  ctx->tex_h = h;
  ctx->owns_texture = false;
  frame->size.w = (uint16_t)w;
  frame->size.h = (uint16_t)h;
  frame->pixels = NULL;
  frame->stride = 0;
  // Reset any queued GPU lines on rebind
  ctx->line_count = 0;
  return true;
}

void pix_frame_destroy_sdl(pix_frame_t *frame) {
  if (!frame)
    return;
  pix_frame_sdl_ctx_t *ctx = (pix_frame_sdl_ctx_t *)frame->user;
  if (!ctx)
    return;
  // Ensure unlocked
  SDL_UnlockTexture(ctx->texture);
  if (ctx->owns_texture && ctx->texture) {
    SDL_DestroyTexture(ctx->texture);
  }
  if (ctx->line_cmds) {
    free(ctx->line_cmds);
  }
  free(ctx);
  frame->user = NULL;
  frame->pixels = NULL;
  frame->stride = 0;
  frame->finalize = NULL;
}
