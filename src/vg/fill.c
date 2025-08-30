// Simplified polygon fill (even-odd / non-zero) with optional small gap
// bridging. All debug / diagnostic instrumentation removed.
#include <math.h>
#include <stdlib.h>

#include "fill_internal.h"
#include <pix/pix.h>
#include <vg/vg.h>

// Bridging configuration (kept minimal, always enabled):
static const int FILL_BRIDGE_THRESH = 4; // max empty rows to simple-bridge
static const int FILL_ADAPTIVE_MAX = 8;  // max empty rows for adaptive bridge

typedef struct SimpleEdge {
  float x;     // current x at scanline center
  float dx_dy; // slope (delta x per 1 y)
  int y_end;   // exclusive end scanline
  int winding; // +1 / -1 for non-zero fill
  struct SimpleEdge *next;
} SimpleEdge;

// No debug tinting retained.

static void vg__fill_path_simple(const vg_path_t *path,
                                 const vg_transform_t *xf, pix_frame_t *frame,
                                 pix_color_t color, vg_fill_rule_t rule,
                                 int clip_x0, int clip_y0, int clip_x1,
                                 int clip_y1) {
  if (!path)
    return;
  // rule parameter used below for even-odd vs non-zero logic
  // 1. Count segments
  int est = 0;
  const vg_path_t *seg = path;
  while (seg) {
    if (seg->size > 1)
      est += (int)seg->size;
    seg = seg->next;
  }
  if (est == 0)
    return;
  // Temp edge list
  typedef struct {
    float x0, y0, x1, y1, dx_dy;
    int y_start, y_end;
    int winding;
  } TmpEdge;
  TmpEdge *tmp = (TmpEdge *)VG_MALLOC(sizeof(TmpEdge) * est);
  if (!tmp)
    return;
  int ec = 0;
  float gmin = 1e30f, gmax = -1e30f;
  seg = path;
  while (seg) {
    if (seg->size < 2) {
      seg = seg->next;
      continue;
    }
    for (size_t i = 1; i < seg->size; i++) {
      pix_point_t a = seg->points[i - 1];
      pix_point_t b = seg->points[i];
      float x0 = a.x, y0 = a.y;
      float x1 = b.x, y1 = b.y;
      if (xf) {
        float tx0 = xf->m[0][0] * x0 + xf->m[0][1] * y0 + xf->m[0][2];
        float ty0 = xf->m[1][0] * x0 + xf->m[1][1] * y0 + xf->m[1][2];
        float tx1 = xf->m[0][0] * x1 + xf->m[0][1] * y1 + xf->m[0][2];
        float ty1 = xf->m[1][0] * x1 + xf->m[1][1] * y1 + xf->m[1][2];
        x0 = tx0;
        y0 = ty0;
        x1 = tx1;
        y1 = ty1;
      }
      if (fabsf(y1 - y0) < 1e-6f)
        continue; // skip horizontal
      int winding = 1;
      if (y0 > y1) { // enforce y0<y1, adjust winding
        float tx = x0, ty = y0;
        x0 = x1;
        y0 = y1;
        x1 = tx;
        y1 = ty;
        winding = -1;
      }
      int y_start = (int)ceilf(y0 - 0.5f);
      int y_end = (int)ceilf(y1 - 0.5f);
      if (y_end <= y_start)
        continue;
      if (y_start < clip_y0)
        y_start = clip_y0;
      if (y_end > clip_y1 + 1)
        y_end = clip_y1 + 1;
      if (y_start >= y_end)
        continue;
      float dx_dy = (x1 - x0) / (y1 - y0);
      if (y0 < gmin)
        gmin = y0;
      if (y1 > gmax)
        gmax = y1;
      tmp[ec].x0 = x0;
      tmp[ec].y0 = y0;
      tmp[ec].x1 = x1;
      tmp[ec].y1 = y1;
      tmp[ec].dx_dy = dx_dy;
      tmp[ec].y_start = y_start;
      tmp[ec].y_end = y_end;
      tmp[ec].winding = winding;
      ec++;
    }
    seg = seg->next;
  }
  if (ec == 0) {
    VG_FREE(tmp);
    return;
  }
  // (debug bookkeeping removed)
  // Buckets
  int global_y0 =
      clip_y0 > (int)floorf(gmin - 0.5f) ? clip_y0 : (int)floorf(gmin - 0.5f);
  int global_y1 =
      clip_y1 < (int)ceilf(gmax - 0.5f) ? clip_y1 : (int)ceilf(gmax - 0.5f);
  if (global_y0 > global_y1) {
    VG_FREE(tmp);
    return;
  }
  int bucket_n = global_y1 - global_y0 + 2;
  SimpleEdge **buckets =
      (SimpleEdge **)VG_MALLOC(sizeof(SimpleEdge *) * bucket_n);
  SimpleEdge *pool = (SimpleEdge *)VG_MALLOC(sizeof(SimpleEdge) * ec);
  if (!buckets || !pool) {
    VG_FREE(buckets);
    VG_FREE(pool);
    VG_FREE(tmp);
    return;
  }
  for (int i = 0; i < bucket_n; i++)
    buckets[i] = NULL;
  for (int i = 0; i < ec; i++) {
    TmpEdge *te = &tmp[i];
    int b = te->y_start - global_y0;
    if (b < 0 || b >= bucket_n)
      continue;
    float x_init = te->x0 + (((float)te->y_start + 0.5f) - te->y0) * te->dx_dy;
    pool[i].x = x_init;
    pool[i].dx_dy = te->dx_dy;
    pool[i].y_end = te->y_end;
    pool[i].winding = te->winding;
    pool[i].next = buckets[b];
    buckets[b] = &pool[i];
  }
  SimpleEdge *active = NULL;
  int *row_min = NULL;
  int *row_max = NULL;
  { // allocate span bounds for bridging
    size_t rows = (size_t)(global_y1 - global_y0 + 1);
    row_min = (int *)VG_MALLOC(rows * sizeof(int));
    row_max = (int *)VG_MALLOC(rows * sizeof(int));
    if (row_min && row_max) {
      for (int i = 0; i <= global_y1 - global_y0; ++i) {
        row_min[i] = 0x7FFFFFFF;
        row_max[i] = -0x7FFFFFFF;
      }
    }
  }
  for (int y = global_y0; y <= global_y1; ++y) {
    // insert
    int bi = y - global_y0;
    if (bi >= 0 && bi < bucket_n) {
      SimpleEdge *e = buckets[bi];
      while (e) {
        SimpleEdge *n = e->next;
        e->next = active;
        active = e;
        e = n;
      }
    }
    // prune
    SimpleEdge **pp = &active;
    while (*pp) {
      if (y >= (*pp)->y_end)
        *pp = (*pp)->next;
      else
        pp = &(*pp)->next;
    }
    if (!active)
      continue;
    // sort by x (insertion)
    SimpleEdge *sorted = NULL;
    SimpleEdge *e = active;
    while (e) {
      SimpleEdge *n = e->next;
      if (!sorted || e->x < sorted->x) {
        e->next = sorted;
        sorted = e;
      } else {
        SimpleEdge *p = sorted;
        while (p->next && p->next->x <= e->x)
          p = p->next;
        e->next = p->next;
        p->next = e;
      }
      e = n;
    }
    active = sorted;
    int span_count_this_row = 0;
    // Build spans
    // Even-odd family (with or without bridging) share parity span building;
    // RAW variant skips the later bridging pass via its distinct enum.
    if (rule == VG_FILL_EVEN_ODD || rule == VG_FILL_EVEN_ODD_RAW) {
      int inside = 0;
      float prev_x = 0.f;
      for (SimpleEdge *se = active; se; se = se->next) {
        float x_curr = se->x;
        if (inside) {
          float L = prev_x, R = x_curr;
          if (R > L) {
            int sx = (int)ceilf(L);
            int ex = (int)floorf(R - 1e-6f);
            if (sx < clip_x0)
              sx = clip_x0;
            if (ex > clip_x1)
              ex = clip_x1;
            if (sx <= ex && y >= clip_y0 && y <= clip_y1) {
              if (sx < 0)
                sx = 0;
              if (ex >= (int)frame->size.w)
                ex = (int)frame->size.w - 1;
              if (y >= 0 && y < (int)frame->size.h) {
                uint32_t *row =
                    (uint32_t *)((char *)frame->pixels + y * frame->stride);
                for (int xi = sx; xi <= ex; ++xi)
                  row[xi] = color;
                span_count_this_row++;
                if (row_min && sx < row_min[y - global_y0])
                  row_min[y - global_y0] = sx;
                if (row_max && ex > row_max[y - global_y0])
                  row_max[y - global_y0] = ex;
              }
            }
          }
        }
        inside = !inside;
        prev_x = x_curr;
      }
    } else { // non-zero winding rule
      int winding = 0;
      float prev_x = 0.f;
      for (SimpleEdge *se = active; se; se = se->next) {
        float x_curr = se->x;
        int new_w = winding + se->winding;
        if ((winding != 0) && (new_w == 0)) {
          float L = prev_x, R = x_curr;
          if (R > L) {
            int sx = (int)ceilf(L);
            int ex = (int)floorf(R - 1e-6f);
            if (sx < clip_x0)
              sx = clip_x0;
            if (ex > clip_x1)
              ex = clip_x1;
            if (sx <= ex && y >= clip_y0 && y <= clip_y1) {
              if (sx < 0)
                sx = 0;
              if (ex >= (int)frame->size.w)
                ex = (int)frame->size.w - 1;
              if (y >= 0 && y < (int)frame->size.h) {
                uint32_t *row =
                    (uint32_t *)((char *)frame->pixels + y * frame->stride);
                for (int xi = sx; xi <= ex; ++xi)
                  row[xi] = color;
                span_count_this_row++;
                if (row_min && sx < row_min[y - global_y0])
                  row_min[y - global_y0] = sx;
                if (row_max && ex > row_max[y - global_y0])
                  row_max[y - global_y0] = ex;
              }
            }
          }
        }
        if (winding == 0 && new_w != 0)
          prev_x = x_curr;
        winding = new_w;
      }
    }
    // advance
    for (SimpleEdge *se = active; se; se = se->next)
      se->x += se->dx_dy;
  }
  // Bridging pass: fill short internal empty gaps by extending/interpolating
  // neighbor coverage (always enabled; previously debug-toggled).
  if (row_min && row_max && rule == VG_FILL_EVEN_ODD) {
    int rows = global_y1 - global_y0 + 1;
    int i = 0;
    while (i < rows) {
      // skip filled rows
      while (i < rows && !(row_min[i] == 0x7FFFFFFF))
        i++;
      int gap_start = i;
      while (i < rows && row_min[i] == 0x7FFFFFFF)
        i++;
      int gap_end = i - 1;
      if (gap_start <= gap_end) {
        int gap_len = gap_end - gap_start + 1;
        if (gap_start > 0 && gap_end < rows - 1) {
          int prev_min = row_min[gap_start - 1];
          int prev_max = row_max[gap_start - 1];
          int next_min = row_min[gap_end + 1];
          int next_max = row_max[gap_end + 1];
          int have_neighbors =
              (prev_min != 0x7FFFFFFF && next_min != 0x7FFFFFFF);
          // 1. Simple small-gap bridge (union of neighbor spans)
          if (have_neighbors && gap_len <= FILL_BRIDGE_THRESH) {
            int fill_min = prev_min < next_min ? prev_min : next_min;
            int fill_max = prev_max > next_max ? prev_max : next_max;
            if (fill_min <= fill_max) {
              for (int gy = gap_start; gy <= gap_end; ++gy) {
                int y = global_y0 + gy;
                int fmin = fill_min, fmax = fill_max;
                if (fmin < 0)
                  fmin = 0;
                if (fmax >= (int)frame->size.w)
                  fmax = (int)frame->size.w - 1;
                uint32_t *rowp =
                    (uint32_t *)((char *)frame->pixels + y * frame->stride);
                uint32_t draw_color = color;
                for (int x = fmin; x <= fmax; ++x)
                  rowp[x] = draw_color;
                row_min[gy] = fmin;
                row_max[gy] = fmax;
              }
            }
          }
          // 2. Adaptive interpolation for medium gaps beyond threshold
          else if (have_neighbors && gap_len <= FILL_ADAPTIVE_MAX) {
            int prev_w = prev_max - prev_min + 1;
            int next_w = next_max - next_min + 1;
            int max_allow_w = (prev_w > next_w ? prev_w : next_w) + 2; // guard
            for (int gy = gap_start; gy <= gap_end; ++gy) {
              float t = (float)(gy - gap_start + 1) / (float)(gap_len + 1);
              float fminf = prev_min + t * (next_min - prev_min);
              float fmaxf = prev_max + t * (next_max - prev_max);
              int fmin = (int)floorf(fminf + 0.5f);
              int fmax = (int)floorf(fmaxf + 0.5f);
              if (fmin > fmax) {
                // fallback to union bounds
                fmin = prev_min < next_min ? prev_min : next_min;
                fmax = prev_max > next_max ? prev_max : next_max;
              }
              if (fmax - fmin + 1 > max_allow_w) {
                // clamp width
                int center = (fmin + fmax) / 2;
                int half = max_allow_w / 2;
                fmin = center - half;
                fmax = fmin + max_allow_w - 1;
              }
              if (fmin < 0)
                fmin = 0;
              if (fmax >= (int)frame->size.w)
                fmax = (int)frame->size.w - 1;
              int y = global_y0 + gy;
              if (y >= 0 && y < (int)frame->size.h && fmin <= fmax) {
                uint32_t *rowp =
                    (uint32_t *)((char *)frame->pixels + y * frame->stride);
                uint32_t draw_color = color;
                for (int x = fmin; x <= fmax; ++x)
                  rowp[x] = draw_color;
                row_min[gy] = fmin;
                row_max[gy] = fmax;
              }
            }
          }
        }
      }
    }
  }
  VG_FREE(pool);
  VG_FREE(buckets);
  VG_FREE(tmp);
  if (row_min)
    VG_FREE(row_min);
  if (row_max)
    VG_FREE(row_max);
}

void vg_fill_path_clipped(const vg_path_t *path, const vg_transform_t *xf,
                          pix_frame_t *frame, pix_color_t color,
                          vg_fill_rule_t rule, pix_point_t clip_min,
                          pix_point_t clip_max) {
  if (!frame || !path)
    return;
  vg__fill_path_simple(path, xf, frame, color, rule, clip_min.x, clip_min.y,
                       clip_max.x, clip_max.y);
}

void vg_fill_path(const vg_path_t *path, const vg_transform_t *xf,
                  pix_frame_t *frame, pix_color_t color, vg_fill_rule_t rule) {
  vg_fill_path_clipped(
      path, xf, frame, color, rule, (pix_point_t){0, 0},
      (pix_point_t){(int16_t)frame->size.w - 1, (int16_t)frame->size.h - 1});
}
