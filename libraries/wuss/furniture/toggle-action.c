/* toggle-action.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

void wuss__furniture_toggle_size(wuss_window_t *window)
{
  box_t before, new_visible, dirty, copied;

  before = window->visible;

  if (window->toggled)
  {
    new_visible = window->pre_toggle;
  }
  else
  {
    int     outline_px, titlebar_height, width, height;
    point_t carve;

    outline_px      = wuss__outline_px(window);
    titlebar_height = wuss__titlebar_height(window);
    wuss__furniture_carve_for(window->flags, wuss__icon_size(window), &carve);

    /* bounded by what's actually left of the screen from the window's
     * current top-left, not the screen's full width/height -- otherwise a
     * window not already at the origin grows straight off the edge. Also
     * account for scrollbar/resize-icon furniture (carve), which sits
     * outside the content area same as create.c/resize.c do, or the window
     * ends up carve.x/carve.y short of the doc size it's meant to reach. */
    width  = MIN(window->doc_width,  window->wuss->scr->width  - window->visible.x0 - 2 * outline_px - carve.x);
    height = MIN(window->doc_height, window->wuss->scr->height - window->visible.y0 - 2 * outline_px - titlebar_height - carve.y);

    window->pre_toggle = window->visible;

    new_visible.x0 = window->visible.x0;
    new_visible.y0 = window->visible.y0;
    new_visible.x1 = new_visible.x0 + width  + 2 * outline_px + carve.x;
    new_visible.y1 = new_visible.y0 + height + titlebar_height + 2 * outline_px + carve.y;
  }

  window->visible = new_visible;
  window->toggled = !window->toggled;

  wuss__furniture_scroll_step(window, (point_t) { 0, 0 }); /* re-clamp to the new content size */

  if (window->wuss->z_order.next == &window->link &&
      screen_copy_rect(window->wuss->scr, &before,
                       (point_t) { before.x0, before.y0 }, &copied))
  {
    /* Topmost, and the screen format supports the blit: the window's
     * top-left never moves for a toggle, so re-blitting "before" onto
     * itself is a no-op that just confirms which of its pixels are still
     * on-screen -- only whatever's newly exposed (grown) or newly vacated
     * (shrunk) relative to that needs an actual repaint. */
    wuss__invalidate_minus(window->wuss, &before, &window->visible);
    wuss__invalidate_minus(window->wuss, &window->visible, &copied);

    /* Unlike a move, a toggle changes the window's size, so furniture that
     * lays itself out relative to that size (titlebar icons anchored to its
     * right edge, scrollbar track/thumb proportions, the resize corner)
     * reflows even where the blit reused valid content pixels -- e.g. the
     * old toggle icon location is now mid-titlebar, not redrawn by either
     * invalidate_minus above since it falls inside both "before" and the
     * new "visible". Force it dirty regardless of the blit. */
    wuss__furniture_invalidate(window);
  }
  else
  {
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
}
