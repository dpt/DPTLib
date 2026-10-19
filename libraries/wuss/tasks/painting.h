/* wuss/tasks/painting.h -- basic painting task */

#ifndef TASKS_PAINTING_H
#define TASKS_PAINTING_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "geom/point.h"
#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/component/saveas.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "painting-doc.h"
#include "painting-new.h"

#define PAINTING_SIZE_MIN 1
#define PAINTING_SIZE_MAX 32

/* the toolbox's radio buttons, in this order */
typedef enum painting_tool
{
  painting_TOOL_BRUSH,
  painting_TOOL_PAINTBRUSH, /* stamp the chosen nozzle along the stroke */
  painting_TOOL_ERASER,
  painting_TOOL_LINE,
  painting_TOOL_RECT,
  painting_TOOL_RECT_FILL, /* as RECT, but solid */
  painting_TOOL_FILL,
  painting_TOOL_SPRAY,  /* scatter dots around the pointer while held */
  painting_TOOL_SELECT, /* rubber-band a rectangular selection */
  painting_TOOL_LASSO,  /* drag out a freeform selection */
  painting_TOOL_PICK,   /* take the fg (Select) or bg (Adjust) from the canvas */
  painting_TOOL__COUNT
}
painting_tool_t;

/* the Menu click menu's rows */
enum
{
  PAINTING_MENU_UNDO,
  PAINTING_MENU_SELECTION, /* submenu: PAINTING_SEL_* */
  PAINTING_MENU_NOZZLE,    /* submenu: one row per paintingdoc_nozzle_t */
  PAINTING_MENU_SAVE,
  PAINTING_MENU__COUNT
};

/* the Selection submenu's rows */
enum
{
  PAINTING_SEL_ALL,
  PAINTING_SEL_NONE,
  PAINTING_SEL_INVERT,
  PAINTING_SEL__COUNT
};

/* a 640x480 white canvas in a scrollable window, with a small toolbox window
 * beside it. Select paints with the foreground colour and Adjust with the
 * background colour; clicking a toolbox swatch sets either. Closing the
 * canvas window closes the toolbox too */
typedef struct painting_task
{
  wuss_t            *wuss;
  wuss_task_t       *delegate;
  wuss_window_t     *window;    /* the canvas window; NULL once closed */
  wuss_window_t     *toolbox;   /* the toolbox window; NULL once closed */
  paintingdoc_t     *doc;
  wuss_saveas_t     *saveas;
  colour_t           fg, bg;
  painting_tool_t    tool;
  paintingdoc_nozzle_t nozzle;  /* the paintbrush's current nozzle */
  int                size;      /* brush and outline width in pixels */
  int                held;      /* button of the stroke in progress, or 0 */
  point_t            anchor;    /* where the stroke began */
  point_t            last;      /* where the stroke last reached */
  point_t           *lasso;     /* points dragged out so far by the lasso */
  int                nlasso, caplasso;
  int                ants_tick;  /* idles since the ants last moved */
  int                ants_phase; /* where the ants have marched, 0..7 */
  rng_t              rng;        /* the spray's scatter */
  wuss_icon_t       *tool_icons[painting_TOOL__COUNT];
  wuss_slider_row_t  size_row;
  wuss_menu_handle_t menu_handle; /* live only between open and a pick */
  wuss_menu_item_t   menu_items[PAINTING_MENU__COUNT];
  wuss_menu_t        menu;
  wuss_menu_item_t   sel_items[PAINTING_SEL__COUNT];
  wuss_menu_t        sel_menu;
  wuss_menu_item_t   nozzle_items[paintingdoc_NOZZLE__COUNT];
  wuss_menu_t        nozzle_menu;
  char               size_text[WUSS_SLIDER_ROW_BUF];
}
painting_task_t;

wuss_window_fn_t painting_handle;

/* create the paint windows against the given wuss instance;
 * if out is non-NULL, the task block is also returned through it */
result_t painting_create(wuss_t *wuss, painting_task_t **out);

/* as painting_create, but with a canvas of the given size and colour */
result_t painting_create_with(wuss_t                     *wuss,
                              const paintingnew_params_t *params,
                              painting_task_t           **out);

/* install the icon bar app that opens paint windows on demand */
result_t painting_app_create(wuss_t *wuss);

/* free a task block allocated by painting_create; normally called by the
 * task's wuss_EVENT_QUIT handler, not by callers directly */
void painting_destroy(painting_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_PAINTING_H */
