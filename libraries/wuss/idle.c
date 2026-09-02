/* wuss/idle.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_idle(wuss_t *wuss)
{
  wuss_event_t event;
  result_t     rc;
  list_t      *e;

  event.kind = wuss_EVENT_IDLE;

  rc = result_OK;
  for (e = wuss->tasks.next; e != NULL; e = e->next)
  {
    wuss_task_t *task;
    result_t     crc;

    task = (wuss_task_t *) e;

    crc = wuss__deliver(task, NULL, &event);
    if (crc != result_OK && rc == result_OK)
      rc = crc;
  }

  return rc;
}
