/* wuss/test/tasks/patterns.h -- dithered colour gradient task */

#ifndef TASKS_PATTERNS_H
#define TASKS_PATTERNS_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/component/saveas.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* window C's task: fills the window with a left-to-right gradient from
 * colour a to colour b in PATTERNS_STEP-pixel steps, each step an
 * ordered-dither pattern from pattern_from_colour against the system
 * palette. a and b start random and are chosen from the Colour A / Colour B
 * menu rows; Menu > Save PNG opens a Save As dialogue (drag its icon onto a
 * Filer window, or Save with a full path already typed) that writes the
 * window's content out */

typedef struct patterns_task
{
  wuss_t             *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_saveas_t       *saveas;
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[4]; /* per-instance: a shared static would
                                       * leak one instance's Info row .window
                                       * into another's menu */
  wuss_menu_t         menu;
  colour_t            a;            /* left edge */
  colour_t            b;            /* right edge */
  colour_t           *colourmenu_target; /* a or b: whichever row last
                                          * opened the colourmenu */
}
patterns_task_t;

wuss_window_fn_t patterns_handle;

/* create the dithered colour gradient patterns window against the given
 * wuss instance; if out is non-NULL, the task block is also returned
 * through it */
result_t patterns_create(wuss_t *wuss, patterns_task_t **out);

/* free a task block allocated by patterns_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void patterns_destroy(patterns_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_PATTERNS_H */
