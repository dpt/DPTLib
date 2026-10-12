/* wuss/test/tasks/resizing.h -- image resizing demo task */

#ifndef TASKS_RESIZING_H
#define TASKS_RESIZING_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "wuss/component/saveas.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#define RESIZING_MAX_NAME_LEN 64
#define RESIZING_EMPTYSZ      64 /* window size before the first DataLoad */
#define RESIZING_NSCALES      8  /* rows in the Scale submenu */

/* Main menu rows */
enum
{
  RESIZING_MENU_METHOD,
  RESIZING_MENU_SCALE,
  RESIZING_MENU_LINEAR,
  RESIZING_MENU_SAVE,
  RESIZING_MENU__LIMIT
};

/* window's task: a PNG dropped on its icon or window (DataLoad) resampled by
 * bitmap_resize to the chosen scale. Menu picks the method and scale, and
 * every change resamples at once. Save writes the resized result as a
 * PNG. */
typedef struct resizing_task
{
  wuss_window_t     *window;
  wuss_t            *wuss;
  wuss_task_t       *delegate;
  bitmap_t           source;  /* owned base: rgbx8888 or rgba8888; NULL
                               * base until the first DataLoad */
  bitmap_t          *resized; /* owned; NULL if the last resize failed */

  bitmap_resample_t  method;
  int                scale;   /* index into the Scale rows */
  int                linear;  /* blend in linear light */

  char               name[RESIZING_MAX_NAME_LEN]; /* leafname of the shown
                               * image, less ".png" */

  wuss_menu_item_t   method_items[bitmap_resample__LIMIT];
  wuss_menu_t        method_menu;
  wuss_menu_item_t   scale_items[RESIZING_NSCALES];
  wuss_menu_t        scale_menu;
  wuss_menu_item_t   menu_items[RESIZING_MENU__LIMIT];
  wuss_menu_t        menu;
  wuss_menu_handle_t menu_handle; /* the open chain, if any */
  wuss_saveas_t     *saveas;
}
resizing_task_t;

wuss_window_fn_t resizing_handle;

/* create an empty window against the given wuss instance; it shows an image
 * once one is dropped on it. if out is non-NULL, the task block is also
 * returned through it */
result_t resizing_create(wuss_t *wuss, resizing_task_t **out);

/* install the icon bar app that opens resizing windows on demand */
result_t resizing_app_create(wuss_t *wuss);

/* free a task block allocated by resizing_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void resizing_destroy(resizing_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_RESIZING_H */
