/* wuss/test/tasks/tiling.h -- tiled backdrop task */

#ifndef TASKS_TILING_H
#define TASKS_TILING_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "geom/point.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#define TILING_MAX_NAMES 64
#define TILING_MAX_NAME_LEN 64

/* fills a screen-sized window with a PNG repeated edge to edge. Select
 * drag moves the tile origin by the pointer's movement; Adjust steps to the
 * next PNG under resources/images; Menu opens Info, Load and Reset origin */
typedef struct tiling_task
{
  wuss_window_t *window;
  wuss_t        *wuss;
  bitmap_t       bitmap; /* owned: base freed when replaced or at QUIT */
  point_t        origin; /* window-local position of one tile's top-left */
  point_t        grab;   /* pointer position at the last drag event */
  int            dragging; /* Select held: pointer moves drag the origin */
  int            index;  /* current position in names[] */
  char           names[TILING_MAX_NAMES][TILING_MAX_NAME_LEN]; /* leafnames
                          * (extension stripped) found at create time */
  int            nnames;
  wuss_task_t   *delegate; /* the wuss task backing this window */
  wuss_menu_item_t load_items[TILING_MAX_NAMES]; /* one row per names[] */
  wuss_menu_t    load_menu; /* "Load" submenu, borrowed by menu */
  wuss_menu_t   *menu;      /* owned: built from a descriptor string on each
                             * open, freed at QUIT */
  wuss_menu_handle_t menu_handle; /* the open chain, if any */
}
tiling_task_t;

wuss_window_fn_t tiling_handle;

/* scan wuss_get_resources(wuss)/resources/images for PNGs, load the first
 * one, and create a screen-sized window tiled with it. if out is non-NULL,
 * the task block is also returned through it */
result_t tiling_create(wuss_t *wuss, tiling_task_t **out);

/* install the icon bar app that opens tiling windows on demand */
result_t tiling_app_create(wuss_t *wuss);

/* free a task block allocated by tiling_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void tiling_destroy(tiling_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_TILING_H */
