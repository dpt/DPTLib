/* wuss/test/tasks/palette.h -- desktop palette swatch grid task */

#ifndef TASKS_PALETTE_H
#define TASKS_PALETTE_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/colour.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* ponytail: fixed cap on the number of *.hex files the picker menu will
 * list, so the menu_items/names arrays can live inline in palette_task_t
 * with no malloc bookkeeping. Raise it if resources/palettes ever grows
 * past this. */
#define PALETTE_MAX_FILES 16

/* window D's task: draws every entry of the desktop palette as a square
 * in a grid, so the palette is visible at a glance. A MENU click over the
 * grid opens a picker for every *.hex file under resources/palettes, plus
 * an Invert toggle. A pick installs the loaded/inverted array via
 * wuss_set_palette; there is no callback here, since every other interested
 * party (framebuffer bitmap, physical palette) reacts to the resulting
 * wuss_EVENT_PALETTE and reads the array back with wuss_get_palette.
 *
 * A second window ("Screen") draws the same kind of grid but for the
 * physical screen bitmap's own palette (event->data.redraw.scr->palette),
 * which is whatever size the screen's pixel format needs (2/4/16/256
 * entries for 1/2/4/8bpp) rather than the fixed wuss_SYSTEM_PALETTE_LENGTH
 * system palette above; a NULL screen palette (32bpp, no palette) just
 * shows a label. */
typedef struct palette_task
{
  wuss_t              *wuss;
  wuss_task_t         *delegate;
  wuss_window_t       *window;
  wuss_window_t       *window2; /* "Screen" window: the physical screen's
                                  * own palette (1/2/4/8bpp) or a "none"
                                  * label (32bpp has no palette) */
  bool                 invert;

  /* *.hex files found under resources/palettes at create time, leafname
   * with the extension stripped (e.g. "PICO-8"), sorted. selected indexes
   * both this and menu_items. */
  char                 names[PALETTE_MAX_FILES][32];
  int                  nnames;
  int                  selected;

  /* the picker menu: per-instance (not static const) because its ticks
   * track selected/invert and it must outlive the open chain, so a stack
   * copy built fresh on each open would not do */
  wuss_menu_item_t     load_items[PALETTE_MAX_FILES]; /* one row per
                                                        * *.hex file */
  wuss_menu_t          load_menu; /* "Load" submenu, opened off menu_items */
  wuss_menu_item_t     menu_items[3]; /* Info, Load, Invert */
  wuss_menu_t          menu;
  wuss_menu_handle_t   menu_handle; /* for wuss_menu_tick_item_live on an
                                     * ADJUST pick, which keeps the chain
                                     * open */
}
palette_task_t;

wuss_window_fn_t palette_handle;

/* load a named *.hex file (leafname, no extension, e.g. "PICO-8") from
 * resources/palettes into a wuss_SYSTEM_PALETTE_LENGTH-entry colour_t
 * array. Used both by palette_create's picker and by callers choosing the
 * startup system palette before any window exists. */
result_t palette_load_hex(const char *resources,
                          const char *name,
                          colour_t   *out);

/* create the palette-swatch-grid window against the given wuss instance.
 * wuss_get_resources(wuss)/resources/palettes holds the *.hex files; it is
 * scanned once here to build the picker menu, ticking whichever file's
 * content matches wuss's current system palette (wuss_get_palette), if any.
 * if out is non-NULL, the task block is also returned through it. */
result_t palette_create(wuss_t *wuss, palette_task_t **out);

/* free a task block allocated by palette_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void palette_destroy(palette_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_PALETTE_H */
