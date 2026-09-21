/* wuss/test/tasks/donut.h -- spinning ASCII-donut torus, in pixels */

#ifndef TASKS_DONUT_H
#define TASKS_DONUT_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* window task: Andy Sloane's "donut.c" torus render (the classic terminal
 * spinning-donut demo), ray-marched per pixel and shaded by surface-normal
 * brightness, redrawn every idle tick with the two rotation angles advanced
 * a little. Select toggles pause; the wheel zooms in/out. */
typedef struct donut_task
{
  wuss_t            *wuss;     /* for wuss_get_pointer, opening the menu */
  wuss_task_t       *delegate; /* the task that owns the menu */
  wuss_window_t     *window;
  wuss_menu_handle_t menu_handle; /* live only between open and a pick */
  wuss_menu_item_t   menu_items[1]; /* per-instance: shared static "Info" row
                                      * would leak one instance's .window
                                      * pointer into another's menu */
  wuss_menu_t        menu;
  double             a, b;    /* rotation angles about the x and z axes */
  double             zoom;    /* scales k1; wheel steps this, clamped */
  int                paused;  /* Select toggles; idle skips advancing a/b */
}
donut_task_t;

/* zoom range/step for the wheel: 1.0 is the original framing, larger zooms
 * in, smaller zooms out */
#define DONUT_ZOOM_MIN  0.3
#define DONUT_ZOOM_MAX  3.0
#define DONUT_ZOOM_STEP 0.1

wuss_window_fn_t donut_handle;

/* create the spinning-donut window against the given wuss instance; the task
 * block is allocated here, owned by the window, and freed when it closes.
 * if out is non-NULL, the task block is also returned through it */
result_t donut_create(wuss_t *wuss, donut_task_t **out);

/* free a task block allocated by donut_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void donut_destroy(donut_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_DONUT_H */
