#include <vg/path.h>
#include <vg/vg.h>

vg_path_t vg_path_init(size_t capacity) {
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

size_t vg_path_count(const vg_path_t *path) {
  size_t count = 0;
  while (path) {
    count += path->size;
    path = path->next;
  }
  return count;
}

bool vg_path_append(vg_path_t *path, vg_point_t point) {
  if (path->size == path->capacity) {
    vg_path_t *next = VG_MALLOC(sizeof(vg_path_t));
    if (!next)
      return false;
    next->points = VG_MALLOC(path->capacity * sizeof(vg_point_t));
    if (!next->points) {
      VG_FREE(next);
      return false;
    }
    next->size = 0;
    next->capacity = path->capacity;
    next->next = NULL;
    path->next = next;
    return vg_path_append(next, point);
  }
  path->points[path->size++] = point;
  return true;
}
