/* wuss/test/tasks/greeble.h -- prefab-scatter greebling pattern task */

#ifndef TASKS_GREEBLE_H
#define TASKS_GREEBLE_H

#ifdef WUSS_APP

#include "utils/rng.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* Largest grid the generator will fill; the visible grid is whatever fits
 * the content area in 8-pixel tiles, capped to these. */
#define GREEBLE_MAX_COLS 48
#define GREEBLE_MAX_ROWS 48

/* Fills the content area with a Xenon-2-style circuit-greeble texture built
 * as a tile-placement problem. greeble-tiles.h bakes 205 hand-drawn 8x8
 * PICO-8 stamps plus, from a second sheet where the artist laid them out in
 * their intended shapes, a set of prefab blocks and a shortlist of stamps
 * that read well standing alone. The generator scatters the prefab blocks
 * without overlap, then fills every remaining cell from the standalone
 * shortlist. Each stamp is blitted 1:1; its four slots index the current
 * greeble_palettes[] row.
 *
 * Select reseeds the pattern; Adjust and the Menu row both toggle per-prefab
 * random palettes. */
typedef struct greeble_task
{
  wuss_t            *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t       *delegate; /* the task that owns the menu */
  wuss_window_t     *window;
  wuss_menu_handle_t menu_handle; /* live only between open and a SELECT pick */
  rng_t              seed;   /* current pattern seed; advanced on click */
  int                cols, rows; /* live grid extent, <= the MAX_* caps */
  unsigned char      palette;  /* greeble_palettes[] row for this pattern */
  int                random_prefab_palettes; /* nonzero: each scattered prefab
                                              * gets its own random palette */
  /* grid[row][col], one stamp index (0..GREEBLE_NTILES-1) per cell;
   * regenerated whenever the seed changes */
  unsigned char      grid[GREEBLE_MAX_ROWS][GREEBLE_MAX_COLS];
  /* cellpal[row][col], the greeble_palettes[] row to draw each cell with;
   * kept in step with grid[][] */
  unsigned char      cellpal[GREEBLE_MAX_ROWS][GREEBLE_MAX_COLS];
}
greeble_task_t;

wuss_window_fn_t greeble_handle;

/* create the greebling window against the given wuss instance */
result_t greeble_create(wuss_t *wuss, greeble_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_GREEBLE_H */
