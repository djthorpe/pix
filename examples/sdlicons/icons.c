// sdlicons: render a random 4x4 grid of generated icons with zoom / pan /
// rotate. Directly uses pixsdl_frame_init + raw SDL event loop.

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <pix/frame.h>
#include <pix/pix.h>
#include <pix/sdl.h>
#include <vg/vg.h>

// Include generated icon headers (a subset keeps compile time low). Add more
// for variety.
#include <filled/icons.h>
#include <outline/icons.h>

// New generator API: each icon builder fills an array of shape pointers.
typedef bool (*icon_builder_fn)(vg_shape_t **, size_t *);

typedef struct icon_entry {
  const char *name; // function style tag (f_/o_ prefix)
  icon_builder_fn builder;
} icon_entry;

// Expanded sample (balanced filled + outline). Add/remove freely; names are
// used for on-screen labels.
// Interleaved pairs so first page shows both filled + outline variants.
static icon_entry ICONS[] = {
    {"f_accessible", vg_icon_f_accessible},
    {"o_accessible", vg_icon_o_accessible},
    {"f_alarm", vg_icon_f_alarm},
    {"o_alarm", vg_icon_o_alarm},
    {"f_bolt", vg_icon_f_bolt},
    {"o_bolt", vg_icon_o_bolt},
    {"f_camera", vg_icon_f_camera},
    {"o_camera", vg_icon_o_camera},
    {"f_calendar_event", vg_icon_f_calendar_event},
    {"o_calendar_event", vg_icon_o_calendar_event},
    {"f_circle_check", vg_icon_f_circle_check},
    {"o_circle_check", vg_icon_o_circle_check},
    {"f_heart", vg_icon_f_heart},
    {"o_heart", vg_icon_o_heart},
    {"f_home", vg_icon_f_home},
    {"o_home", vg_icon_o_home},
    {"f_info_circle", vg_icon_f_info_circle},
    {"o_info_circle", vg_icon_o_info_circle},
    {"f_message_circle", vg_icon_f_message_circle},
    {"o_message_circle", vg_icon_o_message_circle},
    {"f_phone", vg_icon_f_phone},
    {"o_phone", vg_icon_o_phone},
    {"f_photo", vg_icon_f_photo},
    {"o_photo", vg_icon_o_photo},
    {"f_settings", vg_icon_f_settings},
    {"o_settings", vg_icon_o_settings},
    {"f_star", vg_icon_f_star},
    {"o_star", vg_icon_o_star},
    {"f_sun", vg_icon_f_sun},
    {"o_sun", vg_icon_o_sun},
    {"f_user", vg_icon_f_user},
    {"o_user", vg_icon_o_user},
};
static const int ICON_COUNT = (int)(sizeof(ICONS) / sizeof(ICONS[0]));

#define GRID_COLS 4
#define GRID_ROWS 4
#define MAX_ICONS (GRID_COLS * GRID_ROWS)

typedef struct state_t {
  vg_canvas_t canvas; // owns shapes
  vg_shape_t
      *shapes[MAX_ICONS][16]; // per-grid-cell shape pointers (owned by canvas)
  size_t shape_counts[MAX_ICONS]; // number of shapes per cell
  vg_transform_t
      transforms[MAX_ICONS]; // per-shape transforms (stored externally)
  vg_shape_t *label_shapes[MAX_ICONS]; // text label shapes (owned by canvas)
  float label_widths[MAX_ICONS];
  int icon_indices[MAX_ICONS];
  int icon_offset; // starting index into ICONS for current grid page
  float zoom;      // global zoom
  float rot;       // global rotation (radians)
  float pan_x;     // global pan
  float pan_y;
  int win_w; // current window size
  int win_h;
  bool rebuild;     // need to rebuild geometry
  float icon_scale; // scale factor relative to cell size (0..1, portion of
                    // min(cell_w,cell_h))
  bool dragging;
  int drag_last_x, drag_last_y;
} state_t;

