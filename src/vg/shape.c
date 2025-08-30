#include "path_internal.h"
#include "shape_internal.h"
#include <vg/shape.h>
#include <vg/vg.h>

static void vg_shape_defaults(vg_shape_t *s) {
  s->kind = VG_SHAPE_PATH;
  s->transform = NULL;
  s->data.v.path = vg_path_init(64);
  s->data.v.fill_color = PIX_COLOR_NONE;
  s->data.v.stroke_color = PIX_COLOR_NONE;
  s->data.v.stroke_width = 1.0f;
  s->data.v.stroke_cap = VG_CAP_BUTT;
  s->data.v.stroke_join = VG_JOIN_BEVEL;
  s->data.v.miter_limit = 4.0f;
  s->data.v.fill_rule = VG_FILL_EVEN_ODD;
  s->data.img.frame = NULL;
  s->data.img.src_origin = (pix_point_t){0, 0};
  s->data.img.src_size = (pix_size_t){0, 0};
  s->data.img.dst_origin = (pix_point_t){0, 0};
  s->data.img.flags = 0;
}

/* Internal: exported (non-header) symbol for canvas pool initialization. */
void vg__shape_internal_defaults(vg_shape_t *s) { vg_shape_defaults(s); }

vg_shape_t *vg_shape_create(void) {
  vg_shape_t *s = (vg_shape_t *)VG_MALLOC(sizeof(vg_shape_t));
  if (!s)
    return NULL;
  vg_shape_defaults(s);
  return s;
}

void vg_shape_destroy(vg_shape_t *shape) {
  if (!shape)
    return;
  if (shape->kind == VG_SHAPE_PATH) {
    vg_path_finish(&shape->data.v.path);
  }
  VG_FREE(shape);
}

vg_path_t *vg_shape_path(vg_shape_t *shape) {
  if (!shape || shape->kind != VG_SHAPE_PATH)
    return NULL;
  return &shape->data.v.path;
}

void vg_shape_set_transform(vg_shape_t *shape, const vg_transform_t *xf) {
  if (shape)
    shape->transform = xf;
}
const vg_transform_t *vg_shape_get_transform(const vg_shape_t *shape) {
  return shape ? shape->transform : NULL;
}

void vg_shape_set_fill_color(vg_shape_t *shape, pix_color_t c) {
  if (shape && shape->kind == VG_SHAPE_PATH)
    shape->data.v.fill_color = c;
}
void vg_shape_set_stroke_color(vg_shape_t *shape, pix_color_t c) {
  if (shape && shape->kind == VG_SHAPE_PATH)
    shape->data.v.stroke_color = c;
}

pix_color_t vg_shape_get_fill_color(const vg_shape_t *shape) {
  return (shape && shape->kind == VG_SHAPE_PATH) ? shape->data.v.fill_color
                                                 : PIX_COLOR_NONE;
}
pix_color_t vg_shape_get_stroke_color(const vg_shape_t *shape) {
  return (shape && shape->kind == VG_SHAPE_PATH) ? shape->data.v.stroke_color
                                                 : PIX_COLOR_NONE;
}

void vg_shape_set_stroke_width(vg_shape_t *shape, float w) {
  if (shape && shape->kind == VG_SHAPE_PATH)
    shape->data.v.stroke_width = w;
}
float vg_shape_get_stroke_width(const vg_shape_t *shape) {
  return (shape && shape->kind == VG_SHAPE_PATH) ? shape->data.v.stroke_width
                                                 : 0.0f;
}
void vg_shape_set_stroke_cap(vg_shape_t *shape, vg_cap_t cap) {
  if (shape && shape->kind == VG_SHAPE_PATH)
    shape->data.v.stroke_cap = cap;
}
vg_cap_t vg_shape_get_stroke_cap(const vg_shape_t *shape) {
  return (shape && shape->kind == VG_SHAPE_PATH) ? shape->data.v.stroke_cap
                                                 : VG_CAP_BUTT;
}
void vg_shape_set_stroke_join(vg_shape_t *shape, vg_join_t join) {
  if (shape && shape->kind == VG_SHAPE_PATH)
    shape->data.v.stroke_join = join;
}
vg_join_t vg_shape_get_stroke_join(const vg_shape_t *shape) {
  return (shape && shape->kind == VG_SHAPE_PATH) ? shape->data.v.stroke_join
                                                 : VG_JOIN_BEVEL;
}
void vg_shape_set_miter_limit(vg_shape_t *shape, float limit) {
  if (shape && shape->kind == VG_SHAPE_PATH)
    shape->data.v.miter_limit = limit;
}
float vg_shape_get_miter_limit(const vg_shape_t *shape) {
  return (shape && shape->kind == VG_SHAPE_PATH) ? shape->data.v.miter_limit
                                                 : 0.0f;
}

