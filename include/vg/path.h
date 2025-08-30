/**
 * @file vg/path.h
 * @brief Lightweight dynamic path container used by vector shapes.
 *
 * A vg_path_t is a growable sequence of points broken into a linked list of
 * fixed-capacity segments. The first node is embedded directly inside the
 * owning shape for cache locality; additional nodes are heap allocated only
 * when the point count exceeds the current segment capacity. This keeps small
 * paths allocation‑free while still allowing arbitrarily large outlines.
 *
 * End users normally do not create or destroy paths directly; paths are
 * managed by vg_shape_t. Use the shape helper functions (see shape.h) to clear
 * or reserve space. This header exposes a minimal builder utility to append
 * points when constructing geometry procedurally.
 */
#pragma once
#include <pix/pix.h> /* pix_point_t */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @struct vg_path_t
 * @brief A segmented dynamic array of points.
 *
 * @var vg_path_t::points   Pointer to contiguous point storage for this
 * segment.
 * @var vg_path_t::size     Number of points currently stored in this segment.
 * @var vg_path_t::capacity Maximum number of points this segment can hold
 * before chaining.
 * @var vg_path_t::next     Next segment in the chain or NULL if this is the
 * last.
 */
typedef struct vg_path_t {
  pix_point_t *points;    /**< Contiguous points for this segment. */
  size_t size;            /**< Points used in this segment. */
  size_t capacity;        /**< Capacity of this segment. */
  struct vg_path_t *next; /**< Next segment in chain (NULL if end). */
} vg_path_t;

/**
 * @ingroup vg
 * @brief Append one or more points to a path (variadic builder API).
 *
 * The function accepts a NULL‑terminated list of point pointers. Each pointed
 * value is copied into the path in order. When the current segment is full a
 * new segment is allocated with doubled capacity and linked. On allocation
 * failure the function stops early but returns true for points that were
 * successfully appended before the failure (best effort, no rollback).
 *
 * Typical usage:
 * @code
 *   pix_point_t a = {0,0}, b = {10,0}, c = {10,10}, d = {0,10};
 *   vg_path_append(path, &a, &b, &c, &d, &a, NULL); // closed rectangle
 * @endcode
 *
 * For convenience you may pass addresses of compound literals:
 * @code
 *   vg_path_append(path, &(pix_point_t){x,y}, &(pix_point_t){x+5,y}, NULL);
 * @endcode
 *
 * @param path  Target path instance (must be valid).
 * @param first Pointer to the first point (must not be NULL); pass NULL to do
 * nothing.
 * @param ...   Additional point pointers terminated by a NULL sentinel.
 * @return true if at least one point pointer was processed successfully, false
 * on invalid args.
 */
bool vg_path_append(vg_path_t *path, const pix_point_t *first, ...);

/**
 * @brief Start a new empty subpath (segment) in an existing path.
 *
 * A new segment with at least @p reserve capacity (clamped to 4) is
 * allocated and appended to the end of the segment chain. Subsequent
 * calls to vg_path_append will append points to this new segment until
 * it fills (at which point normal doubling allocation applies). This
 * allows callers (such as code generators) to represent multiple SVG
 * subpaths inside a single vg_path_t without introducing implicit
 * connecting edges between the final point of one subpath and the
 * starting point of the next. Returns false on allocation failure.
 *
 * @param path    Target path (must be non-NULL).
 * @param reserve Desired initial point capacity for the new segment.
 * @return true on success, false on allocation failure or invalid args.
 */
bool vg_path_break(vg_path_t *path, size_t reserve);
