/* wuss/tasks/effecting.h -- bitmap effects task */

#ifndef TASKS_EFFECTING_H
#define TASKS_EFFECTING_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "framebuf/screen.h"
#include "wuss/component/saveas.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#define EFFECTING_MAX_NAME_LEN 64
#define EFFECTING_EMPTYSZ      64 /* window size before the first DataLoad */
#define EFFECTING_NAMOUNTS 6

/* indexed by the "Effect" submenu rows */
enum
{
  EFFECTING_NONE,
  EFFECTING_BLUR_BOX,
  EFFECTING_BLUR_GAUSSIAN,
  EFFECTING_SHARPEN,
  EFFECTING_GREY,
  EFFECTING_EXPAND,
  EFFECTING_EQUALISE,
  EFFECTING_BRIGHTEN,
  EFFECTING_INVERT,
  EFFECTING__LIMIT
};

/* window's task: a PNG dropped on its icon or window (DataLoad) run through
 * one of the framebuf/effects functions. Select toggles between the result
 * and the original; Menu picks the effect and the blur amount, and every
 * change re-applies at once. Save PNG writes the result. */
typedef struct effecting_task
{
  wuss_window_t     *window;
  wuss_t            *wuss;
  wuss_task_t       *delegate;
  bitmap_t           source;   /* owned base; NULL until the first DataLoad */
  bitmap_t           result;   /* owned base, same size and format as source */
  int                show_original;

  int                effect;   /* an EFFECTS_* value */
  int                amount;   /* index into the Amount rows */
  char               name[EFFECTING_MAX_NAME_LEN]; /* leafname of the shown
                                * image, less ".png" */

  wuss_menu_item_t   effect_items[EFFECTING__LIMIT];
  wuss_menu_t        effect_menu;
  wuss_menu_item_t   amount_items[EFFECTING_NAMOUNTS];
  wuss_menu_t        amount_menu;
  char               amount_text[EFFECTING_NAMOUNTS][8];
  wuss_menu_t       *menu;     /* owned: rebuilt on each open */
  wuss_menu_handle_t menu_handle; /* the open chain, if any */
  wuss_saveas_t     *saveas;
}
effecting_task_t;

wuss_window_fn_t effecting_handle;

/* create an empty window against the given wuss instance; it shows an image
 * once one is dropped on it. if out is non-NULL, the task block is also
 * returned through it */
result_t effecting_create(wuss_t *wuss, effecting_task_t **out);

/* install the icon bar app that opens effects windows on demand */
result_t effecting_app_create(wuss_t *wuss);

/* free a task block allocated by effecting_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void effecting_destroy(effecting_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_EFFECTING_H */