void vg_shape_set_fill_rule(vg_shape_t *shape, vg_fill_rule_t rule) {
  if (shape && shape->kind == VG_SHAPE_PATH)
    shape->data.v.fill_rule = rule;
}
vg_fill_rule_t vg_shape_get_fill_rule(const vg_shape_t *shape) {
  return (shape && shape->kind == VG_SHAPE_PATH) ? shape->data.v.fill_rule
                                                 : VG_FILL_EVEN_ODD;
}

void vg_shape_set_image(vg_shape_t *shape, const pix_frame_t *frame,
                        pix_point_t src_origin, pix_size_t src_size,
                        pix_point_t dst_origin, unsigned flags) {
  if (!shape)
    return;
  /* If currently a path, release its path storage. */
  if (shape->kind == VG_SHAPE_PATH) {
    vg_path_finish(&shape->data.v.path);
  }
  shape->kind = VG_SHAPE_IMAGE;
  shape->data.img.frame = frame;
  shape->data.img.src_origin = src_origin;
  shape->data.img.src_size = src_size;
  shape->data.img.dst_origin = dst_origin;
  shape->data.img.flags = flags;
}

bool vg_shape_path_clear(vg_shape_t *shape, size_t reserve) {
  if (!shape || shape->kind != VG_SHAPE_PATH)
    return false;
  vg_path_finish(&shape->data.v.path);
  if (reserve < 4)
    reserve = 4;
  shape->data.v.path = vg_path_init(reserve);
  return shape->data.v.path.points != NULL;
}

void vg_shape_bbox(const vg_shape_t *shape, pix_point_t *origin,
                   pix_size_t *size) {
  if (origin) {
    origin->x = 0;
    origin->y = 0;
  }
  if (size) {
    size->w = 0;
    size->h = 0;
  }
  if (!shape)
    return;
  if (shape->kind == VG_SHAPE_PATH) {
    const vg_path_t *p = &shape->data.v.path;
    bool first = true;
    int minx = 0, miny = 0, maxx = 0, maxy = 0;
    while (p) {
      for (size_t i = 0; i < p->size; ++i) {
        pix_point_t pt = p->points[i];
        int x = pt.x, y = pt.y;
        if (first) {
          minx = maxx = x;
          miny = maxy = y;
          first = false;
        } else {
          if (x < minx)
            minx = x;
          if (x > maxx)
            maxx = x;
          if (y < miny)
            miny = y;
          if (y > maxy)
            maxy = y;
        }
      }
      p = p->next;
    }
    if (!first) {
      if (origin) {
        origin->x = (int16_t)minx;
        origin->y = (int16_t)miny;
      }
      if (size) {
        int w = maxx - minx;
        int h = maxy - miny;
        if (w < 0)
          w = 0;
        if (h < 0)
          h = 0;
        if (w > 0xFFFF)
          w = 0xFFFF;
        if (h > 0xFFFF)
          h = 0xFFFF;
        size->w = (uint16_t)w;
        size->h = (uint16_t)h;
      }
    }
  } else if (shape->kind == VG_SHAPE_IMAGE) {
    const vg_image_ref_t *img = &shape->data.img;
    if (!img->frame)
      return;
    int x0 = img->dst_origin.x;
    int y0 = img->dst_origin.y;
    int w = img->src_size.w ? img->src_size.w : img->frame->size.w;
    int h = img->src_size.h ? img->src_size.h : img->frame->size.h;
    if (origin) {
      origin->x = (int16_t)x0;
      origin->y = (int16_t)y0;
    }
    if (size) {
      if (w < 0)
        w = 0;
      if (h < 0)
        h = 0;
      if (w > 0xFFFF)
        w = 0xFFFF;
      if (h > 0xFFFF)
        h = 0xFFFF;
      size->w = (uint16_t)w;
      size->h = (uint16_t)h;
    }
  }
}
