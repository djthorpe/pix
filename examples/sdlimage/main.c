#include <SDL2/SDL.h>
#include <pix/image.h>
#include <pix/pix.h>
#include <pixsdl/pixsdl.h>
#include <stdio.h>
#include <string.h>
#include <vg/vg.h>

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
  sdl_app_t *app = sdl_app_create(win_w, win_h, PIX_FMT_RGB24, "sdlimage");
  if (!app) {
    fprintf(stderr, "SDL app create failed\n");
    if (img->finalize)
      img->finalize(img);
    free(img);
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
      img->finalize(img), free(img);
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
        } else if (k == SDLK_SPACE) {
          /* cycle to next image */
          if (image_count > 0) {
            /* Free current original image */
            if (img) {
              if (img->finalize)
                img->finalize(img);
              free(img);
              img = NULL;
            }
            /* Free flipped cache (will be regenerated lazily) */
            if (img_flipped) {
              free(img_flipped->pixels);
              free(img_flipped);
              img_flipped = NULL;
            }
            index = (index + 1) % image_count;
            if (load_image_index(images, image_count, index, &img)) {
              win_w = img->size.w;
              win_h = img->size.h;
              pix_frame_resize_sdl(frame, win_w, win_h);
              /* Rebind image shapes to new frame */
              vg_shape_set_image(shape_normal, img, (pix_point_t){0, 0},
                                 (pix_size_t){0, 0}, (pix_point_t){0, 0},
                                 PIX_BLIT_NONE);
              /* Clear flipped shape until needed */
              vg_shape_set_image(shape_flipped, NULL, (pix_point_t){0, 0},
                                 (pix_size_t){0, 0}, (pix_point_t){0, 0},
                                 PIX_BLIT_NONE);
            }
          }
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
    free(img);
  }
  if (img_flipped) {
    free(img_flipped->pixels);
    free(img_flipped);
  }
  vg_canvas_destroy(&canvas);
  sdl_app_destroy(app);
  return 0;
}
