#include <pix/frame.h>
#include <stdlib.h>
#include <string.h>
#include <vg/vg.h>

// Updated descent to 2 so we have one full pixel row available for synthetic
// descenders (g, y, p, q, j). The core 5x7 bitmap supplies rows 0..6; we add
// an artificial row 7 for those glyphs below via bitmap_descender_row.
const vg_font_t vg_font_tiny5x7 = {.ascent = 7, .descent = 2};

// Extra bottom row bits (row index 7) for characters with descenders.
// Indexed by (ch - 32). Bits use same convention (col0 bit=0x80 ... col4=0x08).
// We keep a single centered pixel (0x20) for a minimal tail; refine later if
// desired.
static const uint8_t bitmap_descender_row[95] = {
    // 0x20..0x26
    0, 0, 0, 0, 0, 0, 0,
    // 0x27..0x2D
    0, 0, 0, 0, 0, 0, 0,
    // 0x2E..0x34
    0, 0, 0, 0, 0, 0, 0,
    // 0x35..0x3B
    0, 0, 0, 0, 0, 0, 0,
    // 0x3C..0x42
    0, 0, 0, 0, 0, 0, 0,
    // 0x43..0x49
    0, 0, 0, 0, 0, 0, 0,
    // 0x4A..0x50
    0, 0, 0, 0, 0, 0, 0,
    // 0x51..0x57
    0, 0, 0, 0, 0, 0, 0,
    // 0x58..0x5E
    0, 0, 0, 0, 0, 0, 0,
    // 0x5F..0x65
    0, 0, 0, 0, 0, 0, 0,
    // 0x66..0x6C  (indices 70..76) descenders: g(col4=0x08), j(col1=0x40)
    0, 0x08, 0, 0, 0, 0x40, 0, // f g h i j k l
    // 0x6D..0x73 (indices 77..83) descenders: p(col0=0x80), q(col4=0x08)
    0, 0, 0x80, 0x08, 0, 0, 0, // m n o p q r s
    // 0x74..0x7A (indices 84..90) descender: y(col4=0x08)
    0, 0, 0, 0, 0, 0x08, 0, // t u v w x y z
    // 0x7B..0x7F
    0, 0, 0, 0, 0};

