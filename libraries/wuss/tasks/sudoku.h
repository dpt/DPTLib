/* wuss/tasks/sudoku.h -- sudoku task */

#ifndef TASKS_SUDOKU_H
#define TASKS_SUDOKU_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/bmfont.h"
#include "utils/rng.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* a 9x9 sudoku with a unique solution, generated afresh for each game.
 * Select on a cell moves the cursor there; Select on the cursor cell steps
 * its digit up, Adjust steps it down. With the input focus, the arrow keys
 * move the cursor, 1-9 enter a digit and 0, Space, Backspace or Delete clear
 * it. Entries that clash with another digit in their row, column or box are
 * drawn in red; the board turns green once solved. A MENU-button click pops
 * a menu with "New game" (N) and "Restart" (R), which clears every entry */
typedef struct sudoku_task
{
  wuss_t             *wuss;
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[2]; /* per-instance, as checker's */
  wuss_menu_t         menu;
  bmfont_t           *font; /* borrowed; wuss's bold font */
  rng_t               rng;
  unsigned char       solution[9][9];
  unsigned char       cell[9][9]; /* 0 = empty */
  bool                given[9][9];
  int                 cur_r, cur_c; /* cursor cell */
  bool                solved;
}
sudoku_task_t;

wuss_window_fn_t sudoku_handle;

/* create the sudoku window against the given wuss instance;
 * if out is non-NULL, the task block is also returned through it */
result_t sudoku_create(wuss_t *wuss, sudoku_task_t **out);

/* install the icon bar app that opens sudoku windows on demand */
result_t sudoku_app_create(wuss_t *wuss);

/* free a task block allocated by sudoku_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void sudoku_destroy(sudoku_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_SUDOKU_H */
