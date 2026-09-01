/* wuss/scroll.c -- wuss - minimal window manager */

#include "impl.h"

/* Scrolling moves content under a stationary pointer, so the icon the pointer
 * now sits over may differ from before. Re-resolve the hovered icon so a
 * highlighted wuss_ICON_TYPE_MENU_ENTRY does not keep its highlight after
 * scrolling out from under the pointer. */
static void wuss__scroll_rehover(wuss_t        *wuss,
                                 wuss_window_t *win,
                                 point_t        screen_point)
{
#ifdef WUSS_ICONS
  box_t   content;
  point_t doc_point;

  wuss__content_box(win, &content);
  doc_point.x = screen_point.x - content.x0 + win->scroll.x;
  doc_point.y = screen_point.y - content.y0 + win->scroll.y;

  wuss__icon_set_hover(wuss, wuss__icon_hit_test(win, doc_point));
#else
  (void) wuss;
  (void) win;
  (void) screen_point;
#endif
}

result_t wuss_scroll(wuss_t *wuss, point_t p, int delta, wuss_window_t **hit)
{
  wuss_window_t *win;
  int            x, y;

  x = p.x;
  y = p.y;

  win = wuss__window_at(wuss, p);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

#ifdef WUSS_FURNITURE
  {
    box_t titlebar;

    wuss__titlebar_box(win, &titlebar);
    if (box_contains_point(&titlebar, x, y))
      return result_OK;
  }
#endif

  if (win->task.handle != NULL)
  {
    box_t        content;
    wuss_event_t event;

    /* Note the pointer position before scrolling, so the event describes the
     * content the pointer was over when the wheel turned, not the content the
     * step below brings under it. */
    wuss__content_box(win, &content);
    event.kind                = wuss_EVENT_SCROLL;
    event.data.scroll.point.x = x - content.x0 + win->scroll.x;
    event.data.scroll.point.y = y - content.y0 + win->scroll.y;
    event.data.scroll.delta   = delta;

    wuss__scroll_step(win, POINT(0, delta));
    wuss__scroll_rehover(wuss, win, POINT(x, y));

    return win->task.handle(win, &event, win->task.task_data);
  }

  wuss__scroll_step(win, POINT(0, delta));
  wuss__scroll_rehover(wuss, win, POINT(x, y));

  return result_OK;
}
