# pix / vg (software pixels + vector graphics + SDL2)

Small C library providing:

* Pixel frame abstraction with pluggable per‑format ops (RGB24, RGBA32, GRAY8, RGB565)
* Software vector graphics: filled & stroked polylines with AA
* Optional SDL2 integration examples (window + texture blit)

## Features

* Paths & shapes (rect, circle, ellipse, rounded‑rect, triangle primitives)
* Variadic path append API: `vg_path_append(path, &p0, &p1, &p2, NULL);`
* 2D affine transforms (translate / scale / rotate / multiply)
* Fill rules: even‑odd (bridged) + even‑odd raw (no gap bridging)
* Scanline fill with horizontal coverage AA
* Strokes: variable width, caps (butt / square / round), joins (bevel / round / miter + miter limit)
* Hairline (<=1px) stroke AA (Xiaolin Wu)
* RGBA src‑over blending (straight alpha) in software
* Image shapes: blit regions from one `pix_frame_t` into another (alpha optional)
* Basic image scaling (contain) and affine‑transformed image blits
* Bounding box helpers: `vg_shape_bbox`, `vg_canvas_bbox`
* Minimal heap churn (segmented paths; canvas grows in pointer chunks)

## Layout

```text
include/
  pix/    public pixel frame API (+ sdl.h when SDL enabled)
  vg/     vector graphics headers
src/
  pix/    per‑format frame ops + helpers (and SDL backend when enabled)
  vg/     path / fill / primitives / canvas / transform / shape
examples/
  sdldemo   simple animated primitives
  sdltiger  SVG tiger subset (polylines) demo
  sdlimage  image blit + transform showcase
  sdlicons  generated icon grid (multi-shape SVG subpaths)
```

## Build

Requirements: CMake (>= 3.10), a C compiler, and SDL2 development headers (only needed for the SDL examples / backend).

Typical out‑of‑source build:

```bash
mkdir -p build
cd build
cmake ..
make -j
```

This produces example executables in `build/examples/*` (`sdldemo`, `sdltiger`, `sdlimage`).

If you already have a generated Makefile at the repository root, you can also run:

```bash
make
```

## Run

From the build directory run any example, e.g.:

```bash
examples/sdldemo/sdldemo
```

Resizable window shows animated AA fills + strokes.

## Docs (Doxygen)

If Doxygen is installed, you can generate API docs:

```bash
cmake .
make docs
```

HTML will be emitted under `./docs/html/` in the build directory. If CMake reports that Doxygen is not found, install it via your package manager and rerun CMake.

## Using the APIs

### Pixels (`pix/frame.h`)

`pix_frame_t` is a generic pixel buffer descriptor with function pointers set per format at creation:

```c
typedef struct pix_frame_t {
  pix_size_t size;
  pix_format_t format;       // RGB24 / RGBA32 / GRAY8 / RGB565
  void *pixels;               // backing store
  size_t pitch;               // bytes per row
  // function pointers (may be NULL if unsupported by backend):
  void (*destroy)(struct pix_frame_t*);       // optional cleanup (self-frees)
  bool (*lock)(struct pix_frame_t*);          // optional (e.g. SDL texture)
  void (*unlock)(struct pix_frame_t*);
  void (*set_pixel)(struct pix_frame_t*, pix_point_t pt, pix_color_t c);
  pix_color_t (*get_pixel)(struct pix_frame_t*, pix_point_t pt);
  void (*draw_line)(struct pix_frame_t*, pix_point_t a, pix_point_t b, pix_color_t c);
  void (*copy)(struct pix_frame_t *dst, pix_point_t dst_origin,
               struct pix_frame_t *src, pix_point_t src_origin,
               pix_size_t size, pix_blit_flags_t flags);
} pix_frame_t;
```

Blit flags (`pix_blit_flags_t`) currently: `PIX_BLIT_NONE`, `PIX_BLIT_ALPHA`.

Per‑format optimized implementations are internal; you manipulate frames only via these pointers. `PIX_COLOR_NONE` (0) denotes “no paint”.

### Vector Graphics

