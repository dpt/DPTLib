/* wuss/test/tasks/minesweeper.h -- minesweeper task */

#ifndef TASKS_MINESWEEPER_H
#define TASKS_MINESWEEPER_H

#ifdef WUSS_APP

#include <stdbool.h>
#include <time.h>

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

#define MINESWEEPER_CELL 16 /* pixels per cell */

/* board sizes offered on the "Grid Size" menu; arrays are sized for the
 * largest so a resize just changes ms->rows/cols/mines and re-clears */
#define MINESWEEPER_MAX_ROWS 24
#define MINESWEEPER_MAX_COLS 24

typedef enum minesweeper_size
{
  minesweeper_SIZE_24X24,
  minesweeper_SIZE_24X12,
  minesweeper_SIZE_16X16,
  minesweeper_SIZE_16X12,
  minesweeper_SIZE_12X12,
  minesweeper_NSIZES
}
minesweeper_size_t;

typedef enum minesweeper_cell_state
{
  minesweeper_HIDDEN,
  minesweeper_REVEALED,
  minesweeper_FLAGGED
}
minesweeper_cell_state_t;

/* classic minesweeper: Select reveals a cell (flood-filling neighbouring
 * zeros), Adjust toggles a flag. Mines are placed on the first reveal so the
 * opening click is never a mine. A MENU-button click pops a menu with "New
 * Game" and a "Grid Size" submenu that resets the board at a new size. */
typedef struct minesweeper_task
{
  wuss_t                  *wuss;
  wuss_window_t           *window;
  wuss_task_t             *task; /* delegate; opens the New Game menu */
  wuss_menu_handle_t       menu_handle; /* live only between open and a pick */
  wuss_menu_item_t         size_items[minesweeper_NSIZES];
  wuss_menu_t              size_menu;
  wuss_menu_item_t         menu_items[3]; /* per-instance: a shared static
                                            * would leak one instance's
                                            * .window pointer into another's
                                            * menu */
  wuss_menu_t              menu;
  bmfont_t                *font; /* borrowed; draws the neighbour counts */
  minesweeper_size_t       size;
  int                      rows, cols, mines;
  bool                     mine[MINESWEEPER_MAX_ROWS][MINESWEEPER_MAX_COLS];
  minesweeper_cell_state_t state[MINESWEEPER_MAX_ROWS][MINESWEEPER_MAX_COLS];
  bool                     placed;  /* mines placed yet? */
  bool                     dead;    /* a mine was revealed */
  bool                     won;
  int                      flags;   /* flagged cell count, for the counter */
  time_t                   start_time; /* set on first reveal */
  int                      elapsed; /* seconds, frozen on dead/won */
}
minesweeper_task_t;

wuss_window_fn_t minesweeper_handle;

/* create the minesweeper window against the given wuss instance, lettering
 * neighbour counts with wuss's own bold font (wuss_get_font_n(wuss, 1)).
 * if out is non-NULL, the task block is also returned through it */
result_t minesweeper_create(wuss_t *wuss, minesweeper_task_t **out);

/* free a task block allocated by minesweeper_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void minesweeper_destroy(minesweeper_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_MINESWEEPER_H */
