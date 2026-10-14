/* wuss/window/close.c -- wuss - minimal window manager */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

void wuss_window_close(wuss_window_t *doomed)
{
  wuss_task_t *task;
  wuss_event_t event;
  wuss_t      *wuss;

  if (doomed == NULL)
    return;

  wuss = doomed->wuss;
  task = doomed->task;

  wuss__menu_window_closing(wuss, doomed);

  /* Bounce anything still queued for this window now, while it is alive:
   * left queued, it would be read after the free below, or delivered to
   * whatever window a later wuss_window_create allocates at this address. */
  wuss__message_purge_window(wuss, doomed);

#ifdef WUSS_FURNITURE
  if (wuss->furniture.dragging == doomed)
  {
    wuss->furniture.dragging       = NULL;
    wuss->furniture.drag_kind      = wuss_FURNITURE_DRAG_NONE;
    wuss->furniture.pressed_region = wuss_FURNITURE_NONE;
  }
#endif
#ifdef WUSS_ICONS
  if (wuss->pressed.window == doomed)
  {
    wuss->pressed.icon   = NULL;
    wuss->pressed.window = NULL;
  }
  if (wuss->hover.window == doomed)
  {
    wuss->hover.icon   = NULL;
    wuss->hover.window = NULL;
  }
  if (wuss->caret.window == doomed)
  {
    wuss->caret.icon   = NULL;
    wuss->caret.window = NULL;
  }
#endif
  /* No wuss_EVENT_POINTER_EXIT: the task hears about this window going away
   * through PRE_CLOSE/CLOSE, and the struct is freed below. */
  wuss__pointer_forget_window(wuss, doomed);
  wuss__focus_forget_window(wuss, doomed);
  wuss__drag_forget_window(wuss, doomed);
  wuss__capture_forget_window(wuss, doomed);

  /* Stop double-click detection matching a freed window: a later
   * wuss_window_create could reuse this address, giving a false match.
   * Reset the button too: NULL alone would match a backdrop press. */
  if (wuss->last_down.window == doomed)
  {
    wuss->last_down.window = NULL;
    wuss->last_down.button = wuss_BUTTON_NONE;
  }

  /* A hidden window was never actually drawn at "visible" -- whatever is
   * genuinely on screen there (backdrop, or another window's content) is
   * already correct and showing through, so invalidating it would force a
   * pointless repaint of somebody else's pixels. */
  if (!(doomed->flags & wuss_WINDOW_HIDDEN))
    wuss__invalidate_clipped(doomed, &doomed->visible);

  list_remove(&wuss->z_order[doomed->stack], &doomed->link);
  list_remove(&task->windows, &doomed->task_link);

#ifdef WUSS_ICONS
  wuss__icons_free(doomed);
#endif

  wuss__free(wuss, doomed);

  /* An autoclose task self-destructs once it loses its last window: QUIT
   * (so the handler can free task_data), then unlink and free the node.
   * Suppressed while wuss_task_destroy is already tearing the task down. */
  if ((task->flags & wuss_TASK__AUTOCLOSE) &&
      !(task->flags & wuss_TASK__REAPING)  &&
      task->windows.next == NULL)
  {
    /* As wuss_task_destroy: a chain this task still owns would outlive it,
     * so tear it down (delivering MENU_CLOSED) before QUIT. */
    wuss__menu_task_closing(wuss, task);

    event.kind = wuss_EVENT_QUIT;
    (void) wuss__deliver(task, NULL, &event);

    list_remove(&wuss->tasks, &task->link);
    wuss__free(wuss, task);
  }
}
