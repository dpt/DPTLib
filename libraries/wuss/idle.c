/* wuss/idle.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_idle(wuss_t *wuss)
{
  wuss_event_t event;
  result_t     rc;
  list_t      *e;

  event.kind = wuss_EVENT_IDLE;

  rc = result_OK;
  for (e = wuss->z_order.next; e != NULL; e = e->next)
  {
    wuss_window_t *win;
    result_t       crc;

    win = (wuss_window_t *) e;
    if (win->task.handle == NULL)
      continue;

    crc = win->task.handle(win, &event, win->task.task_data);
    if (crc != result_OK)
      rc = crc;
  }

  return rc;
}
