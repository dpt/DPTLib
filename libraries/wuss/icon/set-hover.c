/* wuss/icon/set-hover.c -- track which work-area icon the pointer is over */

#include <stddef.h>

#include "../core/impl.h"

/* Only wuss_ICON_TYPE_MENU_ENTRY changes appearance on hover (see
 * wuss__icon_draw_menu_entry). Buttons and every other type ignore the hover state,
 * so redrawing them on pointer enter/leave is wasted work. */
static int wuss__icon_hover_visible(const wuss_icon_t *icon)
{
  return icon->spec.type == wuss_ICON_TYPE_MENU_ENTRY;
}

void wuss__icon_set_hover(wuss_t        *wuss,
                          wuss_window_t *window,
                          wuss_icon_t   *icon)
{
  wuss_icon_t   *prev;
  wuss_window_t *prevwin;

  prev    = wuss->hover_icon;
  prevwin = wuss->hover_window;
  if (prev == icon)
    return;

  /* Every ancestor row on an open menu chain's open-submenu path keeps its
   * highlight while the pointer is down in a deeper level (RISC OS feel), so
   * don't drop `prev`'s highlight if it is one of those rows. */
#ifdef WUSS_MENUS
  if (prev != NULL && !wuss__menu_row_pinned(wuss, prev))
#else
  if (prev != NULL)
#endif
  {
    wuss__icon_set_state(prev, wuss_ICON_STATE_HOVERED, 0);
    if (wuss__icon_hover_visible(prev))
      wuss__icon_invalidate(prevwin, prev);
  }

  wuss->hover_icon   = icon;
  wuss->hover_window = (icon != NULL) ? window : NULL;

  if (icon != NULL)
  {
    wuss__icon_set_state(icon, wuss_ICON_STATE_HOVERED, 1);
    if (wuss__icon_hover_visible(icon))
      wuss__icon_invalidate(window, icon);
  }
}
