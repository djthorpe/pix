#include <math.h>
#include <pix/frame.h>

void pix_frame_set_pixel(pix_frame_t *frame, size_t x, size_t y,
                         uint32_t color) {
  if (!frame || !frame->pixels)
    return;
  if (x >= frame->width || y >= frame->height)
    return;
  uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
  switch (frame->format) {
  case PIX_FMT_RGB888:
    row[x * 3 + 0] = (color >> 16) & 0xFF; // R
    row[x * 3 + 1] = (color >> 8) & 0xFF;  // G
    row[x * 3 + 2] = color & 0xFF;         // B
    break;
  case PIX_FMT_RGBA8888: {
    // Color format: 0xAARRGGBB, memory layout R G B A
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t sr = (color >> 16) & 0xFF;
    uint8_t sg = (color >> 8) & 0xFF;
    uint8_t sb = color & 0xFF;
    if (a == 255) {
      row[x * 4 + 0] = sr;
      row[x * 4 + 1] = sg;
      row[x * 4 + 2] = sb;
      row[x * 4 + 3] = 255;
    } else if (a == 0) {
      // nothing to draw
    } else {
      // Straight alpha src-over dst
      float fa = a / 255.0f;
      uint8_t dr = row[x * 4 + 0];
      uint8_t dg = row[x * 4 + 1];
      uint8_t db = row[x * 4 + 2];
      uint8_t r = (uint8_t)lroundf(sr * fa + dr * (1.0f - fa));
      uint8_t g = (uint8_t)lroundf(sg * fa + dg * (1.0f - fa));
      uint8_t b = (uint8_t)lroundf(sb * fa + db * (1.0f - fa));
      uint8_t da = row[x * 4 + 3];
      uint8_t out_a =
          (uint8_t)lroundf(255.0f * (fa + (da / 255.0f) * (1.0f - fa)));
      row[x * 4 + 0] = r;
      row[x * 4 + 1] = g;
      row[x * 4 + 2] = b;
      row[x * 4 + 3] = out_a;
    }
  } break;
  case PIX_FMT_GRAY8: {
    uint8_t g = (uint8_t)((((color >> 16) & 0xFF) * 30 +
                           ((color >> 8) & 0xFF) * 59 + (color & 0xFF) * 11) /
                          100);
    row[x] = g;
    break;
  }
  default:
    break;
  }
}

void pix_frame_draw_line(pix_frame_t *frame, size_t x0, size_t y0, size_t x1,
                         size_t y1, uint32_t color) {
  if (!frame || !frame->set_pixel)
    return;
  int dx = abs((int)x1 - (int)x0), sx = (int)x0 < (int)x1 ? 1 : -1;
  int dy = -abs((int)y1 - (int)y0), sy = (int)y0 < (int)y1 ? 1 : -1;
  int err = dx + dy;
  int x = (int)x0, y = (int)y0;
  for (;;) {
    frame->set_pixel(frame, (size_t)x, (size_t)y, color);
    if (x == (int)x1 && y == (int)y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y += sy;
    }
  }
}

void pix_frame_clear(pix_frame_t *frame, uint32_t value) {
  if (!frame || !frame->pixels)
    return;
  for (size_t y = 0; y < frame->height; ++y) {
    uint8_t *row = (uint8_t *)frame->pixels + y * frame->stride;
    switch (frame->format) {
    case PIX_FMT_RGB888: {
      uint8_t r = (value >> 16) & 0xFF;
      uint8_t g = (value >> 8) & 0xFF;
      uint8_t b = value & 0xFF;
      for (size_t x = 0; x < frame->width; ++x) {
        row[x * 3 + 0] = r;
        row[x * 3 + 1] = g;
        row[x * 3 + 2] = b;
      }
      break;
    }
    case PIX_FMT_RGBA8888: {
      uint8_t a = (value >> 24) & 0xFF;
      uint8_t r = (value >> 16) & 0xFF;
      uint8_t g = (value >> 8) & 0xFF;
      uint8_t b = value & 0xFF;
      for (size_t x = 0; x < frame->width; ++x) {
        row[x * 4 + 0] = r;
        row[x * 4 + 1] = g;
        row[x * 4 + 2] = b;
        row[x * 4 + 3] = a;
      }
      break;
    }
    case PIX_FMT_GRAY8: {
      uint8_t g = (uint8_t)((((value >> 16) & 0xFF) * 30 +
                             ((value >> 8) & 0xFF) * 59 + (value & 0xFF) * 11) /
                            100);
      for (size_t x = 0; x < frame->width; ++x) {
        row[x] = g;
      }
      break;
    }
    default:
      for (size_t x = 0; x < frame->stride; ++x) {
        row[x] = ((uint8_t *)&value)[x % sizeof(value)];
      }
      break;
    }
  }
}
