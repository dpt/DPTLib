/* wuss/test/tasks/image.h -- static bitmap image task */

#ifndef TASKS_IMAGE_H
#define TASKS_IMAGE_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#define IMAGE_MAX_NAMES 64
#define IMAGE_MAX_NAME_LEN 64

/* window's task: a loaded PNG, alpha-tested against the window
 * background (this test screen is paletted, so fully-transparent source
 * pixels are skipped and everything else is drawn at full strength) */
typedef struct image_task
{
  wuss_window_t *window;
  wuss_t        *wuss;      /* for wuss_get_pointer when opening the menu */
  wuss_task_t   *delegate;  /* the wuss task backing this window */
  bitmap_t       bitmap;    /* owned: base freed by the caller when done */
  bitmap_t       ninepatch; /* owned: 9-patch tiled behind the main image */
  int            index;     /* current position in names[] */
  char           names[IMAGE_MAX_NAMES][IMAGE_MAX_NAME_LEN]; /* owned:
                              * leafnames (extension stripped) found under
                              * resources/images at spawn time */
  int            nnames;
  wuss_menu_t   *menu;      /* owned: MENU-button demo tree, built from a
                              * descriptor string; rebuilt on each open,
                              * freed at QUIT */
  wuss_proginfo_t *proginfo; /* owned: the "Info" row's standard dialogue,
                              * hung off the menu as a wuss_menu_item_t.window;
                              * freed at QUIT */
  wuss_colourmenu_t *colourmenu; /* owned: the "Background" row's submenu,
                              * hung off the menu as a wuss_menu_item_t.submenu;
                              * freed at QUIT */
  wuss_menu_handle_t menu_handle; /* the open chain, if any -- closed before the
                              * proginfo it borrows is destroyed at QUIT */
  int            dithering; /* enable Bayer ordered dithering */
  wuss_colour_t  background;
}
image_task_t;

wuss_window_fn_t image_handle;

/* scan wuss_get_resources(wuss)/resources/images for PNGs, load the first
 * one found (and the 9-patch PNG at
 * wuss_get_resources(wuss)/resources/wuss/ninepatch.png, drawn tiled behind
 * it), and create its window against the given wuss instance. a click cycles
 * to a different leafname from the scan. if out is non-NULL, the task block
 * is also returned through it */
result_t image_create(wuss_t *wuss, image_task_t **out);

/* free a task block allocated by image_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void image_destroy(image_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_IMAGE_H */
