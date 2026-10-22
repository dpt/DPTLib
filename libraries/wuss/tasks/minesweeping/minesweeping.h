/* wuss/tasks/minesweeping/minesweeping.h -- minesweeper task */

#ifndef TASKS_MINESWEEPING_H
#define TASKS_MINESWEEPING_H

#ifdef WUSS_APP

#include <stdbool.h>
#include <time.h>

#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "../common/fireball.h"

#define MINESWEEPING_CELL 22 /* pixels per cell; matches mine.png/flag.png */

/* board sizes offered on the "Grid size" menu; arrays are sized for the
 * largest so a resize just changes ms->rows/cols/mines and re-clears */
#define MINESWEEPING_MAX_ROWS 24
#define MINESWEEPING_MAX_COLS 24

typedef enum minesweeping_size
{
  minesweeping_SIZE_24X24,
  minesweeping_SIZE_24X12,
  minesweeping_SIZE_16X16,
  minesweeping_SIZE_16X12,
  minesweeping_SIZE_12X12,
  minesweeping_NSIZES
}
minesweeping_size_t;

/* mine densities offered on the "Difficulty" menu; each scales the chosen
 * grid size's classic mine count */
typedef enum minesweeping_difficulty
{
  minesweeping_DIFFICULTY_EASY,
  minesweeping_DIFFICULTY_NORMAL,
  minesweeping_DIFFICULTY_HARD,
  minesweeping_NDIFFICULTIES
}
minesweeping_difficulty_t;

typedef enum minesweeping_cell_state
{
  minesweeping_HIDDEN,
  minesweeping_REVEALED,
  minesweeping_FLAGGED
}
minesweeping_cell_state_t;

/* classic minesweeper: Select reveals a cell (flood-filling neighbouring
 * zeros), Adjust toggles a flag. Select on a revealed number whose flags
 * match it reveals the rest of its neighbours ("chording"). Mines are
 * placed on the first reveal so the opening click is never a mine. A win
 * that beats the session's fastest time at that size and difficulty says
 * so on its banner. A
 * MENU-button click pops a menu with "New game", a "Grid size" submenu
 * that resets the board at a new size and a "Difficulty" submenu that
 * resets it with more or fewer mines; "Give up" ends a game in progress
 * and shows every mine. With the input focus, the arrow keys move a cursor
 * cell, Return reveals it, Space toggles its flag and N starts a new
 * game. Hitting a mine sets off a particle explosion at the clicked cell. */
typedef struct minesweeping_task
{
  wuss_t                  *wuss;
  wuss_window_t           *window;
  wuss_task_t             *task; /* delegate; opens the New game menu */
  wuss_menu_handle_t       menu_handle; /* live only between open and a pick */
  wuss_menu_item_t         size_items[minesweeping_NSIZES];
  wuss_menu_t              size_menu;
  wuss_menu_item_t         difficulty_items[minesweeping_NDIFFICULTIES];
  wuss_menu_t              difficulty_menu;
  wuss_menu_item_t         menu_items[4]; /* per-instance: a shared static
                                            * would leak one instance's
                                            * .window pointer into another's
                                            * menu */
  wuss_menu_t              menu;
  bmfont_t                *font; /* borrowed; draws the neighbour counts */
  bmfont_t                *hud_font; /* owned; draws the mines/timer HUD */
  int                      hud_font_h; /* hud_font's glyph cell height */
  int                      hud_field_w; /* pixel width of a "999" field in
                                         * hud_font, plus a small margin */
  const bitmap_t          *mine_bm; /* private pool "mine"; drawn on a revealed
                                     * mine cell */
  const bitmap_t          *flag_bm; /* private pool "flag"; drawn on a flagged
                                     * cell */
  minesweeping_size_t       size;
  minesweeping_difficulty_t difficulty;
  int                      rows, cols, mines;
  bool                     mine[MINESWEEPING_MAX_ROWS][MINESWEEPING_MAX_COLS];
  minesweeping_cell_state_t state[MINESWEEPING_MAX_ROWS][MINESWEEPING_MAX_COLS];
  bool                     placed;  /* mines placed yet? */
  bool                     dead;    /* a mine was revealed */
  bool                     won;
  bool                     new_best; /* this win beat best[][] */
  int                      best[minesweeping_NSIZES][minesweeping_NDIFFICULTIES];
                                     /* fastest win in seconds + 1, per
                                      * size and difficulty; 0 = none yet */
  int                      flags;   /* flagged cell count, for the counter */
  time_t                   start_time; /* set on first reveal */
  int                      elapsed; /* seconds, frozen on dead/won */
  int                      cur_r, cur_c; /* keyboard cursor cell */
  struct
  {
    colour_t number[9];     /* [1..8] neighbour-count colours; [0] unused */
    colour_t cell_revealed;
    colour_t cell_hidden;
    colour_t cell_border;
    colour_t hud_fg;
    colour_t hud_bg;
    colour_t fill;          /* HUD strip and board frame */
    colour_t dead_bg;       /* banner background on death */
    colour_t won_bg;        /* banner background on win */
    colour_t banner_fg;
    colour_t cursor;        /* keyboard cursor outline */
  }                          colours;

  fireball_t               boom; /* death explosion over the board */
}
minesweeping_task_t;

wuss_window_fn_t minesweeping_handle;

/* create the minesweeper window against the given wuss instance, lettering
 * neighbour counts with wuss's own bold font (wuss_get_font_n(wuss, 1)).
 * if out is non-NULL, the task block is also returned through it */
result_t minesweeping_create(wuss_t *wuss, minesweeping_task_t **out);

/* install the icon bar app that opens minesweeper windows on demand */
result_t minesweeping_app_create(wuss_t *wuss);

/* free a task block allocated by minesweeping_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void minesweeping_destroy(minesweeping_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_MINESWEEPING_H */
