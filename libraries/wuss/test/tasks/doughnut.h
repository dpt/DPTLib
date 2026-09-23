/* wuss/test/tasks/doughnut.h -- spinning ASCII-doughnut torus, in pixels */

#ifndef TASKS_DOUGHNUT_H
#define TASKS_DOUGHNUT_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* window task: Andy Sloane's "donut.c" torus render (the classic terminal
 * spinning-doughnut demo), ray-marched per pixel and shaded by
 * surface-normal brightness, redrawn every idle tick with the two rotation
 * angles advanced a little. Select toggles pause (and takes the input focus);
 * the wheel zooms in/out; an Adjust drag spins the torus directly, overriding
 * the idle auto-rotation for as long as it's held. While paused the arrow
 * keys step the rotation. */
typedef struct doughnut_task
{
  wuss_t            *wuss;     /* for wuss_get_pointer, opening the menu */
  wuss_task_t       *delegate; /* the task that owns the menu */
  wuss_window_t     *window;
  wuss_menu_handle_t menu_handle; /* live only between open and a pick */
  wuss_menu_item_t   menu_items[2]; /* per-instance: shared static "Info" row
                                      * would leak one instance's .window
                                      * pointer into another's menu */
  wuss_menu_t        menu;
  colour_t           bg;      /* background fill, picked from the shared
                                * colourmenu */
  double             a, b;    /* rotation angles about the x and z axes */
  double             zoom;    /* scales k1; wheel steps this, clamped */
  int                paused;  /* Select toggles; idle skips advancing a/b,
                               * arrow keys step them */
  int                dragging;   /* non-zero while an Adjust drag is live */
  int                drag_x, drag_y; /* last drag point, content space */
}
doughnut_task_t;

/* zoom range/step for the wheel: 1.0 is the original framing, larger zooms
 * in, smaller zooms out */
#define DOUGHNUT_ZOOM_MIN  0.3
#define DOUGHNUT_ZOOM_MAX  3.0
#define DOUGHNUT_ZOOM_STEP 0.1

wuss_window_fn_t doughnut_handle;

/* create the spinning-doughnut window against the given wuss instance; the
 * task block is allocated here, owned by the window, and freed when it
 * closes. if out is non-NULL, the task block is also returned through it */
result_t doughnut_create(wuss_t *wuss, doughnut_task_t **out);

/* free a task block allocated by doughnut_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void doughnut_destroy(doughnut_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_DOUGHNUT_H */
