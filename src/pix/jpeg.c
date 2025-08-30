#include "frame_internal.h"
#include <pix/image.h>
#include <pix/pix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vg/vg.h> /* for VG_MALLOC/VG_FREE */

#include "tjpgd.h"

/* Streaming source (either user callback or in-memory) */
typedef struct {
  pix_jpeg_read_cb read_cb; /* user callback (NULL for memory mode) */
  void *user;               /* user data for read_cb */
  const uint8_t *mem;       /* memory buffer (memory mode) */
  size_t mem_size;          /* total size */
  size_t mem_pos;           /* current position */
} jpeg_stream_t;

// Forward declarations of stub lock/unlock
static bool jpeg_frame_lock(pix_frame_t *f) {
  (void)f;
  return true;
}

static void jpeg_frame_unlock(pix_frame_t *f) { (void)f; }

// Tiny helper frame creation for RGB24 software frame (no backend, always
// locked).
static void simple_frame_destroy(pix_frame_t *frame) {
  if (!frame)
    return;
  if (frame->pixels) {
    VG_FREE(frame->pixels);
  }
  VG_FREE(frame);
}

static pix_frame_t *alloc_frame(uint16_t w, uint16_t h, pix_format_t fmt) {
  size_t bpp;
  switch (fmt) {
  case PIX_FMT_RGB24:
    bpp = 3;
    break;
  case PIX_FMT_GRAY8:
    bpp = 1;
    break;
  case PIX_FMT_RGB565:
    bpp = 2;
    break;
  default:
    return NULL;
  }
  size_t stride = (size_t)w * bpp;
  size_t bytes = stride * h;
  uint8_t *pixels = (uint8_t *)VG_MALLOC(bytes);
  if (!pixels)
    return NULL;
  pix_frame_t *f = (pix_frame_t *)VG_MALLOC(sizeof(pix_frame_t));
  if (!f) {
    VG_FREE(pixels);
    return NULL;
  }
  memset(f, 0, sizeof(*f));
  f->pixels = pixels;
  f->size = (pix_size_t){w, h};
  f->stride = stride;
  f->format = fmt;
  f->set_pixel = pix_frame_set_pixel; // generic dispatch uses format
  f->get_pixel = pix_frame_get_pixel; // new function pointer
  f->copy = pix_frame_copy;           // copy/blit entry
  f->draw_line = pix_frame_draw_line;
  // lock/unlock no-op (already CPU accessible)
  f->lock = jpeg_frame_lock;
  f->unlock = jpeg_frame_unlock;
  f->destroy = simple_frame_destroy;
  return f;
}

/* Caller destroys returned frame with frame->destroy(frame) (frees struct). */

typedef struct {
  jpeg_stream_t stream;
  uint16_t width, height;
  pix_format_t format;
  pix_frame_t *frame; /* destination frame */
} jpeg_ctx_t;

// Input callback: fill buffer with up to nbytes from memory source (jd->device
// -> jpeg_ctx_t).
static size_t jpeg_infunc(JDEC *jd, uint8_t *buf, size_t nbyte) {
  jpeg_ctx_t *ctx = (jpeg_ctx_t *)jd->device;
  jpeg_stream_t *s = &ctx->stream;
  if (s->read_cb) {
    if (!buf && nbyte) {
      /* Decoder is requesting to skip bytes; read and discard safely. */
      uint8_t tmp[256];
      size_t remain = nbyte;
      while (remain) {
        size_t chunk = remain < sizeof(tmp) ? remain : sizeof(tmp);
        size_t got = s->read_cb(tmp, chunk, s->user);
        if (!got)
          break; /* EOF or error */
        remain -= got;
        if (got < chunk)
          break; /* short read */
      }
      return nbyte - remain; /* actual bytes skipped */
    }
    return s->read_cb(buf, nbyte, s->user);
  }
  /* memory mode */
  size_t remain = (s->mem_pos < s->mem_size) ? (s->mem_size - s->mem_pos) : 0;
  if (nbyte > remain)
    nbyte = remain;
  if (buf && nbyte) {
    memcpy(buf, s->mem + s->mem_pos, nbyte);
  }
  s->mem_pos += nbyte;
  return nbyte;
}