static void pick_icons(state_t *st) {
  // Deterministic sequential page: fill grid with consecutive icons starting
  // at icon_offset, wrapping around ICON_COUNT.
  for (int i = 0; i < MAX_ICONS; ++i) {
    st->icon_indices[i] = (st->icon_offset + i) % ICON_COUNT;
  }
  st->rebuild = true;
}

static void build_icon_shapes(state_t *st) {
  if (!st->rebuild)
    return;
  // Clear canvas and rebuild from selected icon indices
  // Simpler: destroy old canvas and re-init
  vg_canvas_destroy(&st->canvas);
  st->canvas = vg_canvas_init(128);
  for (int i = 0; i < MAX_ICONS; ++i) {
    st->shape_counts[i] = 0;
    icon_entry *e = &ICONS[st->icon_indices[i]];
    vg_shape_t *tmp[16] = {0};
    size_t count = 0;
    if (!e->builder(tmp, &count) || count == 0) {
      fprintf(stderr, "icon build failed: %s\n", e->name);
      continue;
    }
    if (count > 16)
      count = 16; // clamp
    bool filled = (e->name[0] == 'f');
    for (size_t si = 0; si < count; ++si) {
      vg_shape_t *src = tmp[si];
      if (!src)
        continue;
      // Keep generator output intact (no per-icon filtering).
      // Append a shape to canvas and copy path from src
      vg_shape_t *dst = vg_canvas_append(&st->canvas);
      if (!dst)
        continue;
      // Copy path points (all segments). Each vg_path_t segment represents an
      // original SVG subpath when grouping is enabled. We must preserve
      // segment boundaries (holes) by starting a new segment in the dest for
      // every source->next chain node.
      vg_path_t *sp = vg_shape_path(src);
      if (sp) {
        // Detect roundish polygons (filled or outline) and rebuild with
        // smoother circle (48 seg).
        int total_pts = 0;
        const vg_path_t *scan = sp;
        while (scan) {
          total_pts += (int)scan->size;
          scan = scan->next;
        }
        if (total_pts >= 6 && total_pts <= 96) {
          float cx = 0, cy = 0;
          scan = sp;
          int n = total_pts;
          while (scan) {
            for (size_t pi = 0; pi < scan->size; ++pi) {
              cx += scan->points[pi].x;
              cy += scan->points[pi].y;
            }
            scan = scan->next;
          }
          cx /= n;
          cy /= n;
          float rsum = 0, rmin = 1e9f, rmax = -1e9f;
          scan = sp;
          while (scan) {
            for (size_t pi = 0; pi < scan->size; ++pi) {
              float dx = scan->points[pi].x - cx;
              float dy = scan->points[pi].y - cy;
              float r = sqrtf(dx * dx + dy * dy);
              rsum += r;
              if (r < rmin)
                rmin = r;
              if (r > rmax)
                rmax = r;
            }
            scan = scan->next;
          }
          float ravg = rsum / (float)n;
          if (ravg > 0.f &&
              (rmax - rmin) / ravg < 0.06f) { // tighter circular threshold
            vg_shape_path_clear(dst, 48);
            vg_shape_init_circle(
                dst, (pix_point_t){(int16_t)lroundf(cx), (int16_t)lroundf(cy)},
                (pix_scalar_t)lroundf(ravg), 48);
            vg_shape_destroy(src);
            st->shapes[i][st->shape_counts[i]++] = dst;
            continue;
          }
        }
        // Calculate total first segment capacity for initial clear
        size_t first_cap = sp->size > 0 ? sp->size : 4;
        vg_shape_path_clear(dst, first_cap);
        vg_path_t *dp = vg_shape_path(dst);
        const vg_path_t *seg = sp;
        bool first = true;
        while (seg) {
          if (!first) {
            // Allocate a new empty segment in destination mirroring size
            vg_path_break(dp, seg->size > 0 ? seg->size : 4);
          }
          for (size_t pi = 0; pi < seg->size; ++pi) {
            vg_path_append(dp, &seg->points[pi], NULL);
          }
          first = false;
          seg = seg->next;
        }
      }
      // Style
      if (filled) {
        vg_shape_set_fill_color(dst, 0xFFFFFFFF);
        vg_shape_set_stroke_color(dst, PIX_COLOR_NONE);
        // Use RAW even-odd rule (no gap bridging) to preserve intended holes
        // in filled icons; bridging can erroneously seal small counters.
        vg_shape_set_fill_rule(dst, VG_FILL_EVEN_ODD_RAW);
      } else {
        vg_shape_set_fill_color(dst, PIX_COLOR_NONE);
        vg_shape_set_stroke_color(dst, 0xFFFFFFFF);
        // Uniformly larger outline strokes (base 3px before global scaling)
        vg_shape_set_stroke_width(dst, 3.0f * (float)SVG2PIX_UPSCALE);
        vg_shape_set_stroke_cap(dst, VG_CAP_ROUND);
        vg_shape_set_stroke_join(dst, VG_JOIN_ROUND);
      }
      st->shapes[i][st->shape_counts[i]++] = dst;
      // Free temporary src
      vg_shape_destroy(src);
    }
    // Build text label shape using vg_font (own copy appended to canvas)
    float tw = 0.f;
    vg_shape_t *label_src = vg_font_make_text_shape(
        &vg_font_tiny5x7, e->name, 0xFFFFFFFFu, 7.0f, 1.0f, &tw);
    if (label_src) {
      vg_shape_t *dst = vg_canvas_append(&st->canvas);
      if (dst) {
        // copy path
        vg_path_t *sp = vg_shape_path(label_src);
        if (sp) {
          size_t first_cap = sp->size > 0 ? sp->size : 4;
          vg_shape_path_clear(dst, first_cap);
          vg_path_t *dp = vg_shape_path(dst);
          const vg_path_t *seg = sp;
          bool first = true;
          while (seg) {
            if (!first) {
              vg_path_break(dp, seg->size > 0 ? seg->size : 4);
            }
            for (size_t pi = 0; pi < seg->size; ++pi) {
              vg_path_append(dp, &seg->points[pi], NULL);
            }
            first = false;
            seg = seg->next;
          }
        }
        vg_shape_set_fill_color(dst, 0xFFFFFFFFu);
        vg_shape_set_stroke_color(dst, PIX_COLOR_NONE);
        st->label_shapes[i] = dst;
        st->label_widths[i] = tw;
      }
      vg_shape_destroy(label_src);
    } else {
      st->label_shapes[i] = NULL;
      st->label_widths[i] = 0.f;
    }
  }
  st->rebuild = false;
}

