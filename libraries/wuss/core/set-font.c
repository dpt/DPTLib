/* wuss/set-font.c -- replace a font slot mid-session */

#include <assert.h>

#include "geom/box.h"

#include "impl.h"

#ifdef WUSS_FURNITURE
/* Keep "window"'s content size across a titlebar height change: its visible
 * box grows or shrinks from the top-left by however much the chrome (the
 * titlebar itself, and the scrollbar/resize carve sized from it) did. */
static void regrow(wuss_window_t *window, int old_height)
{
  wuss_t *wuss;
  int     new_height;
  box_t   before;
  box_t   after;
  int     dx, dy;

  wuss       = window->wuss;
  new_height = wuss->titlebar_height;

  wuss->titlebar_height = old_height;
  wuss__content_box(window, &before);
  wuss->titlebar_height = new_height;
  wuss__content_box(window, &after);

  dx = (before.x1 - before.x0) - (after.x1 - after.x0);
  dy = (before.y1 - before.y0) - (after.y1 - after.y0);

  window->visible.x1 += dx;
  window->visible.y1 += dy;
  if (wuss__window_toggled(window))
  {
    window->pre_toggle.x1 += dx;
    window->pre_toggle.y1 += dy;
  }

  wuss__nudge_visible_onscreen(window);
  wuss__chrome_invalidate_layout(window);
}
#endif

result_t wuss_set_font(wuss_t *wuss, int index, const wuss_font_desc_t *desc)
{
#ifdef WUSS_FURNITURE
  int            old_height;
  int            auto_height;
  wuss_window_t *win;
#endif

  assert(wuss != NULL);
  assert(desc != NULL);

  if (index < 0 || index >= wuss_MAX_FONTS || desc->font == NULL)
    return result_BAD_ARG;

#ifdef WUSS_FURNITURE
  old_height  = wuss->titlebar_height;
  auto_height = wuss__titlebar_height_auto(wuss);
#endif

  wuss__fontset_set(&wuss->fonts, index, desc);

#ifdef WUSS_FURNITURE
  /* ponytail: an explicit config height that happens to equal the derived
   * one is taken as derived; store a flag if that ever matters */
  if (old_height == auto_height)
  {
    wuss->titlebar_height = wuss__titlebar_height_auto(wuss);
    if (wuss->titlebar_height != old_height)
      for (win = wuss__z_first(wuss); win != NULL; win = wuss__z_below(win))
        regrow(win, old_height);
  }
#endif

  wuss__invalidate_screen(wuss);

  return wuss__relayout_all(wuss);
}
