/* Linux framebuffer backend implementation */
#include "frame_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <pix/pix.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/fb.h>
#define PIX_ENABLE_FB 1
#endif

#ifdef PIX_ENABLE_FB

typedef struct pix_fb_ctx_t {
  int fd;
  size_t map_length;
} pix_fb_ctx_t;

static bool fb_lock(struct pix_frame_t *f) { return f && f->pixels != NULL; }
static void fb_unlock(struct pix_frame_t *f) { (void)f; }
static void fb_destroy(struct pix_frame_t *f) {
  if (!f)
    return;
  pix_fb_ctx_t *ctx = (pix_fb_ctx_t *)f->user;
  if (f->pixels && ctx && ctx->map_length)
    munmap(f->pixels, ctx->map_length);
  if (ctx && ctx->fd >= 0)
    close(ctx->fd);
  free(ctx);
  free(f);
}

pix_frame_t *pixfb_frame_init(const char *path) {
  if (!path)
    return NULL;
  int fd = open(path, O_RDWR);
  if (fd < 0)
    return NULL;
  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;
  if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0 ||
      ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
    close(fd);
    return NULL;
  }
  pix_format_t fmt = PIX_FMT_UNKNOWN;
  if (vinfo.bits_per_pixel == 32 && vinfo.red.offset == 16 &&
      vinfo.green.offset == 8 && vinfo.blue.offset == 0) {
    fmt = PIX_FMT_RGBA32; /* assume ARGB/XRGB little-endian */
  } else if (vinfo.bits_per_pixel == 24 && vinfo.red.offset == 16 &&
             vinfo.green.offset == 8 && vinfo.blue.offset == 0) {
    fmt = PIX_FMT_RGB24;
  } else if (vinfo.bits_per_pixel == 16 && vinfo.red.length == 5 &&
             vinfo.green.length == 6 && vinfo.blue.length == 5) {
    fmt = PIX_FMT_RGB565;
  } else if (vinfo.bits_per_pixel == 8) {
    fmt = PIX_FMT_GRAY8; /* optimistic */
  } else {
    close(fd);
    return NULL;
  }
  size_t bytespp = 0;
  switch (fmt) {
  case PIX_FMT_RGB24:
    bytespp = 3;
    break;
  case PIX_FMT_RGBA32:
    bytespp = 4;
    break;
  case PIX_FMT_GRAY8:
    bytespp = 1;
    break;
  case PIX_FMT_RGB565:
    bytespp = 2;
    break;
  default:
    close(fd);
    return NULL;
  }
  size_t map_len = (size_t)finfo.line_length * (size_t)vinfo.yres_virtual;
  void *mem = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    close(fd);
    return NULL;
  }
  pix_fb_ctx_t *ctx = (pix_fb_ctx_t *)calloc(1, sizeof(pix_fb_ctx_t));
  if (!ctx) {
    munmap(mem, map_len);
    close(fd);
    return NULL;
  }
  pix_frame_t *f = (pix_frame_t *)calloc(1, sizeof(pix_frame_t));
  if (!f) {
    munmap(mem, map_len);
    close(fd);
    free(ctx);
    return NULL;
  }
  ctx->fd = fd;
  ctx->map_length = map_len;
  f->pixels = mem;
  f->size.w = (uint16_t)vinfo.xres;
  f->size.h = (uint16_t)vinfo.yres;
  f->stride = (size_t)finfo.line_length;
  f->format = fmt;
  f->user = ctx;
  f->destroy = fb_destroy;
  f->lock = fb_lock;
  f->unlock = fb_unlock;
  f->set_pixel = pix_frame_set_pixel;
  f->get_pixel = pix_frame_get_pixel;
  f->draw_line = pix_frame_draw_line;
  f->copy = pix_frame_copy;
  return f;
}

#else /* !PIX_ENABLE_FB */
/* Stub for non-Linux builds: symbol intentionally omitted so header not exposed
 */
#endif
