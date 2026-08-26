/* set-scroll.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_set_scroll(wuss_window_t *window, int x, int y)
{
  box_t content, copied;
  int   dx, dy;

  dx = x - window->scroll_x;
  dy = y - window->scroll_y;
  if (dx == 0 && dy == 0)
    return;

  window->scroll_x = x;
  window->scroll_y = y;

  wuss__content_box(window, &content);

  /* the scrollbar thumb position depends on scroll_x/scroll_y, so its track
   * needs redrawing too -- content invalidation alone never touches it */
  wuss__furniture_invalidate(window);

  if (window->wuss->z_order.next == &window->link)
  {
    /* Topmost, so every pixel in "content" is genuinely this window's own
     * rendering: slide it by the scroll delta (clipped to the viewport, so
     * source and destination both stay inside "content") and only the
     * newly-exposed edge strip(s) need an actual repaint. */
    window->wuss->scr->clip = content;
    if (screen_copy_rect(window->wuss->scr, &content,
                         content.x0 - dx, content.y0 - dy, &copied))
    {
      wuss__invalidate_minus(window->wuss, &content, &copied);
      return;
    }
  }

  wuss__invalidate_clipped(window, &content);
}
