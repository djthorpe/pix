# VG demo (software vector graphics + SDL2)

A tiny software vector-graphics renderer in C with an SDL2 demo. It renders filled and stroked polylines with transparency and anti‑aliasing into a pixel buffer, then presents via an SDL streaming texture.

## Features

- Paths and shapes (rect, circle, ellipse, rounded‑rect, triangle)
- Transforms (2D affine: translate, scale, rotate, multiply)
- Fill rules: even‑odd and non‑zero
- Scanline fill with X‑axis anti‑aliasing (coverage)
- Strokes with width, caps (butt/square/round), joins (bevel/round)
- Hairline stroke AA (Xiaolin Wu)
- RGBA src‑over blending (straight alpha) in software
- Embedded‑friendly: minimal heap churn; stack-first buffers with malloc fallback

## Layout

- `include/`
  - Public headers: `vg/`, `pix/`, `sdl/`
- `src/`
  - Implementations: `vg/`, `pix/`, `sdl/`
- `demo.c`
  - Small program that animates a square, circle, and triangle

## Build

Requirements: CMake (>= 3.10), a C compiler, and SDL2 development headers.

Typical out‑of‑source build:

```bash
mkdir -p build
cd build
cmake ..
make -j
```

This produces a `demo` executable in `build/`.

If you already have a generated Makefile at the repository root, you can also run:

```bash
make
```

## Run

From the build directory:

```bash
./demo
```

A resizable window opens; the scene animates with AA, fills, and strokes.

## Docs (Doxygen)

If Doxygen is installed, you can generate API docs:

```bash
cmake .
make docs
```

HTML will be emitted under `./docs/html/` in the build directory. If CMake reports that Doxygen is not found, install it via your package manager and rerun CMake.

## Using the APIs

- Pixels: `pix/frame.h` defines `pix_frame_t` with lock/unlock, set_pixel, draw_line, and clear. Formats supported: `RGB888`, `RGBA8888`, and `GRAY8`.
- SDL glue: `sdl/sdl_app.h` and `sdl/frame_sdl.h` wrap an SDL window/renderer/streaming texture and expose a `pix_frame_t` you can render into.
- Vector graphics:
  - Paths: `vg/path.h` (packed i16 points, chainable segments)
  - Shapes: `vg/shape.h` (style, transform pointer)
  - Primitives: `vg/primitives.h` (rect/circle/ellipse/rounded‑rect/triangle)
  - Fill: `vg/fill.h` (`vg_fill_path/shape`, rule selection)
  - Canvas: `vg/canvas.h` (append shapes and render fill+stroke in order)
  - Transforms: `vg/transform.h` (build and combine 3×3 affine matrices)

Color is `0xAARRGGBB` (straight alpha). Use `VG_COLOR_NONE` where applicable to disable fill or stroke.

## Notes & Tips

- SDL texture format is set to `ABGR8888` for RGBA to match the CPU memory byte order on little‑endian systems; this prevents color channel swaps.
- Fill sampling uses half‑open pixel coverage (y+0.5) to avoid seam artifacts; intersections are recomputed per scanline.
- For embedded targets, the renderer prefers stack buffers and only allocates on the heap for large paths.
