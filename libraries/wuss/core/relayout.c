/* wuss/relayout.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss__relayout_all(wuss_t *wuss)
{
  result_t       rc;
  wuss_window_t *win;
  wuss_window_t *next;

  rc = result_OK;

  for (win = wuss__z_first(wuss); win != NULL; win = next)
  {
    result_t     crc;
    wuss_event_t event;

    next = wuss__z_below(win); /* the handler may close this window */
    if (win->flags & wuss_WINDOW_PINNED)
      continue;

    event.kind = wuss_EVENT_RELAYOUT;
    crc = wuss__deliver(win->task, win, &event);
    if (crc != result_OK && rc == result_OK)
      rc = crc;
  }

  return rc;
}
