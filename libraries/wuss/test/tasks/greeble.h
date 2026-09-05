/* wuss/test/tasks/greeble.h -- edge-matched greebling pattern task */

#ifndef TASKS_GREEBLE_H
#define TASKS_GREEBLE_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/window.h"

/* Largest grid the generator will fill; the visible grid is whatever fits
 * the content area at the current tile size, capped to these. */
#define GREEBLE_MAX_COLS 24
#define GREEBLE_MAX_ROWS 40

/* Fills the content area with a maze-like circuit-greeble pattern built as a
 * tile-placement problem: each cell holds one of a small set of "pipe" tiles
 * (blank, stub, straight, elbow, tee, cross, plus decorated blank/straight
 * variants) and neighbours must agree on the wire crossing their shared
 * edge. A greedy scan places tiles left-to-right, top-to-bottom, picking
 * uniformly at random from those whose west/north edges match the
 * already-placed neighbours; the outer ring is biased closed so wires do not
 * run off-tile. Each tile is drawn from code as 2px strokes from its centre
 * to its open edges, stamped once offset in a shadow colour and once in the
 * foreground.
 *
 * A content click reseeds the pattern; the scroll wheel changes the tile
 * size. */
typedef struct greeble_task
{
  wuss_window_t *window;
  colour_t       fg;    /* wire / frame colour */
  colour_t       shadow; /* offset drop-shadow behind the wires */
  colour_t       bg;    /* fill behind everything */
  unsigned int   seed;  /* current pattern seed; advanced on click */
  int            tile;  /* tile side in pixels, scroll-adjustable */
  /* grid[row][col], one greeble_TILE_* index per cell; regenerated whenever
   * the seed or tile size changes */
  unsigned char  grid[GREEBLE_MAX_ROWS][GREEBLE_MAX_COLS];
  int            cols, rows; /* live extent of grid, <= the MAX_* caps */
}
greeble_task_t;

wuss_window_fn_t greeble_handle;

/* create the greebling window against the given wuss instance */
result_t greeble_create(wuss_t *wuss, greeble_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_GREEBLE_H */
