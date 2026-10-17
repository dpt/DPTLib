/* wuss/window/try-close.c -- veto-checked window close */

#include <stddef.h>

#include "wuss/task.h"

#include "../impl.h"

result_t wuss_window_try_close(wuss_window_t *window)
{
  result_t     rc;

  if (window == NULL)
    return result_OK;

  /* PRE_CLOSE: a non-OK return from the task vetoes the close; the window
   * stays open and no CLOSE is sent. */
  rc = wuss__deliver_kind(window->task, window, wuss_EVENT_PRE_CLOSE);
  if (rc != result_OK)
    return rc;

  if (window->wuss->focus == window)
    (void) wuss_set_focus(window->wuss, NULL);

  /* CLOSE while the window is still alive, then the unvetoable teardown. */
  (void) wuss__deliver_kind(window->task, window, wuss_EVENT_CLOSE);

  wuss_window_close(window);

  return result_OK;
}
