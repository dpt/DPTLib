/* wuss/test/tasks/display.h -- desktop display mode picker task */

#ifndef TASKS_DISPLAY_H
#define TASKS_DISPLAY_H

#ifdef WUSS_APP

#include "wuss/gadget/stringset.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* window D's task: a small window, after RISC OS's Display Manager, with two
 * string sets -- colour depth and resolution -- showing the current mode.
 * Change calls app_set_mode with the picks, which live-changes the frontend's
 * surface and wuss's own screen, nudging every open window back on-screen and
 * shrinking any that no longer fit, then closes the window (Adjust leaves it
 * open). Cancel closes it, discarding the picks; Adjust-Cancel instead reverts
 * them to the mode in force. */
typedef struct display_task
{
  wuss_t             *wuss;
  wuss_task_t        *delegate;
  wuss_window_t      *window;
  wuss_stringset_t   *colours;
  wuss_stringset_t   *resolution;
  wuss_icon_t        *cancel;
  wuss_icon_t        *change;
}
display_task_t;

wuss_window_fn_t display_handle;

/* create the display-mode window against the given wuss instance. if
 * out is non-NULL, the task block is also returned through it */
result_t display_create(wuss_t *wuss, display_task_t **out);

/* install the icon bar app that opens display windows on demand */
result_t display_app_create(wuss_t *wuss);

/* free a task block allocated by display_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void display_destroy(display_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_DISPLAY_H */
