/* wuss/icon/slider-box.c -- slider groove geometry and value/pixel mapping */

#include "geom/box.h"
#include "geom/point.h"

#include "../core/impl.h"

void wuss__slider_groove_box(const box_t *screen_box, box_t *out)
{
  out->x0 = screen_box->x0 + WUSS_SLIDER_GAP;
  out->y0 = screen_box->y0 + WUSS_SLIDER_GAP;
  out->x1 = screen_box->x1 - WUSS_SLIDER_GAP;
  out->y1 = screen_box->y1 - WUSS_SLIDER_GAP;
}

int wuss__slider_value_to_px(const box_t              *groove,
                             wuss_slider_orientation_t orientation,
                             int                       value,
                             int                       lo,
                             int                       hi)
{
  int span, extent;

  value  = CLAMP(value, lo, hi);
  span   = hi - lo;
  extent = (orientation == wuss_SLIDER_HORIZONTAL)
         ? groove->x1 - groove->x0
         : groove->y1 - groove->y0;

  if (span == 0)
    return 0;

  return (int) ((long) (value - lo) * extent / span);
}

int wuss__slider_px_to_value(const box_t              *groove,
                             wuss_slider_orientation_t orientation,
                             point_t                   screen_point,
                             int                       lo,
                             int                       hi)
{
  int px, extent, span, value;

  extent = (orientation == wuss_SLIDER_HORIZONTAL)
         ? groove->x1 - groove->x0
         : groove->y1 - groove->y0;
  /* screen y grows downward but value grows upward for a vertical slider, so
   * pixel 0 is the groove's bottom (y1), mirroring HORIZONTAL's pixel 0 at
   * its left (x0) */
  px     = (orientation == wuss_SLIDER_HORIZONTAL)
         ? screen_point.x - groove->x0
         : groove->y1 - screen_point.y;
  span   = hi - lo;

  if (extent <= 0)
    return lo;

  px    = CLAMP(px, 0, extent);
  value = lo + (int) (((long) px * span + extent / 2) / extent);

  return CLAMP(value, lo, hi);
}

int wuss__slider_value_for_point(wuss_window_t     *window,
                                 const wuss_icon_t *icon,
                                 point_t            screen_point)
{
  box_t content, screen_box, groove;
  int   lo, hi, value;

  wuss__content_box(window, &content);
  wuss__icon_box_to_screen(&content, window->scroll, &icon->spec.bbox,
                           &screen_box);
  wuss__slider_groove_box(&screen_box, &groove);

  lo    = MIN(icon->spec.u.slider.min, icon->spec.u.slider.max);
  hi    = MAX(icon->spec.u.slider.min, icon->spec.u.slider.max);
  value = wuss__slider_px_to_value(&groove, icon->spec.u.slider.orientation,
                                   screen_point, lo, hi);

  /* min > max runs the fill backwards: the value nominally at the groove's
   * start (min) then sits at its far/high pixel end, so mirror the reading
   * around the [lo,hi] midpoint */
  if (icon->spec.u.slider.min > icon->spec.u.slider.max)
    value = lo + hi - value;

  return value;
}
