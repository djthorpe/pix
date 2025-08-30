#include "path_internal.h"
#include <assert.h>
#include <stdarg.h>
#include <vg/vg.h>

vg_path_t vg_path_init(size_t capacity) {
  assert(capacity > 0 && "vg_path_init requires capacity > 0");
  vg_path_t path;
  path.points = VG_MALLOC(capacity * sizeof(pix_point_t));
  path.size = 0;
  path.capacity = capacity;
  path.next = NULL;
  return path;
}

void vg_path_finish(vg_path_t *path) {
  if (!path)
    return;
  vg_path_t *child = path->next;
  while (child) {
    vg_path_t *next = child->next;
    if (child->points)
      VG_FREE(child->points);
    VG_FREE(child);
    child = next;
  }
  VG_FREE(path->points);
  path->points = NULL;
  path->next = NULL;
  path->size = 0;
  path->capacity = 0;
}

static vg_path_t *append_point_seg(vg_path_t *seg, pix_point_t pt) {
  if (!seg)
    return NULL;
  if (seg->size == seg->capacity) {
    vg_path_t *n = (vg_path_t *)VG_MALLOC(sizeof(vg_path_t));
    if (!n)
      return seg;
    /* Double capacity for each new segment */
    n->capacity = seg->capacity << 1;
    n->points = VG_MALLOC(sizeof(pix_point_t) * n->capacity);
    if (!n->points) {
      VG_FREE(n);
      return seg;
    }
    n->size = 0;
    n->next = NULL;
    seg->next = n;
    seg = n;
  }
  seg->points[seg->size++] = pt;
  return seg;
}

bool vg_path_append(vg_path_t *path, const pix_point_t *first, ...) {
  if (!path || !first)
    return false;
  const pix_point_t *pt = first;
  va_list ap;
  va_start(ap, first);
  while (pt) {
    // Find last segment (so segment breaks inserted via vg_path_break are
    // honored)
    vg_path_t *tail = path;
    while (tail->next)
      tail = tail->next;
    // Append (may allocate new grown segment if tail full)
    if (tail->size == tail->capacity) {
      vg_path_t *n = (vg_path_t *)VG_MALLOC(sizeof(vg_path_t));
      if (!n) {
        // Allocation failure: stop early
        break;
      }
      n->capacity = tail->capacity << 1;
      n->points = VG_MALLOC(sizeof(pix_point_t) * n->capacity);
      if (!n->points) {
        VG_FREE(n);
        break;
      }
      n->size = 0;
      n->next = NULL;
      tail->next = n;
      tail = n;
    }
    tail->points[tail->size++] = *pt;
    pt = va_arg(ap, const pix_point_t *);
  }
  va_end(ap);
  return true;
}

bool vg_path_break(vg_path_t *path, size_t reserve) {
  if (!path)
    return false;
  if (reserve < 4)
    reserve = 4;
  // Walk to last segment
  vg_path_t *seg = path;
  while (seg->next)
    seg = seg->next;
  // Allocate new segment
  vg_path_t *n = (vg_path_t *)VG_MALLOC(sizeof(vg_path_t));
  if (!n)
    return false;
  n->capacity = reserve;
  n->points = VG_MALLOC(sizeof(pix_point_t) * n->capacity);
  if (!n->points) {
    VG_FREE(n);
    return false;
  }
  n->size = 0;
  n->next = NULL;
  seg->next = n;
  return true;
}
