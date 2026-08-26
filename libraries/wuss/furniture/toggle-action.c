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
    int outline_px, titlebar_height, width, height;

    outline_px      = wuss__outline_px(window);
    titlebar_height = wuss__titlebar_height(window);

    /* bounded by what's actually left of the screen from the window's
     * current top-left, not the screen's full width/height -- otherwise a
     * window not already at the origin grows straight off the edge */
    width  = MIN(window->doc_width,  window->wuss->scr->width  - window->visible.x0 - 2 * outline_px);
    height = MIN(window->doc_height, window->wuss->scr->height - window->visible.y0 - 2 * outline_px - titlebar_height);

    window->pre_toggle = window->visible;

    new_visible.x0 = window->visible.x0;
    new_visible.y0 = window->visible.y0;
    new_visible.x1 = new_visible.x0 + width  + 2 * outline_px;
    new_visible.y1 = new_visible.y0 + height + titlebar_height + 2 * outline_px;
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
  }
  else
  {
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
}
