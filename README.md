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
  pix/    public pixel frame API
  vg/     vector graphics headers
  pixsdl/ SDL convenience layer (optional)
src/
  pix/    per‑format frame ops + helpers
  vg/     path / fill / primitives / canvas / transform / shape
  pixsdl/ SDL bridge (creates a `pix_frame_t` backed by streaming texture)
examples/
  sdldemo   simple animated primitives
  sdltiger  SVG tiger subset (polylines) demo
  sdlimage  image blit + transform showcase
```

## Build

Requirements: CMake (>= 3.10), a C compiler, and SDL2 development headers.

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
  void (*finalize)(struct pix_frame_t*);      // optional cleanup
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

`pixsdl/` provides helpers to wrap an SDL2 streaming texture as a `pix_frame_t` so the same software rendering can target a window.

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
* SDL texture format (when using pixsdl) matches the in‑memory pixel layout to avoid channel swizzle.
* Bounding boxes ignore transforms & stroke expansion (future enhancement).

---

Early‑stage project; API surface intentionally small and may evolve.
