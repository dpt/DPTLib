/* wuss/tasks/checker/checker.h -- checkerboard task */

#ifndef TASKS_CHECKER_H
#define TASKS_CHECKER_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "utils/rng.h"
#include "wuss/component/colourmenu.h"
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
 * until Menu > Colours recolours it (Menu > Swap colours exchanges the
 * two); the pattern cycles forward on a Select click and back on Adjust, or
 * is picked from Menu > Pattern; Menu > Save opens a Save As dialogue
 * (drag its icon onto a Filer window, or Save with a full path already
 * typed) that writes the window out; Menu > Randomise (^R) picks a pattern,
 * band width and colours afresh and Menu > Reset (R) restores the starting
 * ones. Once a click has given the window the
 * input focus, Left/Right step the pattern and Up/Down widen/narrow its
 * bands */
typedef struct checker_task
{
  wuss_t             *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t         *delegate; /* the task that owns the menu */
  wuss_saveas_t       *saveas;
  wuss_window_t       *window;
  wuss_menu_handle_t   menu_handle; /* live only between open and a pick */
  wuss_menu_item_t     menu_items[6]; /* per-instance: a shared static would
                                        * leak one instance's .window pointer
                                        * into another's menu */
  wuss_menu_t          menu;
  wuss_menu_item_t     pattern_items[checker_PATTERN__COUNT];
  wuss_menu_t          pattern_menu;
  colour_t             black, white;
  wuss_colourlist_t    colours; /* Ink/Paper submenu */
  checker_pattern_t    pattern;
  int                  band; /* pixels per band, scroll-adjustable */
  rng_t                rng;  /* Randomise */
}
checker_task_t;

wuss_window_fn_t checker_handle;

/* create the checkerboard window against the given wuss instance;
 * if out is non-NULL, the task block is also returned through it */
result_t checker_create(wuss_t *wuss, checker_task_t **out);

/* install the icon bar app that opens checker windows on demand */
result_t checker_app_create(wuss_t *wuss);

/* free a task block allocated by checker_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void checker_destroy(checker_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CHECKER_H */