static void update_transforms(state_t *st) {
  // Compose global view transform pieces
  vg_transform_t T_center, R, S, M_global;
  float cx = st->win_w * 0.5f + st->pan_x;
  float cy = st->win_h * 0.5f + st->pan_y;
  vg_transform_translate(&T_center, cx,
                         cy); // translation to window center (+ pan)
  vg_transform_rotate(&R, st->rot);
  vg_transform_scale(&S, st->zoom, st->zoom);
  vg_transform_t RS, RS_icon, tmp;
  vg_transform_multiply(&RS, &R, &S); // R * S (apply S then R)

  // Grid layout cell size
  float cell_w = st->win_w / (float)GRID_COLS;
  float cell_h = st->win_h / (float)GRID_ROWS;

  for (int i = 0; i < MAX_ICONS; ++i) {
    int col = i % GRID_COLS;
    int row = i / GRID_COLS;
    float gx = (col + 0.5f) * cell_w;
    float gy = (row + 0.5f) * cell_h;
    // Icon geometry was upscaled at generation (SVG2PIX_UPSCALE). Normalize
    // back to a 24x24 logical box so a 2px stroke matches original design.
    float up = (float)SVG2PIX_UPSCALE;
    vg_transform_t T_icon_center, T_icon_offset_up, S_norm, S_cell, S_norm_cell,
        T_prep, RS_prep;
    vg_transform_translate(&T_icon_center, gx, gy);
    vg_transform_translate(&T_icon_offset_up, -12.0f * up, -12.0f * up);
    vg_transform_scale(&S_norm, 1.0f / up, 1.0f / up);
    // Additional scaling to enlarge icons within cell dimensions
    float base_scale = st->icon_scale; // portion of cell span
    float cell_base = fminf(cell_w, cell_h) * base_scale;
    float scale_factor = cell_base / 24.0f; // 24x24 design space
    vg_transform_scale(&S_cell, scale_factor, scale_factor);
    vg_transform_multiply(&S_norm_cell, &S_cell, &S_norm); // S_cell * S_norm
    // Order: center * ((S_cell * S_norm) * T_icon_offset_up)
    vg_transform_multiply(&T_prep, &S_norm_cell, &T_icon_offset_up);
    vg_transform_multiply(&tmp, &T_icon_center, &T_prep); // center * prep
    vg_transform_multiply(&RS_icon, &RS, &tmp); // RS * (center * prep)
    vg_transform_multiply(&st->transforms[i], &T_center, &RS_icon); // final
    for (size_t si = 0; si < st->shape_counts[i]; ++si) {
      vg_shape_t *s = st->shapes[i][si];
      if (s)
        vg_shape_set_transform(s, &st->transforms[i]);
    }
    // Label transform: base off icon transform (already includes global
    // T_center * RS * prep)
    vg_shape_t *label = st->label_shapes[i];
    if (label) {
      float cell_base = fminf(cell_w, cell_h) * st->icon_scale;
      float scale_factor =
          cell_base / 24.0f; // icon scale (we'll use a fraction for text)
      float text_scale = scale_factor * 0.6f; // make label smaller than icon
      // Baseline position below icon
      float icon_half = 12.0f * scale_factor;
      float label_w = st->label_widths[i];
      vg_transform_t T_local_down, S_text, local, final;
      // DEBUG: push label further down a bit and color magenta for visibility
      vg_transform_translate(&T_local_down, -label_w * 0.5f, icon_half + 14.0f);
      vg_transform_scale(&S_text, text_scale, text_scale);
      // local = T * S (apply scale then translate)
      vg_transform_multiply(&local, &T_local_down, &S_text);
      // final = icon_transform * local  (apply local after icon space)
      vg_transform_multiply(&final, &st->transforms[i], &local);
      vg_shape_set_transform(label, &final);
      vg_shape_set_fill_color(label, 0xFFFF00FFu); // magenta temporary
      vg_shape_set_stroke_color(label, PIX_COLOR_NONE);
    }

    // (Removed dynamic stroke normalization so original large outline stroke
    // remains.)
  }
}

