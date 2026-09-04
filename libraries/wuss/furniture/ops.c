/* wuss/furniture/ops.c -- wuss - core-to-furniture dispatch table */

#include "../core/impl.h"

/* The built-in furniture: wuss_create points every window's furniture_ops
 * here unless a caller overrides it. */
const wuss__furniture_ops_t wuss__furniture_default_ops =
{
  wuss__furniture_hit_test,
  wuss__furniture_draw,
  wuss__furniture_invalidate,
  wuss__furniture_invalidate_for,
  wuss__furniture_toggle_size,
  wuss__furniture_drag_resize,
  wuss__furniture_drag_sausage
};
