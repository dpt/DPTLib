/* set-hover.c -- wuss - track which work-area icon the pointer is over */

#include <stddef.h>

#include "../impl.h"

void wuss__icon_set_hover(wuss_t *wuss, wuss_icon_t *icon)
{
  wuss_icon_t *prev;

  prev = wuss->hover_icon;
  if (prev == icon)
    return;

  if (prev != NULL)
  {
    prev->hovered = 0;
    wuss__icon_invalidate(prev);
  }

  wuss->hover_icon = icon;

  if (icon != NULL)
  {
    icon->hovered = 1;
    wuss__icon_invalidate(icon);
  }
}
