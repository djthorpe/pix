#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t vg_point_t; // packed x/y 16-bit

typedef struct vg_path_t {
  vg_point_t *points;
  size_t size;
  size_t capacity;
  struct vg_path_t *next;
} vg_path_t;

vg_path_t vg_path_init(size_t capacity);
void vg_path_finish(vg_path_t *path);
size_t vg_path_count(const vg_path_t *path);
bool vg_path_append(vg_path_t *path, vg_point_t point);
