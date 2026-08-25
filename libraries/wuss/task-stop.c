/* task-stop.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_task_stop(wuss_window_t *window)
{
  wuss_event_t event;

  if (window->task.handle == NULL)
    return result_OK;

  event.kind = wuss_EVENT_QUIT;

  return window->task.handle(window, &event, window->task.task_data);
}
