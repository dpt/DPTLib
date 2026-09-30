/* wuss/test/tasks/checker.h -- checkerboard task */

#ifndef TASKS_CHECKER_H
#define TASKS_CHECKER_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/component/saveas.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* cycled by a content click, in this order, or picked from Menu > Pattern */
typedef enum checker_pattern
{
  checker_PATTERN_CHECKERBOARD,
  checker_PATTERN_HORIZONTAL,
  checker_PATTERN_VERTICAL,
  checker_PATTERN_DIAGONAL,
  checker_PATTERN__COUNT
}
checker_pattern_t;

/* fills the whole content area with a two-tone pattern, black and white
 * until Menu > Ink/Paper recolours both windows (Menu > Swap colours
 * exchanges the two); each window cycles its own pattern independently,
 * forward on a Select click and back on Adjust, or picks it from
 * Menu > Pattern; Menu > Save PNG opens a Save As dialogue (drag its icon
 * onto a Filer window, or Save with a full path already typed) that writes
 * that window out. Once a click has given a window the input focus,
 * Left/Right step its pattern and Up/Down widen/narrow its bands */
typedef struct checker_task
{
  wuss_t             *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t         *delegate; /* the task that owns the menu */
  wuss_task_t         *saveas_task; /* owns the Save As dialogue; separate
                                     * from delegate, which is autoclose and
                                     * wuss_saveas_create forbids that */
  wuss_saveas_t       *saveas;
  wuss_window_t       *window, *window2;
  wuss_menu_handle_t   menu_handle; /* live only between open and a pick */
  wuss_menu_item_t     menu_items[6]; /* per-instance: a shared static would
                                        * leak one instance's .window pointer
                                        * into another's menu */
  wuss_menu_t          menu;
  wuss_menu_item_t     pattern_items[checker_PATTERN__COUNT];
  wuss_menu_t          pattern_menu;
  wuss_window_t       *menu_window; /* window the menu was opened from; the
                                     * Pattern submenu retargets it */
  colour_t             black, white;
  colour_t            *colourmenu_target; /* black or white: whichever row
                                            * last opened the colourmenu */
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
