/* wuss/icon/get-value.c -- query a slider icon's current value */

#include <assert.h>

#include "../core/impl.h"

int wuss_icon_get_value(const wuss_icon_t *icon)
{
  assert(icon != NULL);

  return (icon->spec.type == wuss_ICON_TYPE_SLIDER) ? icon->value : 0;
}
