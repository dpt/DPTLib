/* wuss/test/tasks/swatches.h -- fill-pattern swatch grid task */

#ifndef TASKS_SWATCHES_H
#define TASKS_SWATCHES_H

#ifdef WUSS_APP

/* ponytail: no #ifdef WUSS_COMPONENTS guard -- it is PUBLIC on DPTLib and ON
 * by default, so the wuss app always has the colourmenu component. */
#include "wuss/component/colourmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* Grid of every (fill pattern, palette colour) pair: one row per built-in
 * screen fill pattern, one column per system-palette entry, each cell a
 * PATTERN icon packed edge to edge. The document is taller than the window
 * so the grid scrolls through it. A MENU-button click pops the task's own
 * menu ("Info", "Colour"); "Colour" leads to a wuss_colourmenu whose pick
 * becomes the paper the patterns mix over (white until then). */

typedef struct swatches_task
{
  wuss_t             *wuss;
  wuss_window_t      *window;
  wuss_task_t        *task;      /* delegate; opens the menu */
  wuss_menu_item_t    menu_items[2]; /* per-instance: a shared static would
                                       * leak one instance's .window pointer
                                       * into another's menu */
  wuss_menu_t         menu;
  wuss_menu_handle_t  menu_handle;
  wuss_colour_t       paper;     /* pattern bg; the "mixing" colour */
}
swatches_task_t;

wuss_window_fn_t swatches_handle;

/* create the swatches window against the given wuss instance; if out is
 * non-NULL, the task block is also returned through it */
result_t swatches_create(wuss_t *wuss, swatches_task_t **out);

/* free a task block allocated by swatches_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void swatches_destroy(swatches_task_t *task);


#endif /* WUSS_APP */

#endif /* TASKS_SWATCHES_H */
