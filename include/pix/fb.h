/**
 * @file pix/fb.h
 * @brief Linux framebuffer (/dev/fbN) backend creating a pix_frame_t bound to
 *        a mapped framebuffer device.
 *
 * This header is available only on Linux builds (PIX_ENABLE_FB defined). It
 * exposes a single helper that opens a framebuffer device node (e.g.
 * /dev/fb0), queries its fixed / variable screen info, mmap()s the pixel
 * memory and wraps it in a pix_frame_t. The returned frame provides lock,
 * unlock (no-op) and destroy callbacks; pixels are written directly to the
 * device memory.
 *
 * Lifetime:
 *  - Call ::pixfb_frame_init with a framebuffer device path.
 *  - Optionally call frame->lock(frame) before rendering (lock is a cheap
 *    no-op returning true; provided for API uniformity).
 *  - Render using pix / vg APIs; changes are immediately visible.
 *  - Call frame->unlock(frame) (no-op) if used.
 *  - Destroy with frame->destroy(frame) to unmap, close and free the frame.
 *
 * Threading: All calls should occur on a single thread. No synchronization
 * is performed.
 *
 * Supported pixel formats: Attempts to map common RGB formats to PIX formats:
 *  - 24bpp packed RGB -> PIX_FMT_RGB24
 *  - 32bpp X8R8G8B8 / A8R8G8B8 -> PIX_FMT_RGBA32 (treated as straight alpha)
 *  - 16bpp RGB565 -> PIX_FMT_RGB565
 *  - 8bpp grayscale (if pseudo palette / grayscale) -> PIX_FMT_GRAY8
 * If the device's layout is unsupported, the function returns NULL.
 *
 * Error handling: Returns NULL on open/ioctl/mmap failure or unsupported
 * format. On failure no resources are leaked.
 */
#pragma once

#ifdef PIX_ENABLE_FB
#include <pix/pix.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a pix_frame_t from a Linux framebuffer device.
 * @param path Filesystem path to framebuffer (e.g. "/dev/fb0").
 * @return Newly allocated pix_frame_t* on success, NULL on failure.
 */
pix_frame_t *pixfb_frame_init(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* PIX_ENABLE_FB */
