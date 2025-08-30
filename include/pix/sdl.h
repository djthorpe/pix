/**
 * @file pix/sdl.h
 * @brief SDL2 window / texture backend creating a pix_frame_t bound to an SDL
 *        streaming texture.
 *
 * This header is available only when the library is built with SDL2 support
 * (PIX_ENABLE_SDL defined). It exposes a single helper that hides all SDL
 * window / renderer / texture management behind the generic pix_frame_t API.
 * The returned frame supplies lock/unlock/destroy callbacks; pixels written
 * while locked are uploaded to the SDL texture on unlock and presented.
 *
 * Lifetime:
 *  - Call ::pixsdl_frame_init to create an SDL window + renderer + texture.
 *  - Render using the standard pix / vg APIs after calling frame->lock(frame).
 *  - Call frame->unlock(frame) to present.
 *  - Destroy with frame->destroy(frame); this also frees the frame object.
 *
 * Threading: All calls must occur on the same thread that owns the SDL window
 * (typically the main thread). No internal synchronization is performed.
 *
 * Supported pixel formats: PIX_FMT_RGB24 and PIX_FMT_RGBA32. If a format other
 * than PIX_FMT_RGB24 is passed, an SDL ARGB8888 texture is created and the
 * frame->format field is set to the requested value (the software renderer
 * treats it as RGBA32). Other formats (GRAY8, RGB565) are currently not backed
 * by specialized SDL texture formats.
 *
 * Error handling: Returns NULL on allocation failure, SDL initialization /
 * window / renderer / texture creation failure. In those cases SDL_Quit is
 * called if needed and no resources are leaked.
 */
#pragma once

#include <pix/pix.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an SDL-backed frame (window + streaming texture).
 * @param title UTF-8 window title (may be NULL for default).
 * @param size  Desired window / texture size in pixels (both > 0).
 * @param fmt   Requested pixel format (PIX_FMT_RGB24 or PIX_FMT_RGBA32
 *              recommended; others fallback to an RGBA32-equivalent texture).
 * @return Newly allocated pix_frame_t* on success, NULL on failure.
 *
 * The function internally calls SDL_Init(SDL_INIT_VIDEO) (once per process
 * use); on destroy (frame->destroy) it will tear down the renderer, window and
 * call SDL_Quit() if it owns the window. The frame's destroy callback frees
 * the pix_frame_t itself, so do not free() it manually.
 */
pix_frame_t *pixsdl_frame_init(const char *title, pix_size_t size,
                               pix_format_t fmt);

#ifdef __cplusplus
}
#endif
