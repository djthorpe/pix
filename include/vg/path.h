#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t vg_point_t;

typedef struct vg_path_t {
  vg_point_t *points;
  size_t size;
  size_t capacity;
  struct vg_path_t *next;
} vg_path_t;

/* Initialize a path with an initial point capacity (>0 required). */
vg_path_t vg_path_init(size_t capacity); /* capacity must be > 0 */
void vg_path_finish(vg_path_t *path);
bool vg_path_append(vg_path_t *path, vg_point_t point);
