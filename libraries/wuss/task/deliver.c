/* wuss/task/deliver.c -- the single event-dispatch chokepoint */

#include <assert.h>
#include <stddef.h>

#include "wuss/task.h"

#include "../impl.h"

#ifndef NDEBUG
/* Is "kind" valid for delivery to a window (win != NULL) or to a task
 * (win == NULL)? The two sets are the wuss_window_event_kind_t /
 * wuss_task_event_kind_t views documented in task.h. */
static int wuss__kind_ok_for(wuss_event_kind_t kind, int have_window)
{
  switch (kind)
  {
  case wuss_EVENT_REDRAW:
  case wuss_EVENT_MOUSE:
  case wuss_EVENT_SCROLL:
  case wuss_EVENT_OPEN:
  case wuss_EVENT_PRE_SHOW:
  case wuss_EVENT_SHOW:
  case wuss_EVENT_PRE_CLOSE:
  case wuss_EVENT_CLOSE:
    return have_window;

  case wuss_EVENT_IDLE:
  case wuss_EVENT_QUIT:
  case wuss_EVENT_PALETTE:
  case wuss_EVENT_MENU_SELECT:
    return !have_window;

  case wuss_EVENT_ICON:
    return 1; /* window view: real icon; task view: reserved shared element */
  }

  return 0;
}
#endif

result_t wuss__deliver(wuss_task_t        *task,
                       wuss_window_t      *win_or_null,
                       const wuss_event_t *ev)
{
  if (task == NULL || task->handle == NULL)
    return result_OK;

#ifndef NDEBUG
  assert(wuss__kind_ok_for(ev->kind, win_or_null != NULL));
#endif

  return task->handle(win_or_null, ev, task->task_data);
}
