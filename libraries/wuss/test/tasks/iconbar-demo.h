/* wuss/test/tasks/iconbar-demo.h -- icon bar demo task */

#ifndef TASKS_ICONBAR_DEMO_H
#define TASKS_ICONBAR_DEMO_H

#if defined(WUSS_APP) && defined(WUSS_ICONBAR)

#include "wuss/iconbar.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* Adds three icons ("A", "B", "C") to the icon bar; clicking one opens a
 * small window naming which icon was clicked, or closes it again if it is
 * already open for that icon -- the classic RISC OS "click the icon bar
 * icon to open/close the app's window" pattern. */
typedef struct iconbar_demo_task
{
  wuss_t               *wuss;
  wuss_task_t           *delegate;
  wuss_iconbar_icon_t   *icons[3];
  wuss_window_t         *window;     /* NULL when closed */
  int                    open_index; /* icon that opened window, or -1 */
}
iconbar_demo_task_t;

wuss_window_fn_t iconbar_demo_handle;

/* create the demo's three icon bar icons and register their owning task
 * against the given wuss instance; if out is non-NULL, the task block is
 * also returned through it */
result_t iconbar_demo_create(wuss_t *wuss, iconbar_demo_task_t **out);

/* free a task block allocated by iconbar_demo_create; normally called by
 * the task's wuss_EVENT_QUIT handler, not by callers directly */
void iconbar_demo_destroy(iconbar_demo_task_t *task);

#endif /* WUSS_APP && WUSS_ICONBAR */

#endif /* TASKS_ICONBAR_DEMO_H */
