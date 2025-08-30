#pragma once

#ifndef VG_MALLOC
#define VG_MALLOC malloc
#endif
#ifndef VG_FREE
#define VG_FREE free
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "canvas.h"
#include "fill.h"
#include "font.h"
#include "path.h"
#include "primitives.h"
#include "shape.h"
#include "transform.h"

#define VG_COLOR_NONE ((pix_color_t)0x00000000u)
