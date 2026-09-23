/* wuss/test/tasks/blank.h -- dithered colour-blending task */

#ifndef TASKS_BLANK_H
#define TASKS_BLANK_H

#ifdef WUSS_APP

#include "framebuf/pattern.h"
#include "utils/rng.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* window C's task: fills the window with an ordered-dither pattern that
 * blends from colour a to colour b over a fixed period, then picks a fresh
 * random b (the old b becoming a) and repeats; each frame's pattern comes
 * from pattern_from_colour against the system palette */
typedef struct blank_task
{
  wuss_t             *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[1]; /* per-instance: a shared static would
                                       * leak one instance's Info row .window
                                       * into another's menu */
  wuss_menu_t         menu;
  rng_t               rng;
  colour_t            a;           /* blend start */
  colour_t            b;           /* blend end */
  int                 frame_count; /* frames into the current blend */
  pattern_t           pattern;     /* this frame's fill */
}
blank_task_t;

wuss_window_fn_t blank_handle;

/* create the dithered colour-blending blank window against the given wuss instance;
 * if out is non-NULL, the task block is also returned through it */
result_t blank_create(wuss_t *wuss, blank_task_t **out);

/* free a task block allocated by blank_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void blank_destroy(blank_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_BLANK_H */
