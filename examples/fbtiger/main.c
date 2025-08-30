#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <pix/fb.h>
#include <pix/pix.h>
#include <vg/vg.h>

#include "../sdltiger/canvas.h" // reuse tiger canvas + transform helpers
#include "../sdltiger/tiger.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) {
  (void)sig;
  g_stop = 1;
}

int main(int argc, char **argv) {
  const char *fb_path = "/dev/fb0";
  if (argc > 1)
    fb_path = argv[1];

#ifndef PIX_ENABLE_FB
  fprintf(stderr, "Framebuffer backend not built (PIX_ENABLE_FB missing)\n");
  return 1;
#endif

  pix_frame_t *frame = pixfb_frame_init(fb_path);
  if (!frame) {
    fprintf(stderr, "Failed to init framebuffer %s\n", fb_path);
    return 1;
  }
  printf("fbtiger: opened %s (%ux%u stride=%zu fmt=%d) - Ctrl+C to exit\n",
         fb_path, (unsigned)frame->size.w, (unsigned)frame->size.h,
         frame->stride, frame->format);
  fflush(stdout);

  // Build tiger canvas (idempotent).
  tiger_build_canvas(NULL, NULL);

  // Fit artwork to current framebuffer size once.
  int win_w = frame->size.w;
  int win_h = frame->size.h;
  update_transform(win_w, win_h);

  // Install SIGINT handler for clean exit.
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_sigint;
  sigaction(SIGINT, &sa, NULL);

  // Simple animation: gentle auto-zoom in/out + slow rotation.
  float t = 0.f;
  const float dt = 1.f / 60.f; // ~60Hz logical update
  struct timespec last;
  clock_gettime(CLOCK_MONOTONIC, &last);
  uint32_t clear = 0xFFFFFFFFu; // white

  while (!g_stop) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (double)(now.tv_sec - last.tv_sec) +
                     (double)(now.tv_nsec - last.tv_nsec) / 1e9;
    if (elapsed < dt) {
      // Sleep remaining time (coarse)
      struct timespec req = {0, (long)((dt - elapsed) * 1e9)};
      if (req.tv_nsec > 0)
        nanosleep(&req, NULL);
      continue; // accumulate
    }
    last = now;
    t += (float)elapsed;

    // Animate scale (ping-pong between 0.75x and 1.25x) & rotation.
    g_user_scale = 1.0f + 0.25f * sinf(t * 0.5f);
    g_user_rotate = 0.2f * sinf(t * 0.25f); // slow gentle sway
    update_transform(win_w, win_h);

    if (!frame->lock(frame))
      break;
    pix_frame_clear(frame, clear);
    vg_canvas_render(&g_canvas, frame);
    frame->unlock(frame); // mmap direct; still keep for API symmetry
  }

  free_tiger_shapes();
  if (frame && frame->destroy)
    frame->destroy(frame);
  return 0;
}
