/* wuss/test/tasks/checker.h -- checkerboard task */

#ifndef TASKS_CHECKER_H
#define TASKS_CHECKER_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* cycled by a content click, in this order */
typedef enum checker_pattern
{
  checker_PATTERN_CHECKERBOARD,
  checker_PATTERN_HORIZONTAL,
  checker_PATTERN_VERTICAL,
  checker_PATTERN_DIAGONAL,
  checker_PATTERN__COUNT
}
checker_pattern_t;

/* fills the whole content area with a black and white two-tone pattern;
 * each window cycles its own pattern independently on a content click */
typedef struct checker_task
{
  wuss_t             *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t         *delegate; /* the task that owns the menu */
  wuss_window_t       *window, *window2;
  wuss_menu_handle_t   menu_handle; /* live only between open and a pick */
  wuss_proginfo_t     *proginfo;
  wuss_menu_item_t     menu_items[1]; /* per-instance: a shared static would
                                        * leak one instance's proginfo window
                                        * pointer into another's menu */
  wuss_menu_t          menu;
  colour_t             black, white;
  checker_pattern_t    pattern, pattern2;
  int                  band, band2; /* pixels per band, scroll-adjustable */
}
checker_task_t;

wuss_window_fn_t checker_handle;

/* create the two checkerboard windows against the given wuss instance;
 * if out is non-NULL, the task block is also returned through it */
result_t checker_create(wuss_t *wuss, checker_task_t **out);

/* free a task block allocated by checker_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void checker_destroy(checker_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CHECKER_H */
