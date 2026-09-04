/* wuss/window/try-close.c -- veto-checked window close */

#include <stddef.h>

#include "wuss/task.h"

#include "../impl.h"

result_t wuss_window_try_close(wuss_window_t *window)
{
  wuss_event_t event;
  result_t     rc;

  if (window == NULL)
    return result_OK;

  /* PRE_CLOSE: a non-OK return from the task vetoes the close; the window
   * stays open and no CLOSE is sent. */
  event.kind = wuss_EVENT_PRE_CLOSE;
  rc = wuss__deliver(window->task, window, &event);
  if (rc != result_OK)
    return rc;

  /* CLOSE while the window is still alive, then the unvetoable teardown. */
  event.kind = wuss_EVENT_CLOSE;
  (void) wuss__deliver(window->task, window, &event);

  wuss_window_close(window);

  return result_OK;
}
