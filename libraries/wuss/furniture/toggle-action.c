/* toggle-action.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

void wuss__furniture_toggle_size(wuss_window_t *window)
{
  box_t before, new_visible, dirty;

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

    width  = MIN(window->doc_width,  window->wuss->scr->width  - 2 * outline_px);
    height = MIN(window->doc_height, window->wuss->scr->height - 2 * outline_px - titlebar_height);

    window->pre_toggle = window->visible;

    new_visible.x0 = window->visible.x0;
    new_visible.y0 = window->visible.y0;
    new_visible.x1 = new_visible.x0 + width  + 2 * outline_px;
    new_visible.y1 = new_visible.y0 + height + titlebar_height + 2 * outline_px;
  }

  window->visible = new_visible;
  window->toggled = !window->toggled;

  wuss__furniture_scroll_step(window, (point_t) { 0, 0 }); /* re-clamp to the new content size */

  box_union(&before, &window->visible, &dirty);
  wuss__invalidate_clipped(window, &dirty);
}
