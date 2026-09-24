/* wuss/test/tasks/minesweeper.h -- minesweeper task */

#ifndef TASKS_MINESWEEPER_H
#define TASKS_MINESWEEPER_H

#ifdef WUSS_APP

#include <stdbool.h>
#include <time.h>

#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

#define MINESWEEPER_CELL 22 /* pixels per cell; matches mine.png/flag.png */

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
 * Game" and a "Grid Size" submenu that resets the board at a new size. With
 * the input focus, the arrow keys move a cursor cell, Return reveals it and
 * Space toggles its flag. */
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
  bmfont_t                *hud_font; /* owned; draws the mines/timer HUD */
  int                      hud_font_h; /* hud_font's glyph cell height */
  int                      hud_field_w; /* pixel width of a "999" field in
                                         * hud_font, plus a small margin */
  bitmap_t                 mine_bm; /* owned; drawn on a revealed mine
                                     * cell */
  bitmap_t                 flag_bm; /* owned; drawn on a flagged cell */
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
