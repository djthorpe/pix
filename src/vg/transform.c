#include <math.h>
#include <vg/vg.h>

void vg_transform_identity(vg_transform_t *t) {
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      t->m[i][j] = (i == j) ? 1.0f : 0.0f;
    }
  }
}

void vg_transform_translate(vg_transform_t *t, float tx, float ty) {
  vg_transform_identity(t);
  t->m[0][2] = tx;
  t->m[1][2] = ty;
}

void vg_transform_scale(vg_transform_t *t, float sx, float sy) {
  vg_transform_identity(t);
  t->m[0][0] = sx;
  t->m[1][1] = sy;
}

void vg_transform_rotate(vg_transform_t *t, float angle) {
  vg_transform_identity(t);
  float c = cosf(angle);
  float s = sinf(angle);
  t->m[0][0] = c;
  t->m[0][1] = -s;
  t->m[1][0] = s;
  t->m[1][1] = c;
}

void vg_transform_multiply(vg_transform_t *out, const vg_transform_t *a,
                           const vg_transform_t *b) {
  vg_transform_t result;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result.m[i][j] = 0.0f;
      for (int k = 0; k < 3; ++k) {
        result.m[i][j] += a->m[i][k] * b->m[k][j];
      }
    }
  }
  *out = result;
}

void vg_transform_point(const vg_transform_t *t, float x, float y, float *out_x,
                        float *out_y) {
  float tx = t->m[0][0] * x + t->m[0][1] * y + t->m[0][2];
  float ty = t->m[1][0] * x + t->m[1][1] * y + t->m[1][2];
  *out_x = tx;
  *out_y = ty;
}
