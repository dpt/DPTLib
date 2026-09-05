/* wuss/window/set-doc.c -- wuss - minimal window manager */

#include "../impl.h"

result_t wuss_window_set_doc(wuss_window_t *window, size2d_t doc)
{
  box_t   content;
  point_t clamped;

  if (!wuss__size_ok(doc.w, doc.h))
    return result_WUSS_TOO_SMALL;

  window->doc = doc;

  clamped = wuss__scroll_clamp(window, window->scroll);
  window->scroll = clamped;

  /* the scrollbar sausage size depends on doc, so its well needs redrawing
   * too -- content invalidation alone never touches it */
  wuss__chrome_repaint(window);

  wuss__content_box(window, &content);
  wuss__invalidate_clipped(window, &content);

  return result_OK;
}
