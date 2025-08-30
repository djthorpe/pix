#include <assert.h>
#include <vg/vg.h>

vg_path_t vg_path_init(size_t capacity) {
  assert(capacity > 0 && "vg_path_init requires capacity > 0");
  vg_path_t path;
  path.points = VG_MALLOC(capacity * sizeof(vg_point_t));
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

static vg_path_t *append_point_seg(vg_path_t *seg, int32_t packed) {
  if (!seg)
    return NULL;
  if (seg->size == seg->capacity) {
    vg_path_t *n = (vg_path_t *)VG_MALLOC(sizeof(vg_path_t));
    if (!n)
      return seg;
    /* Double capacity for each new segment */
    n->capacity = seg->capacity << 1;
    n->points = VG_MALLOC(sizeof(vg_point_t) * n->capacity);
    if (!n->points) {
      VG_FREE(n);
      return seg;
    }
    n->size = 0;
    n->next = NULL;
    seg->next = n;
    seg = n;
  }
  seg->points[seg->size++] = packed;
  return seg;
}

bool vg_path_append(vg_path_t *path, vg_point_t point) {
  if (!path)
    return false;
  append_point_seg(path, point);
  return true;
}