static const uint8_t bitmap_5x7[95][7] = {
    // 0x20 ' '
    {0, 0, 0, 0, 0, 0, 0},
    // 0x21 '!'
    {0x80, 0x80, 0x80, 0x80, 0x00, 0x80, 0x00},
    // 0x22 '"'
    {0xA0, 0xA0, 0x40, 0x00, 0x00, 0x00, 0x00},
    // 0x23 '#'
    {0x50, 0xF8, 0x50, 0xF8, 0x50, 0x00, 0x00},
    // 0x24 '$'
    {0x70, 0xA0, 0x70, 0x28, 0x70, 0x20, 0x00},
    // 0x25 '%'
    {0xC0, 0xC8, 0x10, 0x20, 0x4C, 0x0C, 0x00},
    // 0x26 '&'
    {0x60, 0x90, 0xA0, 0x40, 0xA8, 0x90, 0x68},
    // 0x27 ''
    {0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00},
    // 0x28 '('
    {0x20, 0x40, 0x80, 0x80, 0x80, 0x40, 0x20},
    // 0x29 ')'
    {0x80, 0x40, 0x20, 0x20, 0x20, 0x40, 0x80},
    // 0x2A '*'
    {0x00, 0xA0, 0x40, 0xE0, 0x40, 0xA0, 0x00},
    // 0x2B '+'
    {0x00, 0x40, 0x40, 0xE0, 0x40, 0x40, 0x00},
    // 0x2C ','
    {0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x80},
    // 0x2D '-'
    {0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00},
    // 0x2E '.'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xC0},
    // 0x2F '/'
    {0x00, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00},
    // 0x30 '0'
    {0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70},
    // 0x31 '1'
    {0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70},
    // 0x32 '2'
    {0x70, 0x88, 0x08, 0x30, 0x40, 0x80, 0xF8},
    // 0x33 '3'
    {0xF0, 0x08, 0x08, 0x70, 0x08, 0x08, 0xF0},
    // 0x34 '4'
    {0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10},
    // 0x35 '5'
    {0xF8, 0x80, 0x80, 0xF0, 0x08, 0x08, 0xF0},
    // 0x36 '6'
    {0x30, 0x40, 0x80, 0xF0, 0x88, 0x88, 0x70},
    // 0x37 '7'
    {0xF8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40},
    // 0x38 '8'
    {0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70},
    // 0x39 '9'
    {0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60},
    // 0x3A ':'
    {0x00, 0x00, 0xC0, 0xC0, 0x00, 0xC0, 0xC0},
    // 0x3B ';'
    {0x00, 0x00, 0xC0, 0xC0, 0x00, 0xC0, 0x40},
    // 0x3C '<'
    {0x10, 0x20, 0x40, 0x80, 0x40, 0x20, 0x10},
    // 0x3D '='
    {0x00, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00},
    // 0x3E '>'
    {0x80, 0x40, 0x20, 0x10, 0x20, 0x40, 0x80},
    // 0x3F '?'
    {0x70, 0x88, 0x08, 0x30, 0x20, 0x00, 0x20},
    // 0x40 '@'
    {0x70, 0x88, 0xA8, 0xB8, 0xB0, 0x80, 0x78},
    // 0x41 'A'
    {0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88},
    // 0x42 'B'
    {0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0},
    // 0x43 'C'
    {0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70},
    // 0x44 'D'
    {0xE0, 0x90, 0x88, 0x88, 0x88, 0x90, 0xE0},
    // 0x45 'E'
    {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8},
    // 0x46 'F'
    {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80},
    // 0x47 'G'
    {0x70, 0x88, 0x80, 0xB8, 0x88, 0x88, 0x70},
    // 0x48 'H'
    {0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88},
    // 0x49 'I'
    {0xE0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xE0},
    // 0x4A 'J'
    {0x38, 0x10, 0x10, 0x10, 0x10, 0x90, 0x60},
    // 0x4B 'K'
    {0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88},
    // 0x4C 'L'
    {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8},
    // 0x4D 'M'
    {0x88, 0xD8, 0xA8, 0xA8, 0x88, 0x88, 0x88},
    // 0x4E 'N'
    {0x88, 0xC8, 0xC8, 0xA8, 0x98, 0x98, 0x88},
    // 0x4F 'O'
    {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70},
    // 0x50 'P'
    {0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80},
    // 0x51 'Q'
    {0x70, 0x88, 0x88, 0x88, 0xA8, 0x90, 0x68},
    // 0x52 'R'
    {0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88},
    // 0x53 'S'
    {0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70},
    // 0x54 'T'
    {0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20},
    // 0x55 'U'
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70},
    // 0x56 'V'
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x50, 0x20},
    // 0x57 'W'
    {0x88, 0x88, 0xA8, 0xA8, 0xA8, 0xD8, 0x88},
    // 0x58 'X'
    {0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88},
    // 0x59 'Y'
    {0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20},
    // 0x5A 'Z'
    {0xF8, 0x10, 0x20, 0x40, 0x80, 0x80, 0xF8},
    // 0x5B '['
    {0xE0, 0x80, 0x80, 0x80, 0x80, 0x80, 0xE0},
    // 0x5C '\\'
    {0x00, 0x80, 0x40, 0x20, 0x10, 0x08, 0x00},
    // 0x5D ']'
    {0xE0, 0x20, 0x20, 0x20, 0x20, 0x20, 0xE0},
    // 0x5E '^'
    {0x20, 0x50, 0x88, 0x00, 0x00, 0x00, 0x00},
    // 0x5F '_'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8},
    // 0x60 '`'
    {0x40, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00},
    // 0x61 'a'
    {0x00, 0x00, 0x70, 0x08, 0x78, 0x88, 0x78},
    // 0x62 'b'
    {0x80, 0x80, 0xB0, 0xC8, 0x88, 0xC8, 0xB0},
    // 0x63 'c'
    {0x00, 0x00, 0x70, 0x80, 0x80, 0x80, 0x70},
    // 0x64 'd'
    {0x08, 0x08, 0x68, 0x98, 0x88, 0x98, 0x68},
    // 0x65 'e'
    {0x00, 0x00, 0x70, 0x88, 0xF8, 0x80, 0x70},
    // 0x66 'f'
    {0x30, 0x40, 0xF0, 0x40, 0x40, 0x40, 0x40},
    // 0x67 'g'
    {0x00, 0x00, 0x68, 0x98, 0x98, 0x68, 0x08},
    // 0x68 'h'
    {0x80, 0x80, 0xB0, 0xC8, 0x88, 0x88, 0x88},
    // 0x69 'i'
    {0x40, 0x00, 0x40, 0x40, 0x40, 0x40, 0x60},
    // 0x6A 'j'
    {0x20, 0x00, 0x20, 0x20, 0x20, 0xA0, 0x40},
    // 0x6B 'k'
    {0x80, 0x80, 0x90, 0xA0, 0xC0, 0xA0, 0x90},
    // 0x6C 'l'
    {0xC0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40},
    // 0x6D 'm'
    {0x00, 0x00, 0xD0, 0xA8, 0xA8, 0x88, 0x88},
    // 0x6E 'n'
    {0x00, 0x00, 0xB0, 0xC8, 0x88, 0x88, 0x88},
    // 0x6F 'o'
    {0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x70},
    // 0x70 'p'
    {0x00, 0x00, 0xB0, 0xC8, 0xC8, 0xB0, 0x80},
    // 0x71 'q'
    {0x00, 0x00, 0x68, 0x98, 0x98, 0x68, 0x08},
    // 0x72 'r'
    {0x00, 0x00, 0xB0, 0xC8, 0x80, 0x80, 0x80},
    // 0x73 's'
    {0x00, 0x00, 0x70, 0x80, 0x70, 0x08, 0xF0},
    // 0x74 't'
    {0x40, 0x40, 0xF0, 0x40, 0x40, 0x40, 0x30},
    // 0x75 'u'
    {0x00, 0x00, 0x88, 0x88, 0x88, 0x98, 0x68},
    // 0x76 'v'
    {0x00, 0x00, 0x88, 0x88, 0x88, 0x50, 0x20},
    // 0x77 'w'
    {0x00, 0x00, 0x88, 0xA8, 0xA8, 0xA8, 0x50},
    // 0x78 'x'
    {0x00, 0x00, 0x88, 0x50, 0x20, 0x50, 0x88},
    // 0x79 'y'
    {0x00, 0x00, 0x88, 0x88, 0x98, 0x68, 0x08},
    // 0x7A 'z'
    {0x00, 0x00, 0xF8, 0x10, 0x20, 0x40, 0xF8},
    // 0x7B '{'
    {0x30, 0x20, 0x20, 0xC0, 0x20, 0x20, 0x30},
    // 0x7C '|'
    {0x40, 0x40, 0x40, 0x00, 0x40, 0x40, 0x40},
    // 0x7D '}'
    {0xC0, 0x40, 0x40, 0x30, 0x40, 0x40, 0xC0},
    // 0x7E '~'
    {0x00, 0x00, 0x50, 0xA0, 0x00, 0x00, 0x00},
};

