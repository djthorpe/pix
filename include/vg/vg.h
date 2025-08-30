/**
 * @defgroup vg Vector Graphics
 * @brief Paths, shapes, fills, primitives, canvas, transforms.
 *
 * Software vector drawing built atop the core pixel frame module.
 */
#pragma once

#ifndef VG_MALLOC
#define VG_MALLOC malloc
#endif
#ifndef VG_FREE
#define VG_FREE free
#endif
#ifndef VG_REALLOC
#define VG_REALLOC realloc
#endif

#include <pix/pix.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* Module headers (each documents its own API) */
#include "canvas.h"     /**< @ingroup vg */
#include "font.h"       /**< @ingroup vg */
#include "path.h"       /**< @ingroup vg */
#include "primitives.h" /**< @ingroup vg */
#include "shape.h"      /**< @ingroup vg */
#include "transform.h"  /**< @ingroup vg */
