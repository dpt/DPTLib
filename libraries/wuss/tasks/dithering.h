/* wuss/tasks/dithering.h -- error-diffusion dithering demo task */

#ifndef TASKS_DITHERING_H
#define TASKS_DITHERING_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "framebuf/dither.h"
#include "wuss/component/dialogue.h"
#include "wuss/component/saveas.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "palette.h"

#define DITHERING_MAX_NAME_LEN 64
#define DITHERING_EMPTYSZ      64 /* window size before the first DataLoad */
#define DITHERING_SLIDER_W     128 /* slider dialogues' slider */
#define DITHERING_COLOURS_STEPS 8 /* Colours slider positions per doubling */

/* Palette submenu rows: these two, then one per *.hex file */
#define DITHERING_PALETTE_OPTIMAL 0
#define DITHERING_PALETTE_DESKTOP 1
#define DITHERING_PALETTE_FIRST_HEX 2

/* Main menu rows */
enum
{
  DITHERING_MENU_METHOD,
  DITHERING_MENU_PALETTE,
  DITHERING_MENU_COLOURS,
  DITHERING_MENU_STRENGTH,
  DITHERING_MENU_SERPENTINE,
  DITHERING_MENU_SAVE,
  DITHERING_MENU__LIMIT
};

/* window's task: a PNG dropped on its icon or window (DataLoad) reduced by
 * dither_bitmap, optionally to an octree palette. A Select click toggles
 * between the dithered result and the original; Menu picks the options, and
 * every change re-dithers at once; Colours and Strength are slider
 * dialogues. Save writes the dithered result as a PNG. */
typedef struct dithering_task
{
  wuss_window_t     *window;
  wuss_t            *wuss;
  wuss_task_t       *delegate;
  bitmap_t           source;   /* owned base: rgbx8888 or rgba8888; NULL
                                * base until the first DataLoad */
  bitmap_t          *dithered; /* owned; NULL if the last dither failed */
  int                show_original;

  dither_method_t    method;
  int                palette;  /* a DITHERING_PALETTE_* row */
  int                colours_pos; /* Colours slider position: optimal
                                   * palette size is 2^(pos / STEPS) */
  int                strength; /* percent */
  int                serpentine;

  char               name[DITHERING_MAX_NAME_LEN]; /* leafname of the shown
                                * image, less ".png" */
  char               palettes[PALETTE_MAX_FILES][32]; /* *.hex leafnames */
  int                npalettes;

  wuss_menu_item_t   method_items[dither__LIMIT];
  wuss_menu_t        method_menu;
  wuss_menu_item_t   palette_items[DITHERING_PALETTE_FIRST_HEX +
                                   PALETTE_MAX_FILES];
  wuss_menu_t        palette_menu;
  wuss_menu_item_t   menu_items[DITHERING_MENU__LIMIT];
  wuss_menu_t        menu;
  wuss_menu_handle_t menu_handle; /* the open chain, if any */
  wuss_saveas_t     *saveas;
  wuss_dialogue_t   *strength_dialogue; /* NULL once the window closes */
  wuss_slider_row_t  strength_row;
  wuss_dialogue_t   *colours_dialogue; /* NULL once the window closes */
  wuss_slider_row_t  colours_row;
}
dithering_task_t;

wuss_window_fn_t dithering_handle;

/* scan wuss_get_resources(wuss)/resources/palettes for *.hex files and create
 * an empty window against the given wuss instance; it shows an image once one
 * is dropped on it. if out is non-NULL, the task block is also returned
 * through it */
result_t dithering_create(wuss_t *wuss, dithering_task_t **out);

/* install the icon bar app that opens dithering windows on demand */
result_t dithering_app_create(wuss_t *wuss);

/* free a task block allocated by dithering_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void dithering_destroy(dithering_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_DITHERING_H */
