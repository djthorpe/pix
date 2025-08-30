#include "../../src/pix/frame_internal.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <pix/image.h>
#include <pix/pix.h>
#include <pixsdl/pixsdl.h>
#include <stdio.h>
#include <string.h>
#include <vg/vg.h>

/* Forward declarations for interactive transform helpers (file scope C). */
static void img_rebuild_initial(pix_frame_t *frame, const pix_frame_t *img,
                                float *base_scale, float *user_scale,
                                float *pan_x, float *pan_y, float *angle,
                                int *xf_dirty);
static void img_rebuild_transform(pix_frame_t *frame, const pix_frame_t *img,
                                  float base_scale, float user_scale,
                                  float pan_x, float pan_y, float angle,
                                  vg_transform_t *xf, int *xf_dirty);

static const char *kFallbackImages[] = {
    "etc/car-1300x730.jpg",
    "etc/sign.jpg",
    "etc/desert.jpg",
};
static const size_t kFallbackImageCount =
    sizeof(kFallbackImages) / sizeof(kFallbackImages[0]);

/* Streaming file read context + callback */
typedef struct file_stream_ctx_t {
  FILE *fp;
} file_stream_ctx_t;

static size_t file_read_cb(void *data, size_t size, void *user_data) {
  file_stream_ctx_t *ctx = (file_stream_ctx_t *)user_data;
  if (!ctx || !ctx->fp)
    return 0;
  size_t n = fread(data, 1, size, ctx->fp);
  return n;
}

static int load_image_index(const char *const *images, size_t image_count,
                            size_t index, struct pix_frame_t **out_frame) {
  (void)image_count;
  const char *path = images[index];
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    fprintf(stderr, "Could not open image %s\n", path);
    return 0;
  }
  file_stream_ctx_t ctx = {fp};
  struct pix_frame_t *f =
      pix_frame_init_jpeg_stream(file_read_cb, &ctx, PIX_FMT_RGB24);
  fclose(fp);
  if (!f) {
    fprintf(stderr, "JPEG decode failed for %s\n", path);
    return 0;
  }
  fprintf(stderr, "Stream-loaded %s (%ux%u)\n", path, f->size.w, f->size.h);
  *out_frame = f;
  return 1;
}

