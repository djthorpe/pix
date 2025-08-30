/* Internal color utilities (not public API) */
#pragma once
#include <stdint.h>

static inline uint8_t pix_luma(uint8_t r, uint8_t g, uint8_t b) {
  /* Integer approximation of ITU-R BT.601 luma: 0.299R + 0.587G + 0.114B */
  return (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
}