float vg_font_text_width(const vg_font_t *font, const char *text,
                         float pixel_size, float letter_spacing) {
  if (!font || !text)
    return 0.0f;
  if (pixel_size <= 0.0f)
    pixel_size = 7.0f;
  if (letter_spacing < 0)
    letter_spacing = 0;
  float scale = pixel_size / 7.0f;
  float width = 0, line = 0;
  float adv = (6.0f + letter_spacing) * scale;
  for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
    if (*p == '\n') {
      if (line > width)
        width = line;
      line = 0;
      continue;
    }
    line += adv;
  }
  if (line > width)
    width = line;
  return width;
}

typedef struct run_t {
  int x, y, w;
} run_t;

typedef struct run_buf_t {
  run_t *data;
  size_t size;
  size_t cap;
} run_buf_t;

static bool run_buf_push(run_buf_t *rb, int x, int y, int w) {
  if (w <= 0)
    return true;
  if (rb->size == rb->cap) {
    size_t ncap = rb->cap ? rb->cap * 2 : 64;
    void *np = VG_MALLOC(ncap * sizeof(run_t));
    if (!np)
      return false;
    if (rb->data)
      memcpy(np, rb->data, rb->size * sizeof(run_t));
    VG_FREE(rb->data);
    rb->data = (run_t *)np;
    rb->cap = ncap;
  }
  rb->data[rb->size++] = (run_t){x, y, w};
  return true;
}