int main(int argc, char **argv) {
  const char *const *images = NULL;
  size_t image_count = 0;
  if (argc > 1) {
    images = (const char *const *)&argv[1];
    image_count = (size_t)(argc - 1);
  } else {
    images = kFallbackImages;
    image_count = kFallbackImageCount;
    fprintf(stderr,
            "No image arguments supplied; using built-in list (%zu images).\n",
            image_count);
  }
  size_t index = 0;
  struct pix_frame_t *img = NULL;
  if (!load_image_index(images, image_count, index, &img))
    return 1;

  int win_w = img->size.w;
  int win_h = img->size.h;
  float img_aspect =
      (img->size.h != 0) ? (float)img->size.w / (float)img->size.h : 1.0f;
  sdl_app_t *app = sdl_app_create(win_w, win_h, PIX_FMT_RGB24, "sdlimage");
  if (!app) {
    fprintf(stderr, "SDL app create failed\n");
    if (img->finalize)
      img->finalize(img);
    VG_FREE(img);
    return 1;
  }
  pix_frame_t *frame = sdl_app_get_frame(app);
  /* Canvas with room for two image shapes (normal + flipped). */
  vg_canvas_t canvas = vg_canvas_init(2);
  vg_shape_t *shape_normal = vg_canvas_append(&canvas);
  vg_shape_t *shape_flipped = vg_canvas_append(&canvas);
  if (!shape_normal || !shape_flipped) {
    fprintf(stderr, "Canvas allocation failed\n");
    vg_canvas_destroy(&canvas);
    if (img && img->finalize)
      img->finalize(img), VG_FREE(img);
    sdl_app_destroy(app);
    return 1;
  }
  /* Configure initial image shape (full image). */
  vg_shape_set_image(shape_normal, img, (pix_point_t){0, 0}, (pix_size_t){0, 0},
                     (pix_point_t){0, 0}, PIX_BLIT_NONE);
  /* Flipped shape will be baked into a temporary frame lazily when first
   * toggled. */
  pix_frame_t *img_flipped = NULL;

  bool hflip = false;
  /* Interactive transform state */
  float pan_x = 0.0f, pan_y = 0.0f; /* in screen pixels */
  float base_scale = 1.0f;          /* fit-to-window scale (auto) */
  float user_scale = 1.0f;          /* user zoom multiplier */
  float angle = 0.0f;               /* radians */
  bool dragging = false;            /* left mouse dragging */
  int last_mx = 0, last_my = 0;
  vg_transform_t xf; /* composed transform */
  int xf_dirty = 1;  /* rebuild flag (int to pass by ptr) */

  img_rebuild_initial(frame, img, &base_scale, &user_scale, &pan_x, &pan_y,
                      &angle, &xf_dirty);
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_KEYDOWN: {
        SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_ESCAPE) {
          running = false;
        } else if (k == SDLK_f) {
          hflip = !hflip; /* toggle horizontal mirror */
        } else if (k == SDLK_LEFT) {
          angle -= (float)M_PI / 180.0f * 5.0f; /* 5 deg step */
          xf_dirty = true;
        } else if (k == SDLK_RIGHT) {
          angle += (float)M_PI / 180.0f * 5.0f;
          xf_dirty = true;
        } else if (k == SDLK_r) { /* reset */
          img_rebuild_initial(frame, img, &base_scale, &user_scale, &pan_x,
                              &pan_y, &angle, &xf_dirty);
        } else if (k == SDLK_SPACE) {
          /* cycle to next image */
          if (image_count > 0) {
            /* Free current original image */
            if (img) {
              if (img->finalize)
                img->finalize(img);
              VG_FREE(img);
              img = NULL;
            }
            /* Free flipped cache (will be regenerated lazily) */
            if (img_flipped) {
              VG_FREE(img_flipped->pixels);
              VG_FREE(img_flipped);
              img_flipped = NULL;
            }
            index = (index + 1) % image_count;
            if (load_image_index(images, image_count, index, &img)) {
              win_w = img->size.w;
              win_h = img->size.h;
              img_aspect = (img->size.h != 0)
                               ? (float)img->size.w / (float)img->size.h
                               : 1.0f;
              pix_frame_resize_sdl(frame, win_w, win_h);
              /* Rebind image shapes to new frame */
              vg_shape_set_image(shape_normal, img, (pix_point_t){0, 0},
                                 (pix_size_t){0, 0}, (pix_point_t){0, 0},
                                 PIX_BLIT_NONE);
              /* Clear flipped shape until needed */
              vg_shape_set_image(shape_flipped, NULL, (pix_point_t){0, 0},
                                 (pix_size_t){0, 0}, (pix_point_t){0, 0},
                                 PIX_BLIT_NONE);
              img_rebuild_initial(frame, img, &base_scale, &user_scale, &pan_x,
                                  &pan_y, &angle, &xf_dirty);
            }
          }
        }
        break;
      }
      case SDL_MOUSEBUTTONDOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
          dragging = true;
          last_mx = e.button.x;
          last_my = e.button.y;
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (e.button.button == SDL_BUTTON_LEFT) {
          dragging = false;
        }
        break;
      case SDL_MOUSEMOTION:
        if (dragging) {
          int mx = e.motion.x, my = e.motion.y;
          pan_x += (float)(mx - last_mx);
          pan_y += (float)(my - last_my);
          last_mx = mx;
          last_my = my;
          xf_dirty = true;
        }
        break;
      case SDL_MOUSEWHEEL: {
        /* Scroll up positive y => zoom in */
        if (e.wheel.y != 0) {
          float factor = (e.wheel.y > 0) ? 1.1f : 1.0f / 1.1f;
          /* Zoom about window center: adjust pan so visual center stays */
          float old_user = user_scale;
          user_scale *= factor;
          if (user_scale < 0.01f)
            user_scale = 0.01f;
          if (user_scale > 100.0f)
            user_scale = 100.0f;
          float sx = user_scale / old_user;
          /* Keep pan relative to center (simple: scale pan as well) */
          pan_x *= sx;
          pan_y *= sx;
          xf_dirty = true;
        }
        break;
      }
      case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            e.window.event == SDL_WINDOWEVENT_RESIZED) {
          int new_w = e.window.data1;
          int new_h = e.window.data2;
          if (new_w > 0 && new_h > 0) {
            pix_frame_resize_sdl(frame, new_w, new_h);
            /* Recompute only base_scale (retain user_scale, pan, angle). */
            if (img && img->size.w > 0 && img->size.h > 0) {
              float sx = (float)frame->size.w / (float)img->size.w;
              float sy = (float)frame->size.h / (float)img->size.h;
              float s = (sx < sy ? sx : sy);
              if (s <= 0.f)
                s = 1.f;
              base_scale = s;
              xf_dirty = 1;
            }
          }
        }
        break;
      default:
        break;
      }
    }
    if (!frame->lock(frame))
      break;
    /* Clear */
    pix_frame_clear(frame, 0xFF000000);
    if (img) {
      if (xf_dirty)
        img_rebuild_transform(frame, img, base_scale, user_scale, pan_x, pan_y,
                              angle, &xf, &xf_dirty);
      /* Attach transform to whichever shape is active */
      vg_shape_set_transform(shape_normal, &xf);
      vg_shape_set_transform(shape_flipped, &xf);
      if (hflip) {
        if (!img_flipped) {
          /* Allocate flipped frame same format/size. */
          img_flipped = (pix_frame_t *)malloc(sizeof(pix_frame_t));
          if (img_flipped) {
            /* Shallow init by copying meta then allocating pixel buffer. */
            memset(img_flipped, 0, sizeof(*img_flipped));
            img_flipped->format = img->format;
            img_flipped->size = img->size;
            size_t bpp = 3; /* PIX_FMT_RGB24 only right now */
            img_flipped->stride = img->size.w * bpp;
            img_flipped->pixels =
                malloc((size_t)img_flipped->stride * img->size.h);
            img_flipped->lock = NULL;
            img_flipped->unlock = NULL;
            img_flipped->finalize = NULL;
            if (!img_flipped->pixels) {
              free(img_flipped);
              img_flipped = NULL;
            }
            if (img_flipped) {
              for (int y = 0; y < img->size.h; ++y) {
                const uint8_t *srow =
                    (const uint8_t *)img->pixels + y * img->stride;
                uint8_t *drow =
                    (uint8_t *)img_flipped->pixels + y * img_flipped->stride;
                for (int x = 0; x < img->size.w; ++x) {
                  const uint8_t *sp = srow + x * 3;
                  uint8_t *dp = drow + (img->size.w - 1 - x) * 3;
                  dp[0] = sp[0];
                  dp[1] = sp[1];
                  dp[2] = sp[2];
                }
              }
              vg_shape_set_image(shape_flipped, img_flipped,
                                 (pix_point_t){0, 0}, (pix_size_t){0, 0},
                                 (pix_point_t){0, 0}, PIX_BLIT_NONE);
            }
          }
        }
      }
      vg_canvas_render(&canvas, frame);
    }
    frame->unlock(frame);
    SDL_Delay(10); /* simple throttle */
  }

  if (img) {
    if (img->finalize)
      img->finalize(img);
    VG_FREE(img);
  }
  if (img_flipped) {
    VG_FREE(img_flipped->pixels);
    VG_FREE(img_flipped);
  }
  vg_canvas_destroy(&canvas);
  sdl_app_destroy(app);
  return 0;
}

