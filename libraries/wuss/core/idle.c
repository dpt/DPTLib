/* wuss/idle.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_idle(wuss_t *wuss)
{
  result_t     rc;
  wuss_event_t event;
  list_t      *e;

  event.kind = wuss_EVENT_IDLE;

  wuss__message_enter(wuss);
  wuss__message_leave(wuss); /* drain before IDLE, per the file overview */
  wuss__message_enter(wuss);

#ifdef WUSS_FURNITURE
  wuss__scroll_repeat(wuss);
#endif

  if (wuss->drag_window != NULL)
  {
    wuss->drag_frame++;
    wuss_invalidate(wuss, &wuss->drag_box); /* re-paint the ants for the new phase */
  }

  rc = result_OK;
  for (e = wuss->tasks.next; e != NULL; )
  {
    result_t     crc;
    wuss_task_t *task;
    list_t      *next;

    task = (wuss_task_t *) e;
    next = e->next; /* an autoclose task may free its own node in the handler */

    crc = wuss__deliver(task, NULL, &event);
    if (crc != result_OK && rc == result_OK)
      rc = crc;

    e = next;
  }

  wuss__message_leave(wuss); /* drain after IDLE too */

  return rc;
}
