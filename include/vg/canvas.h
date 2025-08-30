/**
 * @file vg/canvas.h
 * @brief Minimal growable canvas (list) of vector/image shapes for rendering.
 *
 * A canvas stores pointers to shapes (vector paths or images) in insertion
 * order. When the initial capacity is exceeded an overflow chunk is
 * allocated and linked, avoiding large reallocations. All shapes appended
 * are owned by the canvas and destroyed when the canvas is destroyed.
 */
#pragma once
#include "shape.h"

/**
 * @struct vg_canvas_t
 * @brief Head (and optional chained chunks) of a shape list.
 *
 * Users treat this as an opaque aggregate; fields are public only for
 * lightweight access. The list is singly linked through @ref next.
 */
typedef struct vg_canvas_t {
  struct vg_shape_t **shapes; /**< Pointer array of length @ref capacity. */
  size_t size;                /**< Number of valid entries in @ref shapes. */
  size_t capacity;            /**< Allocated pointer capacity for this chunk. */
  struct vg_canvas_t *next;   /**< Overflow chunk (NULL if none). */
} vg_canvas_t;

/**
 * @ingroup vg
 * @brief Initialize a canvas with an initial pointer capacity.
 *
 * @param capacity Maximum number of shapes storable in the head chunk before
 *                 an overflow chunk is allocated (0 creates an inert canvas).
 * @return Initialized canvas value. Test returned .capacity to detect OOM.
 */
vg_canvas_t vg_canvas_init(size_t capacity);

/**
 * @ingroup vg
 * @brief Append a new (empty) shape to the canvas.
 *
 * Allocates a heap shape, growing by adding a new chunk if the current tail
 * is full.
 *
 * @param canvas Canvas to append to.
 * @return Newly created shape pointer, or NULL on allocation failure.
 */
struct vg_shape_t *vg_canvas_append(vg_canvas_t *canvas);

/**
 * @ingroup vg
 * @brief Destroy the canvas and all shapes.
 *
 * Frees every shape and all overflow chunks, then releases the head array and
 * zeros the structure (safe to re-init with vg_canvas_init afterwards).
 * @param canvas Canvas to destroy (may be NULL).
 */
void vg_canvas_destroy(vg_canvas_t *canvas);

/**
 * @ingroup vg
 * @brief Render every shape (fill then stroke) in insertion order.
 *
 * Each shape's optional transform is applied, then fill (if enabled) and
 * stroke (if enabled) are rasterized into @p frame. Image shapes invoke the
 * frame copy pipeline. The function performs best when shapes are batched
 * by similar state (not yet state-sorted internally).
 *
 * @param canvas Canvas to draw (NULL ignored).
 * @param frame Target frame (NULL ignored; must be locked if backend requires).
 */
void vg_canvas_render(const vg_canvas_t *canvas, struct pix_frame_t *frame);

/**
 * @ingroup vg
 * @brief Compute axis-aligned bounds (bounding box) of untransformed geometry.
 *
 * Current limitations: per-shape transforms and stroke expansion are
 * ignored; only raw path coordinates are considered. For image shapes, the
 * destination rectangle is included. If the canvas is empty, (0,0) / {0,0}
 * is returned.
 *
 * @param canvas Canvas to inspect (NULL => no-op).
 * @param origin Output min point (may be NULL).
 * @param size Output size (width/height) (may be NULL).
 */
void vg_canvas_bbox(const vg_canvas_t *canvas, pix_point_t *origin,
                    pix_size_t *size);
