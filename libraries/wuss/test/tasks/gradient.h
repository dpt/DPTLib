/* wuss/test/tasks/gradient.h -- gradient fill task */

#ifndef TASKS_GRADIENT_H
#define TASKS_GRADIENT_H

#ifdef WUSS_APP

#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* fills its whole content area with a two-axis colour gradient; opens
 * small (100x100) against a large (400x400) document, so scrollbars
 * appear and the fill can be scrolled around. SELECT/ADJUST clicks cycle
 * the ordered-dither matrix forward/backward through 2x2, 4x4 and 8x8. */
typedef struct gradient_task
{
  wuss_t             *wuss;   /* borrowed; for wuss_get_font in the redraw */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[1]; /* per-instance: a shared static would
                                       * leak one instance's .window pointer
                                       * into another's menu */
  wuss_menu_t         menu;
  int                  dither_index;
}
gradient_task_t;

wuss_window_fn_t gradient_handle;

/* create the gradient window against the given wuss instance; if out is
 * non-NULL, the task block is also returned through it */
result_t gradient_create(wuss_t *wuss, gradient_task_t **out);

/* free a task block allocated by gradient_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void gradient_destroy(gradient_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_GRADIENT_H */
