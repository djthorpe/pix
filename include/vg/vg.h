#pragma once

#ifndef VG_MALLOC
#define VG_MALLOC malloc
#endif
#ifndef VG_FREE
#define VG_FREE free
#endif

#include <stdlib.h>
#include <vg/canvas.h>
#include <vg/fill.h>
#include <vg/path.h>
#include <vg/primitives.h>
#include <vg/shape.h>
#include <vg/transform.h>

#define VG_COLOR_NONE 0xDEADBEEFu
