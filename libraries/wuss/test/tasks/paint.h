/* wuss/test/tasks/paint.h -- basic painting task */

#ifndef TASKS_PAINT_H
#define TASKS_PAINT_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "geom/point.h"
#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/component/saveas.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "paint-doc.h"
#include "paint-new.h"

#define PAINT_SIZE_MIN 1
#define PAINT_SIZE_MAX 32

/* the toolbox's radio buttons, in this order */
typedef enum paint_tool
{
  paint_TOOL_BRUSH,
  paint_TOOL_ERASER,
  paint_TOOL_LINE,
  paint_TOOL_RECT,
  paint_TOOL_FILL,
  paint_TOOL_SELECT, /* rubber-band a rectangular selection */
  paint_TOOL_LASSO,  /* drag out a freeform selection */
  paint_TOOL__COUNT
}
paint_tool_t;

/* the Menu click menu's rows */
enum
{
  PAINT_MENU_UNDO,
  PAINT_MENU_SAVE,
  PAINT_MENU__COUNT
};

/* a 640x480 white canvas in a scrollable window, with a small toolbox window
 * beside it. Select paints with the foreground colour and Adjust with the
 * background colour; clicking a toolbox swatch sets either. Closing the
 * canvas window closes the toolbox too */
typedef struct paint_task
{
  wuss_t            *wuss;
  wuss_task_t       *delegate;
  wuss_window_t     *window;    /* the canvas window; NULL once closed */
  wuss_window_t     *toolbox;   /* the toolbox window; NULL once closed */
  paintdoc_t        *doc;
  wuss_saveas_t     *saveas;
  colour_t           fg, bg;
  paint_tool_t       tool;
  int                size;      /* brush and outline width in pixels */
  int                fill;      /* rect tool: filled, not an outline */
  int                held;      /* button of the stroke in progress, or 0 */
  point_t            anchor;    /* where the stroke began */
  point_t            last;      /* where the stroke last reached */
  point_t           *lasso;     /* points dragged out so far by the lasso */
  int                nlasso, caplasso;
  int                ants_tick; /* idles since the marching ants last moved */
  int                ants_phase; /* where the ants have marched to, 0..7 */
  wuss_icon_t       *tool_icons[paint_TOOL__COUNT];
  wuss_icon_t       *fill_icon;
  wuss_slider_row_t  size_row;
  wuss_menu_handle_t menu_handle; /* live only between open and a pick */
  wuss_menu_item_t   menu_items[PAINT_MENU__COUNT];
  wuss_menu_t        menu;
  char               size_text[WUSS_SLIDER_ROW_BUF];
}
paint_task_t;

wuss_window_fn_t paint_handle;

/* create the paint windows against the given wuss instance;
 * if out is non-NULL, the task block is also returned through it */
result_t paint_create(wuss_t *wuss, paint_task_t **out);

/* as paint_create, but with a canvas of the given size and colour */
result_t paint_create_with(wuss_t                  *wuss,
                           const paintnew_params_t *params,
                           paint_task_t           **out);

/* install the icon bar app that opens paint windows on demand */
result_t paint_app_create(wuss_t *wuss);

/* free a task block allocated by paint_create; normally called by the
 * task's wuss_EVENT_QUIT handler, not by callers directly */
void paint_destroy(paint_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_PAINT_H */
