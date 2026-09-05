/* wuss/test/tasks/greeble.h -- random-scatter greebling pattern task */

#ifndef TASKS_GREEBLE_H
#define TASKS_GREEBLE_H

#ifdef WUSS_APP

#include "wuss/window.h"

/* Largest grid the generator will fill; the visible grid is whatever fits
 * the content area in 8-pixel tiles, capped to these. */
#define GREEBLE_MAX_COLS 32
#define GREEBLE_MAX_ROWS 48

/* Fills the content area with a Xenon-2-style circuit-greeble texture built
 * as a tile-placement problem. The tile set is 205 hand-drawn 8x8 PICO-8
 * stamps baked from an artist's sheet (greeble-tiles.h). The sheet is a
 * scrapbook of decorative fragments, not a connective tile set, so each cell
 * simply takes a stamp chosen uniformly at random -- the result reads as the
 * intended greeble texture without any adjacency rule. The stamp is blitted
 * 1:1; its four palette slots map to the active Wuss palette, so the pattern
 * recolours with the window-manager theme.
 *
 * A content click reseeds the pattern. */
typedef struct greeble_task
{
  wuss_window_t *window;
  unsigned int   seed;   /* current pattern seed; advanced on click */
  int            cols, rows; /* live grid extent, <= the MAX_* caps */
  /* grid[row][col], one stamp index (0..GREEBLE_NTILES-1) per cell;
   * regenerated whenever the seed changes */
  unsigned char  grid[GREEBLE_MAX_ROWS][GREEBLE_MAX_COLS];
}
greeble_task_t;

wuss_window_fn_t greeble_handle;

/* create the greebling window against the given wuss instance */
result_t greeble_create(wuss_t *wuss, greeble_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_GREEBLE_H */
