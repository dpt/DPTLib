/* wuss/test/tasks/greeble.h -- prefab-scatter greebling pattern task */

#ifndef TASKS_GREEBLE_H
#define TASKS_GREEBLE_H

#ifdef WUSS_APP

#include "utils/rng.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* Largest grid the generator will fill; Menu > Size picks the live grid
 * from a table of presets within these caps. */
#define GREEBLE_MAX_COLS 48
#define GREEBLE_MAX_ROWS 48

/* Most base palettes the Palette submenu can list; greeble.c checks
 * GREEBLE_NPALETTE against it at compile time. */
#define GREEBLE_MAX_PALETTES 16

/* Number of presets in the Size submenu. */
#define GREEBLE_NSIZES 4

/* Fills the content area with a Xenon-2-style circuit-greeble texture built
 * as a tile-placement problem. greeble-tiles.h bakes 205 hand-drawn 8x8
 * PICO-8 stamps plus, from a second sheet where the artist laid them out in
 * their intended shapes, a set of prefab blocks and a shortlist of stamps
 * that read well standing alone. The generator scatters the prefab blocks
 * without overlap, then fills every remaining cell from the standalone
 * shortlist. Each stamp is blitted 1:1; its four slots index the current
 * greeble_palettes[] row.
 *
 * Select reseeds the pattern; Adjust steps the base palette, which Menu >
 * Palette also picks directly; Menu > Random palettes toggles per-prefab
 * random palettes; Menu > Size sets the grid (and window) to a preset size;
 * Menu > Save PNG writes the window to greeble.png. Once a click has given
 * the window the input focus, Space reseeds, Left/Right step the base
 * palette and R toggles random palettes. */
typedef struct greeble_task
{
  wuss_t            *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t       *delegate; /* the task that owns the menu */
  wuss_window_t     *window;
  wuss_menu_handle_t menu_handle; /* live only between open and a SELECT pick */
  rng_t              seed;   /* current pattern seed; advanced on click */
  int                cols, rows; /* live grid extent, <= the MAX_* caps */
  unsigned char      palette;  /* greeble_palettes[] row for this pattern */
  int                size;     /* greeble_sizes[] preset for the grid */
  int                random_prefab_palettes; /* nonzero: each scattered prefab
                                              * gets its own random palette */
  /* grid[row][col], one stamp index (0..GREEBLE_NTILES-1) per cell;
   * regenerated whenever the seed changes */
  unsigned char      grid[GREEBLE_MAX_ROWS][GREEBLE_MAX_COLS];
  /* cellpal[row][col], the greeble_palettes[] row to draw each cell with;
   * kept in step with grid[][] */
  unsigned char      cellpal[GREEBLE_MAX_ROWS][GREEBLE_MAX_COLS];
  wuss_menu_item_t   menu_items[5]; /* per-instance: a shared static would
                                     * leak one instance's .window pointer
                                     * into another's menu */
  wuss_menu_t        menu;
  char               palette_names[GREEBLE_MAX_PALETTES][4]; /* "1".."16" */
  wuss_menu_item_t   palette_items[GREEBLE_MAX_PALETTES];
  wuss_menu_t        palette_menu;
  wuss_menu_item_t   size_items[GREEBLE_NSIZES];
  wuss_menu_t        size_menu;
}
greeble_task_t;

wuss_window_fn_t greeble_handle;

/* create the greebling window against the given wuss instance; if out is
 * non-NULL, the task block is also returned through it */
result_t greeble_create(wuss_t *wuss, greeble_task_t **out);

/* free a task block allocated by greeble_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void greeble_destroy(greeble_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_GREEBLE_H */