// Output callback: copy RGB888 rectangle into frame (jd->device -> jpeg_ctx_t)
static int jpeg_outfunc(JDEC *jd, void *bitmap, JRECT *rect) {
  jpeg_ctx_t *ctx = (jpeg_ctx_t *)jd->device;
  if (!bitmap || !rect || !ctx->frame)
    return 0;
  uint16_t w = (uint16_t)(rect->right - rect->left + 1);
  uint16_t h = (uint16_t)(rect->bottom - rect->top + 1);
  uint8_t *src = (uint8_t *)bitmap; /* RGB888 block */

  pix_frame_t *f = ctx->frame;
  size_t dst_stride = f->stride;
  switch (ctx->format) {
  case PIX_FMT_RGB24: {
    for (uint16_t row = 0; row < h; ++row) {
      uint8_t *dst = (uint8_t *)f->pixels +
                     ((size_t)(rect->top + row) * dst_stride) +
                     ((size_t)rect->left * 3u);
      memcpy(dst, src + (size_t)row * w * 3u, (size_t)w * 3u);
    }
    break;
  }
  case PIX_FMT_GRAY8: {
    for (uint16_t row = 0; row < h; ++row) {
      uint8_t *dst = (uint8_t *)f->pixels +
                     ((size_t)(rect->top + row) * dst_stride) +
                     (size_t)rect->left;
      uint8_t *src_row = src + (size_t)row * w * 3u;
      for (uint16_t x = 0; x < w; ++x) {
        uint8_t r = src_row[x * 3 + 0], g = src_row[x * 3 + 1],
                b = src_row[x * 3 + 2];
        dst[x] = (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
      }
    }
    break;
  }
  case PIX_FMT_RGB565: {
    for (uint16_t row = 0; row < h; ++row) {
      uint16_t *dst = (uint16_t *)((uint8_t *)f->pixels +
                                   ((size_t)(rect->top + row) * dst_stride)) +
                      rect->left;
      uint8_t *src_row = src + (size_t)row * w * 3u;
      for (uint16_t x = 0; x < w; ++x) {
        uint8_t r = src_row[x * 3 + 0];
        uint8_t g = src_row[x * 3 + 1];
        uint8_t b = src_row[x * 3 + 2];
        dst[x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
      }
    }
    break;
  }
  default:
    return 0;
  }
  return 1; /* continue */
}

/* No longer needed: conversion now happens per block in jpeg_outfunc. */

static pix_frame_t *pix_frame_init_jpeg_common(jpeg_stream_t *stream,
                                               pix_format_t format) {
  if (!stream)
    return NULL;
  if (!(format == PIX_FMT_RGB24 || format == PIX_FMT_GRAY8 ||
        format == PIX_FMT_RGB565))
    return NULL;
  jpeg_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.stream = *stream;
  ctx.format = format;
  size_t pool_size = 32 * 1024; /* work buffer */
  void *pool = VG_MALLOC(pool_size);
  if (!pool)
    return NULL;
  JDEC jd;
  JRESULT jr = jd_prepare(&jd, jpeg_infunc, pool, pool_size, &ctx);
  if (jr != JDR_OK) {
    fprintf(stderr, "jd_prepare failed (%d)\n", jr);
    VG_FREE(pool);
    return NULL;
  }
  ctx.width = jd.width;
  ctx.height = jd.height;
  ctx.frame = alloc_frame(jd.width, jd.height, format);
  if (!ctx.frame) {
    fprintf(stderr, "alloc_frame failed (%ux%u)\n", jd.width, jd.height);
    VG_FREE(pool);
    return NULL;
  }
  jr = jd_decomp(&jd, jpeg_outfunc, 0); /* scale=0 full size */
  if (jr != JDR_OK) {
    fprintf(stderr, "jd_decomp failed (%d)\n", jr);
    ctx.frame->destroy(ctx.frame);
    VG_FREE(pool);
    return NULL;
  }
  VG_FREE(pool);
  return ctx.frame;
}

pix_frame_t *pix_frame_init_jpeg(const void *data, size_t size,
                                 pix_format_t format) {
  if (!data || size == 0)
    return NULL;
  jpeg_stream_t s;
  memset(&s, 0, sizeof(s));
  s.mem = (const uint8_t *)data;
  s.mem_size = size;
  return pix_frame_init_jpeg_common(&s, format);
}

pix_frame_t *pix_frame_init_jpeg_stream(pix_jpeg_read_cb read_cb, void *user,
                                        pix_format_t format) {
  if (!read_cb)
    return NULL;
  jpeg_stream_t s;
  memset(&s, 0, sizeof(s));
  s.read_cb = read_cb;
  s.user = user;
  return pix_frame_init_jpeg_common(&s, format);
}

/* Convenience inline (header not provided) could be added later for default
 * RGB24. */
