/**
 * @file include/sdl/frame_sdl.h
 * @brief SDL2 backend for pix_frame_t.
 *
 * This header declares helpers to bind a pix_frame_t to an SDL_Renderer and a
 * streaming SDL_Texture so the software renderer can draw into a CPU buffer
 * and present via SDL. The backend also supports queuing GPU-accelerated line
 * draws that are flushed after the texture upload on unlock.
 */
#pragma once
#include <SDL2/SDL.h>
#include <pix/frame.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend-specific SDL context stored in pix_frame_t::user.
 */
typedef struct pix_frame_sdl_ctx_t {
  SDL_Renderer *renderer; /**< SDL renderer used for texture upload/present. */
  SDL_Texture *texture;   /**< Streaming texture backing the frame. */
  int tex_w;              /**< Texture width in pixels. */
  int tex_h;              /**< Texture height in pixels. */
  bool owns_texture; /**< If true, destroy texture in pix_frame_destroy_sdl. */
  /** Queue of GPU draw commands (lines) to render after texture copy. */
  void *line_cmds;      /**< Opaque array of internal line commands. */
  size_t line_count;    /**< Number of queued line commands. */
  size_t line_capacity; /**< Allocated capacity of the command queue. */
} pix_frame_sdl_ctx_t;

/**
 * @brief Initialize a pix_frame_t to use an existing SDL texture.
 * @param frame Target frame to initialize.
 * @param ren SDL renderer used for texture operations and present.
 * @param tex Existing streaming SDL_Texture (caller retains ownership).
 * @param w Texture width in pixels.
 * @param h Texture height in pixels.
 * @param fmt Pixel format for the frame (e.g., PIX_FMT_RGBA8888).
 * @return true on success, false on failure.
 *
 * The frame's function pointers (lock/unlock/set_pixel/draw_line) will be set.
 * The context is stored in frame->user as pix_frame_sdl_ctx_t and will not
 * destroy the texture on pix_frame_destroy_sdl (owns_texture = false).
 */
bool pix_frame_init_sdl(pix_frame_t *frame, SDL_Renderer *ren, SDL_Texture *tex,
                        int w, int h, pix_format_t fmt);

/**
 * @brief Initialize a pix_frame_t and create a streaming SDL texture.
 * @param frame Target frame to initialize.
 * @param ren SDL renderer used for texture creation and present.
 * @param w Texture width in pixels.
 * @param h Texture height in pixels.
 * @param fmt Pixel format for the frame (e.g., PIX_FMT_RGBA8888).
 * @return true on success, false on failure.
 *
 * This function creates and owns a streaming SDL_Texture. On
 * pix_frame_destroy_sdl the texture will be destroyed (owns_texture = true).
 */
bool pix_frame_init_sdl_auto(pix_frame_t *frame, SDL_Renderer *ren, int w,
                             int h, pix_format_t fmt);

/**
 * @brief Resize the underlying SDL texture (recreating as needed).
 * @param frame Frame to resize.
 * @param w New width in pixels.
 * @param h New height in pixels.
 * @return true on success, false on failure.
 */
bool pix_frame_resize_sdl(pix_frame_t *frame, int w, int h);

/**
 * @brief Rebind the frame to a different existing SDL texture.
 * @param frame Frame to rebind.
 * @param tex New existing texture (caller retains ownership).
 * @param w Texture width in pixels.
 * @param h Texture height in pixels.
 * @return true on success, false on failure.
 */
bool pix_frame_rebind_sdl(pix_frame_t *frame, SDL_Texture *tex, int w, int h);

/**
 * @brief Destroy the SDL backend context and any owned resources.
 * @param frame Frame previously initialized with pix_frame_init_sdl[_auto].
 *
 * Frees the line command queue; if owns_texture is true, also destroys the SDL
 * texture. Does not free the pix_frame_t itself.
 */
void pix_frame_destroy_sdl(pix_frame_t *frame);

#ifdef __cplusplus
}
#endif
