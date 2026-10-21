/* wuss/tasks/viewing/viewing.h -- static bitmap viewing task */

#ifndef TASKS_VIEWING_H
#define TASKS_VIEWING_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "framebuf/screen.h"
#include "geom/stage.h"
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
  wuss_menu_t   *menu;      /* owned: MENU-button demo tree, built from a
                              * descriptor string; rebuilt on each open,
                              * freed at QUIT */
  wuss_menu_handle_t menu_handle; /* the open chain, if any */
  wuss_menu_item_t dither_items[3]; /* one row per screen_dither_t */
  wuss_menu_t    dither_menu; /* "Dithering" submenu, borrowed by menu */
  screen_dither_t dithering; /* method for paletted screens */
  colour_t       colour_pasteboard; /* stage: outside the page */
  colour_t       colour_paper;      /* page; the band behind the image when
                                     * the stage is off */
  colour_t       colour_shadow;
  colour_t       colour_border;
  wuss_colourlist_t colours; /* "Colours" submenu over the four above */
  int            stage;     /* nonzero: draw a stage (page, border, shadow)
                             * behind the image instead of a plain band */
  stageconfig_t  stagecfg;
}
viewing_task_t;

wuss_window_fn_t viewing_handle;

/* create an empty viewing window against the given wuss instance. A
 * wuss_MESSAGE_DATA_LOAD sent to the window, whose payload is the path of a
 * PNG, shows that image; a file that will not load
 * is not acknowledged. if out is non-NULL, the task block is also returned
 * through it */
result_t viewing_create(wuss_t *wuss, viewing_task_t **out);

/* install the icon bar app that opens viewing windows on demand */
result_t viewing_app_create(wuss_t *wuss);

/* free a task block allocated by viewing_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void viewing_destroy(viewing_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_VIEWING_H */