/* Helper implementations */
static void img_rebuild_initial(pix_frame_t *frame, const pix_frame_t *img,
                                float *base_scale, float *user_scale,
                                float *pan_x, float *pan_y, float *angle,
                                int *xf_dirty) {
  if (!img || !frame)
    return;
  float sx = (float)frame->size.w / (float)img->size.w;
  float sy = (float)frame->size.h / (float)img->size.h;
  float s = (sx < sy ? sx : sy);
  if (s <= 0.f)
    s = 1.f;
  *base_scale = s;
  if (user_scale)
    *user_scale = 1.0f; /* reset user zoom on rebuild */
  *pan_x = 0.f;
  *pan_y = 0.f;
  *angle = 0.f;
  if (xf_dirty)
    *xf_dirty = 1;
}

static void img_rebuild_transform(pix_frame_t *frame, const pix_frame_t *img,
                                  float base_scale, float user_scale,
                                  float pan_x, float pan_y, float angle,
                                  vg_transform_t *xf, int *xf_dirty) {
  if (!img || !frame || !xf)
    return;
  float wcx = frame->size.w * 0.5f;
  float wcy = frame->size.h * 0.5f;
  vg_transform_t t_center_neg, t_scale, t_rot, t_final, tmp1, tmp2;
  vg_transform_translate(&t_center_neg, -(float)img->size.w * 0.5f,
                         -(float)img->size.h * 0.5f);
  float s = base_scale * user_scale;
  vg_transform_scale(&t_scale, s, s);
  vg_transform_rotate(&t_rot, angle);
  vg_transform_translate(&t_final, wcx + pan_x, wcy + pan_y);
  vg_transform_multiply(&tmp1, &t_scale, &t_center_neg);
  vg_transform_multiply(&tmp2, &t_rot, &tmp1);
  vg_transform_multiply(xf, &t_final, &tmp2);
  if (xf_dirty)
    *xf_dirty = 0;
}
