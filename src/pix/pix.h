#pragma once
#include <pix/pix.h>

// Private declarations for format-specialized implementations.
void pix_frame_set_pixel_rgb24(pix_frame_t *frame, pix_point_t pt,
                               pix_color_t color);
void pix_frame_set_pixel_rgba32(pix_frame_t *frame, pix_point_t pt,
                                pix_color_t color);
void pix_frame_set_pixel_gray8(pix_frame_t *frame, pix_point_t pt,
                               pix_color_t color);

void pix_frame_clear_rgb24(pix_frame_t *frame, pix_color_t value);
void pix_frame_clear_rgba32(pix_frame_t *frame, pix_color_t value);
void pix_frame_clear_gray8(pix_frame_t *frame, pix_color_t value);

void pix_frame_set_pixel_rgb24(struct pix_frame_t *frame, pix_point_t pt,
                               pix_color_t color);
void pix_frame_clear_rgb24(struct pix_frame_t *frame, pix_color_t value);
void pix_frame_set_pixel_rgba32(struct pix_frame_t *frame, pix_point_t pt,
                                pix_color_t color);
void pix_frame_clear_rgba32(struct pix_frame_t *frame, pix_color_t value);
void pix_frame_set_pixel_gray8(struct pix_frame_t *frame, pix_point_t pt,
                               pix_color_t color);
void pix_frame_clear_gray8(struct pix_frame_t *frame, pix_color_t value);
void pix_frame_set_pixel_rgb565(struct pix_frame_t *frame, pix_point_t pt,
                                pix_color_t color);
void pix_frame_clear_rgb565(struct pix_frame_t *frame, pix_color_t value);
