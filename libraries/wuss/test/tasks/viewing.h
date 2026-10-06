/* wuss/test/tasks/viewing.h -- static bitmap viewing task */

#ifndef TASKS_VIEWING_H
#define TASKS_VIEWING_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "framebuf/screen.h"
#include "wuss/component/colourmenu.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* window's task: a loaded PNG, alpha-tested against the window
 * background (this test screen is paletted, so fully-transparent source
 * pixels are skipped and everything else is drawn at full strength) */
typedef struct viewing_task
{
  wuss_window_t *window;
  wuss_t        *wuss;      /* for wuss_get_pointer when opening the menu */
  wuss_task_t   *delegate;  /* the wuss task backing this window */
  bitmap_t       bitmap;    /* owned: base freed by the caller when done */
  bitmap_t       ninepatch; /* owned: 9-patch tiled behind the main image */
  wuss_menu_t   *menu;      /* owned: MENU-button demo tree, built from a
                              * descriptor string; rebuilt on each open,
                              * freed at QUIT */
  wuss_menu_handle_t menu_handle; /* the open chain, if any */
  wuss_menu_item_t dither_items[3]; /* one row per screen_dither_t */
  wuss_menu_t    dither_menu; /* "Dithering" submenu, borrowed by menu */
  screen_dither_t dithering; /* method for paletted screens */
  wuss_colour_t  background;
}
viewing_task_t;

wuss_window_fn_t viewing_handle;

/* load a fixed PNG from wuss_get_resources(wuss)/resources/images (and the
 * 9-patch PNG at wuss_get_resources(wuss)/resources/wuss/ninepatch.png, drawn
 * tiled behind it), and create its window against the given wuss instance.
 * The image cannot be changed afterwards. if out is non-NULL, the task block
 * is also returned through it */
result_t viewing_create(wuss_t *wuss, viewing_task_t **out);

/* install the icon bar app that opens viewing windows on demand */
result_t viewing_app_create(wuss_t *wuss);

/* free a task block allocated by viewing_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void viewing_destroy(viewing_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_VIEWING_H */
