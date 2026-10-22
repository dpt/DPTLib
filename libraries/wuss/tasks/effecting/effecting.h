/* wuss/tasks/effecting/effecting.h -- bitmap effects task */

#ifndef TASKS_EFFECTING_H
#define TASKS_EFFECTING_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/effects.h"
#include "framebuf/screen.h"
#include "framebuf/tonemap.h"
#include "wuss/component/saveas.h"
#include "wuss/gadget/gridview.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "../common/dithermenu.h"

#define EFFECTING_MAX_NAME_LEN 64
#define EFFECTING_LABEL_LEN    32 /* longest list label, with NUL */
#define EFFECTING_LEVEL_MIN    (-100)
#define EFFECTING_LEVEL_MAX    200

/* the kinds of element in the stack; also the rows of the Add submenu */
typedef enum effecting_type
{
  EFFECTING_CLEAR,
  EFFECTING_TONE,
  EFFECTING_GREY,
  EFFECTING_BLUR,
  EFFECTING_SHARPEN,
  EFFECTING_EXPAND,
  EFFECTING_EQUALISE,
  EFFECTING__LIMIT
}
effecting_type_t;

/* one effect in the stack */
typedef struct effecting_element
{
  effecting_type_t type;
  union
  {
    colour_t clear;
    struct
    {
      effects_blur_method_t method;
      int                   amount;
    }
    blur;
    tonemap_t *tone; /* owned */
  }
  u;
}
effecting_element_t;

/* menu rows */
enum
{
  EFFECTING_MENU_SAVE,
  EFFECTING_MENU_ADD,
  EFFECTING_MENU_EDIT,
  EFFECTING_MENU_DELETE,
  EFFECTING_MENU_DELETE_ALL,
  EFFECTING_MENU_LEVEL,
  EFFECTING_MENU_DITHERING,
  EFFECTING_MENU_APPLY,
  EFFECTING_MENU_CANCEL,
  EFFECTING_MENU__LIMIT
};

/* window's task: a PNG dropped on its icon or window (DataLoad) is run
 * through a stack of framebuf/effects functions, top to bottom, and the
 * outcome blended with the original by Level (-100% to 200%). Select toggles
 * between the result and the original. The stack is shown in a second
 * window: pick an element, drag it to reorder it. Menu adds, edits and
 * deletes elements, sets the level and the dithering used on paletted
 * screens, makes the result permanent (Apply) or discards it (Cancel) or
 * writes it as a PNG. Every change re-applies at once. */
typedef struct effecting_task
{
  wuss_window_t       *window;
  wuss_t              *wuss;
  wuss_task_t         *delegate;
  bitmap_t             source;   /* owned base; NULL until DataLoad */
  bitmap_t             chain;    /* owned base: source through the stack */
  bitmap_t             result;   /* owned base: chain blended with source */
  int                  show_original;
  int                  level;    /* percent of chain in the result */

  effecting_element_t *elements; /* owned */
  int                  nelements;
  int                  allocated;

  wuss_window_t       *list_window;
  wuss_gridview_t     *list;
  char                 label[EFFECTING_LABEL_LEN]; /* item callback */
  int                  drag_index; /* element being dragged, or -1 */

  char                 name[EFFECTING_MAX_NAME_LEN]; /* leafname of the shown
                                * image, less ".png" */

  wuss_menu_item_t     add_items[EFFECTING__LIMIT];
  wuss_menu_t          add_menu;
  dithermenu_t         dither;
  screen_dither_t      dithering; /* method for paletted screens */
  wuss_menu_item_t     menu_items[EFFECTING_MENU__LIMIT];
  wuss_menu_t          menu;
  wuss_menu_handle_t   menu_handle; /* the open chain, if any */
  wuss_saveas_t       *saveas;
  struct effecting_edit *edit;   /* the editing dialogues */
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
