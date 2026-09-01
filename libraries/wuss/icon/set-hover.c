/* wuss/icon/set-hover.c -- track which work-area icon the pointer is over */

#include <stddef.h>

#include "../impl.h"

/* Only wuss_ICON_TYPE_MENU_ENTRY changes appearance on hover (see
 * wuss__icon_draw_menu_entry). Buttons and every other type ignore the hover state,
 * so redrawing them on pointer enter/leave is wasted work. */
static int wuss__icon_hover_visible(const wuss_icon_t *icon)
{
  return icon->type == wuss_ICON_TYPE_MENU_ENTRY;
}

void wuss__icon_set_hover(wuss_t *wuss, wuss_icon_t *icon)
{
  wuss_icon_t *prev;

  prev = wuss->hover_icon;
  if (prev == icon)
    return;

  if (prev != NULL)
  {
    wuss__icon_set_state(prev, wuss_ICON_STATE_HOVERED, 0);
    if (wuss__icon_hover_visible(prev))
      wuss__icon_invalidate(prev);
  }

  wuss->hover_icon = icon;

  if (icon != NULL)
  {
    wuss__icon_set_state(icon, wuss_ICON_STATE_HOVERED, 1);
    if (wuss__icon_hover_visible(icon))
      wuss__icon_invalidate(icon);
  }
}
