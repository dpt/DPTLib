/* wuss/icon/set-value.c -- set a slider icon's current value */

#include <assert.h>

#include "../core/impl.h"

void wuss__icon_set_value(wuss_window_t *window,
                          wuss_icon_t   *icon,
                          int            value)
{
  int lo, hi, clamped;

  assert(window != NULL);
  assert(icon   != NULL);

  if (icon->spec.type != wuss_ICON_TYPE_SLIDER)
    return;

  lo      = MIN(icon->spec.u.slider.min, icon->spec.u.slider.max);
  hi      = MAX(icon->spec.u.slider.min, icon->spec.u.slider.max);
  clamped = CLAMP(value, lo, hi);

  if (clamped == icon->value)
    return;

  icon->value = clamped;
  wuss__icon_invalidate(window, icon);
}

void wuss_icon_set_value(wuss_window_t *window,
                         wuss_icon_t   *icon,
                         int            value)
{
  wuss__icon_set_value(window, icon, value);
}