static inline int32_t pack_i16(int x, int y) {
  return ((int32_t)(x & 0xFFFF) << 16) | (int32_t)(y & 0xFFFF);
}
static vg_path_t make_outline_from_runs(const run_buf_t *rb, int ascent) {
  // We'll build the first rectangle in 'head'; subsequent rectangles become
  // chained vg_path_t nodes via the 'next' pointer to keep them disjoint.
  vg_path_t head = vg_path_init(6); // 5 points + potential spare
  vg_path_t *current = &head;
  bool first_rect = true;
  bool *used = (bool *)VG_MALLOC(rb->size * sizeof(bool));
  if (!used)
    return head;
  memset(used, 0, rb->size * sizeof(bool));
  for (size_t i = 0; i < rb->size; ++i) {
    if (used[i])
      continue;
    run_t r = rb->data[i];
    int x = r.x, y0 = r.y, w = r.w, y1 = r.y + 1;
    bool extended;
    do {
      extended = false;
      for (size_t j = i + 1; j < rb->size; ++j) {
        if (used[j])
          continue;
        run_t r2 = rb->data[j];
        if (r2.x == x && r2.w == w && r2.y == y1) {
          used[j] = true;
          y1 = r2.y + 1;
          extended = true;
        }
      }
    } while (extended);
    int x0 = x, y_top = y0, x1 = x + w, y_bot = y1; // bottom exclusive
    // Allocate a new segment for every rectangle except the first
    vg_path_t *seg = current;
    if (!first_rect) {
      seg = (vg_path_t *)VG_MALLOC(sizeof(vg_path_t));
      if (!seg)
        continue; // skip rectangle on OOM
      *seg = vg_path_init(6);
      current->next = seg;
      current = seg;
    }
    first_rect = false;
    // y coordinates are row indices (top=0). Baseline occurs at font->ascent.
    vg_path_append(seg, pack_i16(x0, y_top));
    vg_path_append(seg, pack_i16(x1, y_top));
    vg_path_append(seg, pack_i16(x1, y_bot));
    vg_path_append(seg, pack_i16(x0, y_bot));
    vg_path_append(seg, pack_i16(x0, y_top));
  }
  VG_FREE(used);
  return head;
}

vg_shape_t *vg_font_make_text_shape(const vg_font_t *font, const char *text,
                                    pix_color_t color, float pixel_size,
                                    float letter_spacing, float *out_width) {
  if (!font || !text)
    return NULL;
  float scale = (pixel_size > 0 ? pixel_size : 7.0f) / 7.0f;
  run_buf_t rb = {0};
  int pen_x = 0;
  int baseline = font->ascent; // internal EM baseline (row after top rows)
  int ls =
      (int)(letter_spacing < 0 ? 0
                               : letter_spacing); // integer pixel spacing
                                                  // pre-scale for simplicity
  for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
    unsigned char ch = *p;
    if (ch == '\n') {
      pen_x = 0;
      baseline += (font->ascent + font->descent);
      continue;
    }
    if (ch < 32 || ch > 126) {
      pen_x += 6;
      continue;
    }
    const uint8_t *rows = bitmap_5x7[ch - 32];
    for (int row = 0; row < 7; ++row) {
      uint8_t bits = rows[row];
      int col = 0;
      while (col < 5) {
        if (bits & (0x80 >> col)) {
          int start = col;
          while (col < 5 && (bits & (0x80 >> col)))
            col++;
          int run_w = col - start;
          if (!run_buf_push(&rb, pen_x + start, row + (baseline - font->ascent),
                            run_w)) {
            VG_FREE(rb.data);
            return NULL;
          }
        } else
          col++;
      }
    }
    // Synthetic descender row (row index 7) for select glyphs.
    int idx = (int)ch - 32;
    if (idx >= 0 && idx < 95) {
      uint8_t bits = bitmap_descender_row[idx];
      if (bits) {
        int row = 7; // descender row
        int col = 0;
        while (col < 5) {
          if (bits & (0x80 >> col)) {
            int start = col;
            while (col < 5 && (bits & (0x80 >> col)))
              col++;
            int run_w = col - start;
            if (!run_buf_push(&rb, pen_x + start,
                              row + (baseline - font->ascent), run_w)) {
              VG_FREE(rb.data);
              return NULL;
            }
          } else {
            col++;
          }
        }
      }
    }
    pen_x += 6 + ls; // advance with letter spacing
  }
  vg_path_t outline = make_outline_from_runs(&rb, font->ascent);
  VG_FREE(rb.data);
  vg_shape_t *shape = vg_shape_create();
  if (!shape) {
    vg_path_finish(&outline);
    return NULL;
  }
  *vg_shape_path(shape) = outline;
  vg_shape_set_fill_color(shape, color);
  vg_shape_set_fill_rule(shape, VG_FILL_EVEN_ODD_RAW);
  // fill/stroke implied by colors (stroke none)
  vg_shape_set_stroke_width(shape, 0.0f);
  if (scale != 1.0f) {
    vg_transform_t *xf = (vg_transform_t *)VG_MALLOC(sizeof(vg_transform_t));
    if (!xf) {
      vg_path_finish(vg_shape_path(shape));
      VG_FREE(shape);
      return NULL;
    }
    vg_transform_scale(xf, scale, scale);
    vg_shape_set_transform(shape, xf);
  }
  if (out_width)
    *out_width = pen_x * scale;
  return shape;
}

