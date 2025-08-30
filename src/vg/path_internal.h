#pragma once
#include <vg/vg.h>

/* Internal path initializer (capacity must be >0). */
vg_path_t vg_path_init(size_t capacity);
void vg_path_finish(vg_path_t *path); /* internal */