* Paths (`vg/path.h`): segmented list of packed `int16_t` points. Append points variadically: `vg_path_append(path, &p0, &p1, &p2, NULL);`
* Shapes (`vg/shape.h`): style (fill/stroke colors, widths, caps, joins, miter limit, fill rule) + optional transform pointer or image descriptor (`vg_shape_set_image`).
* Primitives (`vg/primitives.h`): helpers to append rectangles, circles, ellipses, rounded rects, triangles to a path.
* Canvas (`vg/canvas.h`): growable list (chunked pointer arrays) owning appended shapes; `vg_canvas_render` draws fill then stroke.
* Fill (`vg/fill.h`): internal scan conversion used by render; you normally rely on canvas.
* Transforms (`vg/transform.h`): 3×3 affine matrix helpers; apply at render time.
* Bounding boxes: `vg_shape_bbox` for a single shape, `vg_canvas_bbox` for all shapes (ignores transforms & stroke expansion currently).

### Image Shapes

Turn a shape into an image blit: `vg_shape_set_image(shape, frame, src_origin, src_size, dst_origin, flags)`. Size `{0,0}` means full frame. Affine transform (if set) may scale / rotate the blit (axis‑aligned fast path + general affine fallback).

### SDL glue

When built with SDL (`PIX_ENABLE_SDL` defined) `pix/sdl.h` exposes a single convenience function that creates a window + streaming texture and returns a `pix_frame_t*` whose pixel buffer maps the texture:

```c
#include <pix/sdl.h> // requires build with SDL
pix_frame_t *frame = pixsdl_frame_init("Icons", (pix_size_t){ 1024, 768 }, PIX_FMT_RGBA32);
// ... render ...
frame->destroy(frame);
```

All SDL window / renderer / texture details are intentionally hidden; the returned frame supplies `lock`, `unlock`, and `destroy` callbacks. Previous separate library name and accessors were removed to keep the public surface minimal.

Color encoding is `0xAARRGGBB` (straight alpha). Use `PIX_COLOR_NONE` to disable fill or stroke.

### Canvas Growth Strategy

The canvas stores an array of shape pointers per chunk. When a chunk fills, a new chunk (same capacity as head) is linked. Each shape is currently heap‑allocated; future pooling optimizations may inline shapes per chunk. Ownership: shapes appended to a canvas are freed by `vg_canvas_destroy`.

Guidelines:

* Do not manually destroy shapes returned by `vg_canvas_append`.
* Use `vg_shape_create` only for shapes you manage outside a canvas.
* Mutate geometry directly via `vg_shape_path(shape)` + `vg_path_append`.

## Notes & Tips

* `vg_path_append` is variadic: always end the list with `NULL`.
* For large paths, the library allocates additional segments instead of reallocating the whole point array.
* Fill sampling uses half‑open pixel centers (y+0.5) to reduce seam artifacts.
* SDL texture format (when built with SDL) matches the in‑memory pixel layout to avoid channel swizzle.
* Bounding boxes ignore transforms & stroke expansion (future enhancement).

---

Early‑stage project; API surface intentionally small and may evolve.

## Generated Icon Demo (sdlicons)

An optional generator (Go tool under `cmd/svg2pix`) converts a curated SVG
icon set into C builders. Each icon can consist of multiple SVG subpaths;
the generator now preserves that structure by emitting a builder function
per icon with the signature:

```c
bool vg_icon_<style>_<name>(vg_shape_t **out_shapes, size_t *out_count);
```

Usage pattern:

1. Allocate a temporary array, e.g. `vg_shape_t *tmp[16]; size_t n = 0;`
2. Call the builder. On success `tmp[0..n-1]` are heap-allocated shapes
  (created via `vg_shape_create`) that you own.
3. Copy or append their path/style data into your canvas-owned shapes
  (e.g. by appending new shapes via `vg_canvas_append`).
4. Destroy the temporary shapes with `vg_shape_destroy` (do not pass them
  directly to a canvas; canvas will free only shapes it allocates).

Why: multiple subpaths previously had to be heuristically split after the
fact (detecting large jump segments). Preserving subpaths at generation
time avoids visual artifacts like unwanted stroke bridges.

The `examples/sdlicons` demo shows a 4x4 grid of randomly selected filled
and outline icons rendered white on black, with interactive pan / zoom /
rotate (arrow keys to pan, +/- to zoom, q/e to rotate, space to reshuffle).

Filled icons set only a fill color; outline icons set only a stroke style.

Note: This is an experimental pipeline; the icon generator is not yet part
of the default build and large icon sets will increase compile time.