static void render(state_t *st, pix_frame_t *frame) {
  build_icon_shapes(st);
  update_transforms(st);
  if (!frame->lock(frame))
    return;
  // Clear manually (straight alpha fill) since generic clear helper isn't
  // public
  int w = frame->size.w, h = frame->size.h;
  uint32_t *row = (uint32_t *)frame->pixels;
  if (frame->format == PIX_FMT_RGBA32 && row) {
    for (int y = 0; y < h; ++y) {
      uint32_t *p = (uint32_t *)((uint8_t *)frame->pixels + y * frame->stride);
      for (int x = 0; x < w; ++x)
        p[x] = 0xFF000000; // ARGB: opaque black
    }
  }
  vg_canvas_render(&st->canvas, frame);
  frame->unlock(frame);
}

static void handle_key(state_t *st, SDL_Keysym keysym) {
  switch (keysym.sym) {
  case SDLK_SPACE:
    // Advance to next page of icons (wrap-around)
    if (ICON_COUNT > 0) {
      st->icon_offset = (st->icon_offset + MAX_ICONS) % ICON_COUNT;
      pick_icons(st);
    }
    break;
  case SDLK_LEFT:
    st->rot -= 0.1f; // rotate with left arrow
    break;
  case SDLK_RIGHT:
    st->rot += 0.1f; // rotate with right arrow
    break;
  case SDLK_KP_4: // rotate left (numeric keypad)
    st->rot -= 0.1f;
    break;
  case SDLK_KP_6: // rotate right (numeric keypad)
    st->rot += 0.1f;
    break;
  case SDLK_UP:
    st->pan_y += 40.f;
    break;
  case SDLK_DOWN:
    st->pan_y -= 40.f;
    break;
  case SDLK_q:
    st->rot -= 0.1f;
    break;
  case SDLK_e:
    st->rot += 0.1f;
    break;
  case SDLK_MINUS:
  case SDLK_KP_MINUS:
    st->zoom *= 0.9f;
    break;
  case SDLK_EQUALS:
  case SDLK_PLUS:
  case SDLK_KP_PLUS:
    st->zoom *= 1.1f;
    break;
  default:
    break;
  }
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  srand((unsigned)time(NULL));

  int width = 800, height = 800;
  pix_frame_t *frame = NULL;
  frame = pixsdl_frame_init("pix sdlicons",
                            (pix_size_t){(uint16_t)width, (uint16_t)height},
                            PIX_FMT_RGBA32);
  if (!frame) {
    fprintf(stderr, "failed to init frame\n");
    return 1;
  }
  if (!frame) {
    fprintf(stderr, "Failed to init pixsdl frame.\n");
    return 1;
  }

  state_t st = {0};
  st.canvas = vg_canvas_init(64);
  st.zoom = 1.0f;
  st.rot = 0.0f;
  st.win_w = width;
  st.win_h = height;
  st.rebuild = true;
  st.icon_scale = 0.85f; // occupy 85% of the smaller cell dimension (bigger)

  // Initial empty canvas; shapes allocated during build_icon_shapes
  pick_icons(&st);

  bool running = true;
  while (running) {
    // Event pump (custom so we can intercept keys & wheel)
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_KEYDOWN:
        handle_key(&st, e.key.keysym);
        break;
      case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            e.window.event == SDL_WINDOWEVENT_RESIZED) {
          st.win_w = e.window.data1;
          st.win_h = e.window.data2;
          // Recreate frame (simple path; optimization possible later)
          if (frame && frame->destroy)
            frame->destroy(frame), frame = NULL;
          frame = pixsdl_frame_init(
              "pix sdlicons",
              (pix_size_t){(uint16_t)st.win_w, (uint16_t)st.win_h},
              PIX_FMT_RGBA32);
          if (!frame) {
            fprintf(stderr, "Failed to recreate frame after resize.\n");
            running = false;
          }
        }
        break;
      case SDL_MOUSEWHEEL:
        if (e.wheel.y > 0)
          st.zoom *= 1.1f;
        else if (e.wheel.y < 0)
          st.zoom *= 0.9f;
        break;
      case SDL_MOUSEBUTTONDOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
          st.dragging = true;
          st.drag_last_x = e.button.x;
          st.drag_last_y = e.button.y;
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (e.button.button == SDL_BUTTON_LEFT) {
          st.dragging = false;
        }
        break;
      case SDL_MOUSEMOTION:
        if (st.dragging) {
          int dx = e.motion.x - st.drag_last_x;
          int dy = e.motion.y - st.drag_last_y;
          st.pan_x += dx;
          st.pan_y += dy;
          st.drag_last_x = e.motion.x;
          st.drag_last_y = e.motion.y;
        }
        break;
      default:
        break;
      }
    }

    render(&st, frame);
    SDL_Delay(16); // ~60 FPS
  }

  vg_canvas_destroy(&st.canvas);
  if (frame && frame->destroy)
    frame->destroy(frame), frame = NULL;
  return 0;
}
