/* wuss/test/tasks/minesweeper.h -- minesweeper task */

#ifndef TASKS_MINESWEEPER_H
#define TASKS_MINESWEEPER_H

#ifdef WUSS_APP

#include <stdbool.h>
#include <time.h>

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/task.h"
#include "wuss/window.h"

#define MINESWEEPER_COLS 12
#define MINESWEEPER_ROWS 12
#define MINESWEEPER_MINES 20
#define MINESWEEPER_CELL 16 /* pixels per cell */

typedef enum minesweeper_cell_state
{
  minesweeper_HIDDEN,
  minesweeper_REVEALED,
  minesweeper_FLAGGED
}
minesweeper_cell_state_t;

/* classic minesweeper: Select reveals a cell (flood-filling neighbouring
 * zeros), Adjust toggles a flag. Mines are placed on the first reveal so the
 * opening click is never a mine. A MENU-button click pops a "New Game" menu
 * that resets the board. */
typedef struct minesweeper_task
{
  wuss_t                  *wuss;
  wuss_window_t           *window;
  wuss_task_t             *task; /* delegate; opens the New Game menu */
  bmfont_t                *font; /* borrowed; draws the neighbour counts */
  bool                     mine[MINESWEEPER_ROWS][MINESWEEPER_COLS];
  minesweeper_cell_state_t state[MINESWEEPER_ROWS][MINESWEEPER_COLS];
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
 * neighbour counts with the given (caller-owned) font */
result_t minesweeper_create(wuss_t             *wuss,
                            bmfont_t           *font,
                            minesweeper_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_MINESWEEPER_H */
