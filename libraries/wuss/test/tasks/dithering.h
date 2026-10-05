/* wuss/test/tasks/dithering.h -- error-diffusion dithering demo task */

#ifndef TASKS_DITHERING_H
#define TASKS_DITHERING_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "framebuf/dither.h"
#include "wuss/component/saveas.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "palette.h"

#define DITHERING_MAX_IMAGES   64
#define DITHERING_MAX_NAME_LEN 64
#define DITHERING_NCOUNTS      6 /* rows in the Colours submenu */
#define DITHERING_NSTRENGTHS   5 /* rows in the Strength submenu */

/* Palette submenu rows: these two, then one per *.hex file */
#define DITHERING_PALETTE_OPTIMAL 0
#define DITHERING_PALETTE_DESKTOP 1
#define DITHERING_PALETTE_FIRST_HEX 2

/* Main menu rows */
enum
{
  DITHERING_MENU_IMAGE,
  DITHERING_MENU_METHOD,
  DITHERING_MENU_PALETTE,
  DITHERING_MENU_COLOURS,
  DITHERING_MENU_STRENGTH,
  DITHERING_MENU_SERPENTINE,
  DITHERING_MENU_SAVE,
  DITHERING_MENU__LIMIT
};

/* window's task: a PNG from resources/images reduced by dither_bitmap,
 * optionally to an octree palette. A Select click toggles between the
 * dithered result and the original; Menu picks the image and the options,
 * and every change re-dithers at once. Save writes the dithered result as
 * a PNG. */
typedef struct dithering_task
{
  wuss_window_t     *window;
  wuss_t            *wuss;
  wuss_task_t       *delegate;
  bitmap_t           source;   /* owned base: rgbx8888 or rgba8888 */
  bitmap_t          *dithered; /* owned; NULL if the last dither failed */
  int                show_original;

  int                image;    /* index into images[] */
  dither_method_t    method;
  int                palette;  /* a DITHERING_PALETTE_* row */
  int                count;    /* index into the Colours rows */
  int                strength; /* index into the Strength rows */
  int                serpentine;

  char               images[DITHERING_MAX_IMAGES][DITHERING_MAX_NAME_LEN];
  int                nimages;
  char               palettes[PALETTE_MAX_FILES][32]; /* *.hex leafnames */
  int                npalettes;

  wuss_menu_item_t   image_items[DITHERING_MAX_IMAGES];
  wuss_menu_t        image_menu;
  wuss_menu_item_t   method_items[dither__LIMIT];
  wuss_menu_t        method_menu;
  wuss_menu_item_t   palette_items[DITHERING_PALETTE_FIRST_HEX +
                                   PALETTE_MAX_FILES];
  wuss_menu_t        palette_menu;
  wuss_menu_item_t   count_items[DITHERING_NCOUNTS];
  wuss_menu_t        count_menu;
  wuss_menu_item_t   strength_items[DITHERING_NSTRENGTHS];
  wuss_menu_t        strength_menu;
  wuss_menu_item_t   menu_items[DITHERING_MENU__LIMIT];
  wuss_menu_t        menu;
  wuss_menu_handle_t menu_handle; /* the open chain, if any */
  wuss_saveas_t     *saveas;
}
dithering_task_t;

wuss_window_fn_t dithering_handle;

/* scan wuss_get_resources(wuss)/resources/images for PNGs and
 * resources/palettes for *.hex files, dither the first image and create its
 * window against the given wuss instance. if out is non-NULL, the task block
 * is also returned through it */
result_t dithering_create(wuss_t *wuss, dithering_task_t **out);

/* install the icon bar app that opens dithering windows on demand */
result_t dithering_app_create(wuss_t *wuss);

/* free a task block allocated by dithering_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void dithering_destroy(dithering_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_DITHERING_H */
