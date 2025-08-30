/**
 * @file vg/canvas.h
 * @brief Lightweight fixed‑capacity shape list with optional pooled storage.
 *
 * A canvas holds up to `capacity` shape pointers for a single render pass. It
 * also tries to allocate one contiguous block of `vg_shape_t` structs (the
 * pool) so appends avoid per‑shape malloc/free. If the pool allocation fails
 * the implementation transparently falls back to allocating each shape on the
 * heap via vg_shape_create.
 *
 * Pool semantics:
 *  - Shapes returned by vg_canvas_append usually live inside the pool.
 *  - They are lazily default‑initialized (path allocated at first append, not
 *    for the entire unused pool up front).
 *  - Do NOT call vg_shape_destroy on pooled shapes. Their paths are released
 *    and the structs recycled by vg_canvas_clear / vg_canvas_destroy.
 *  - After vg_canvas_clear previous pooled shape pointers remain valid as
 *    memory locations but their path content has been finished; rebuild the
 *    geometry before reusing.
 *
 * External shapes: You may manually store externally allocated shapes in the
 * pointer array (within capacity). The canvas will destroy any shapes that do
 * not lie within the pool range when cleared/destroyed.
 *
 * Characteristics:
 *  - Fixed pointer capacity (no growth). Append returns NULL when full.
 *  - O(1) append, O(n) render.
 *  - Not thread‑safe.
 *  - Per shape: fill drawn before stroke in insertion order.
 */
#pragma once
#include "fill.h"
#include "shape.h"

/** Canvas container */
typedef struct vg_canvas_t {
  struct vg_shape_t **shapes; /**< Pointer array (size<=capacity). */
  size_t size;                /**< Number of valid shape pointers. */
  size_t capacity;            /**< Maximum number of pointers storable. */
  /* Internal pool (optional). If non-NULL, provides contiguous storage for
    up to pool_capacity shapes to reduce per-shape heap allocations. Shapes
    returned by vg_canvas_append come from this pool. */
  struct vg_shape_t *pool; /**< Contiguous shape storage. */
  size_t pool_capacity;    /**< Number of shapes in pool. */
  size_t pool_used;        /**< Count of shapes currently initialized. */
} vg_canvas_t;

/**
 * Create a canvas with space for up to 'capacity' shape pointers.
 * If allocation fails, the returned canvas has capacity==0.
 */
vg_canvas_t vg_canvas_init(size_t capacity);

/**
 * Append a new shape (from the pool if available) and return it, or NULL if
 * capacity reached / allocation failed. Do not call vg_shape_destroy on the
 * result; clearing/destroying the canvas handles lifetime.
 */
struct vg_shape_t *vg_canvas_append(vg_canvas_t *canvas);

/**
 * Destroy and free all shapes currently stored, then reset size to zero.
 * The pointer array itself is retained for reuse.
 */
void vg_canvas_clear(vg_canvas_t *canvas);

/** Free all owned shapes, their paths, then the pointer array, and zero struct.
 */
void vg_canvas_destroy(vg_canvas_t *canvas);

/** Render all shapes (in insertion order) into the target frame. */
void vg_canvas_render(const vg_canvas_t *canvas, struct pix_frame_t *frame);