// ------------- Simple cache for text shapes -------------
typedef struct cached_text_shape_t {
  const vg_font_t *font;
  pix_color_t color;
  float pixel_size;
  float letter_spacing;
  char *text;        // owned copy
  vg_shape_t *shape; // owned
} cached_text_shape_t;

static cached_text_shape_t *g_text_cache = NULL;
static size_t g_text_cache_size = 0;
static size_t g_text_cache_cap = 0;
static size_t g_text_cache_limit = 64; // simple LRU cap (adjustable)

static void text_cache_evict_if_needed(void) {
  if (g_text_cache_size < g_text_cache_limit)
    return;
  // Evict oldest (index 0)
  cached_text_shape_t victim = g_text_cache[0];
  if (victim.shape) {
    const vg_transform_t *xf = vg_shape_get_transform(victim.shape);
    if (xf)
      VG_FREE((void *)xf);
    vg_shape_destroy(victim.shape);
  }
  VG_FREE(victim.text);
  memmove(&g_text_cache[0], &g_text_cache[1],
          (g_text_cache_size - 1) * sizeof(cached_text_shape_t));
  g_text_cache_size--;
}

static bool text_cache_append(const cached_text_shape_t *entry) {
  if (g_text_cache_size == g_text_cache_cap) {
    size_t ncap = g_text_cache_cap ? g_text_cache_cap * 2 : 16;
    void *np = VG_MALLOC(ncap * sizeof(cached_text_shape_t));
    if (!np)
      return false;
    if (g_text_cache)
      memcpy(np, g_text_cache, g_text_cache_size * sizeof(cached_text_shape_t));
    VG_FREE(g_text_cache);
    g_text_cache = (cached_text_shape_t *)np;
    g_text_cache_cap = ncap;
  }
  g_text_cache[g_text_cache_size++] = *entry;
  return true;
}

vg_shape_t *vg_font_get_text_shape_cached(const vg_font_t *font,
                                          const char *text, pix_color_t color,
                                          float pixel_size,
                                          float letter_spacing,
                                          float *out_width) {
  if (!font || !text)
    return NULL;
  // Lookup
  for (size_t i = 0; i < g_text_cache_size; ++i) {
    cached_text_shape_t *e = &g_text_cache[i];
    if (e->font == font && e->color == color && e->pixel_size == pixel_size &&
        e->letter_spacing == letter_spacing && strcmp(e->text, text) == 0) {
      if (out_width) {
        // Recompute quickly (stored width not kept) using measure
        *out_width = vg_font_text_width(font, text, pixel_size, letter_spacing);
      }
      return e->shape;
    }
  }
  // Create new
  float w = 0.0f;
  vg_shape_t *shape = vg_font_make_text_shape(font, text, color, pixel_size,
                                              letter_spacing, &w);
  if (!shape)
    return NULL;
  // Enforce cache size
  text_cache_evict_if_needed();
  cached_text_shape_t e;
  e.font = font;
  e.color = color;
  e.pixel_size = pixel_size;
  e.letter_spacing = letter_spacing;
  size_t len = strlen(text);
  e.text = (char *)VG_MALLOC(len + 1);
  if (!e.text) {
    const vg_transform_t *xf = vg_shape_get_transform(shape);
    if (xf)
      VG_FREE((void *)xf);
    vg_shape_destroy(shape);
    return NULL;
  }
  memcpy(e.text, text, len + 1);
  e.shape = shape;
  if (!text_cache_append(&e)) {
    const vg_transform_t *xf = vg_shape_get_transform(shape);
    if (xf)
      VG_FREE((void *)xf);
    vg_shape_destroy(shape);
    VG_FREE(e.text);
    return NULL;
  }
  if (out_width)
    *out_width = w;
  return shape;
}

// --------- Cache management public API ---------
void vg_font_text_cache_clear(void) {
  for (size_t i = 0; i < g_text_cache_size; ++i) {
    cached_text_shape_t *e = &g_text_cache[i];
    if (e->shape) {
      const vg_transform_t *xf = vg_shape_get_transform(e->shape);
      if (xf)
        VG_FREE((void *)xf);
      vg_shape_destroy(e->shape);
    }
    VG_FREE(e->text);
  }
  g_text_cache_size = 0;
}
size_t vg_font_text_cache_size(void) { return g_text_cache_size; }
size_t vg_font_text_cache_limit(void) { return g_text_cache_limit; }
void vg_font_text_cache_set_limit(size_t n) {
  if (n == 0)
    n = 1; // minimum 1
  g_text_cache_limit = n;
  // Evict until within limit
  while (g_text_cache_size > g_text_cache_limit) {
    text_cache_evict_if_needed();
  }
}
