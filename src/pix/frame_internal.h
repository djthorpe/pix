/* Internal frame helpers (not part of public API) */
#pragma once
#include <pix/pix.h>

/* Generic dispatching helpers retained internally for VG renderer/backends */
void pix_frame_set_pixel(pix_frame_t *frame, pix_point_t pt, pix_color_t color);
pix_color_t pix_frame_get_pixel(const pix_frame_t *frame, pix_point_t pt);
void pix_frame_draw_line(pix_frame_t *frame, pix_point_t a, pix_point_t b,
                         pix_color_t color);
void pix_frame_clear(pix_frame_t *frame, pix_color_t value);
bool pix_frame_copy(pix_frame_t *dst, pix_point_t dst_origin,
                    const pix_frame_t *src, pix_point_t src_origin,
                    pix_size_t size, pix_blit_flags_t flags);

/* Per-format helpers (previously public) */
void pix_frame_set_pixel_rgb24(pix_frame_t *, pix_point_t, pix_color_t);
void pix_frame_set_pixel_rgba32(pix_frame_t *, pix_point_t, pix_color_t);
void pix_frame_set_pixel_gray8(pix_frame_t *, pix_point_t, pix_color_t);
void pix_frame_set_pixel_rgb565(pix_frame_t *, pix_point_t, pix_color_t);
void pix_frame_clear_rgb24(pix_frame_t *, pix_color_t);
void pix_frame_clear_rgba32(pix_frame_t *, pix_color_t);
void pix_frame_clear_gray8(pix_frame_t *, pix_color_t);
void pix_frame_clear_rgb565(pix_frame_t *, pix_color_t);
pix_color_t pix_frame_get_pixel_rgb24(const pix_frame_t *, pix_point_t);
pix_color_t pix_frame_get_pixel_rgba32(const pix_frame_t *, pix_point_t);
pix_color_t pix_frame_get_pixel_gray8(const pix_frame_t *, pix_point_t);
pix_color_t pix_frame_get_pixel_rgb565(const pix_frame_t *, pix_point_t);
bool pix_frame_copy_from_rgb24(pix_frame_t *dst, pix_point_t dst_origin,
                               const pix_frame_t *src, pix_point_t src_origin,
                               pix_size_t size, pix_blit_flags_t flags);
bool pix_frame_copy_from_rgba32(pix_frame_t *dst, pix_point_t dst_origin,
                                const pix_frame_t *src, pix_point_t src_origin,
                                pix_size_t size, pix_blit_flags_t flags);
bool pix_frame_copy_from_gray8(pix_frame_t *dst, pix_point_t dst_origin,
                               const pix_frame_t *src, pix_point_t src_origin,
                               pix_size_t size, pix_blit_flags_t flags);
bool pix_frame_copy_from_rgb565(pix_frame_t *dst, pix_point_t dst_origin,
                                const pix_frame_t *src, pix_point_t src_origin,
                                pix_size_t size, pix_blit_flags_t flags);