/* wuss/tasks/curving/curving.h -- draggable Bezier curve task */

#ifndef TASKS_CURVING_H
#define TASKS_CURVING_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "geom/point.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/saveas.h"
#include "wuss/menu.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

#define CURVING_MINCONTROLPTS 2 /* a straight line */
#define CURVING_MAXCONTROLPTS 6 /* a quintic Bezier */
#define CURVING_NKINDS (CURVING_MAXCONTROLPTS - CURVING_MINCONTROLPTS + 1)
#define CURVING_NWIDTHS 6 /* rows in Menu > Width */

/* a single Bezier curve with draggable control points, redrawn as nsegments
 * straight line segments. The mouse wheel adjusts nsegments; an Adjust click
 * cycles the curve type (line, quadratic, cubic, quartic, quintic) by
 * stepping npoints, the count of points[] actually in play; Menu > Type
 * picks it directly; Menu > Width sets the line thickness; Menu > Hull
 * shows or hides the control points' convex hull; Menu > Reset points puts
 * the control points back where they started; Menu > Save opens a Save As dialogue (drag its icon onto a
 * Filer window, or Save with a full path already typed) that writes the
 * window's content out. Once a click has given the window the input focus,
 * Left/Right step the curve type, Up/Down step nsegments, H toggles the
 * hull and R resets the points. */
typedef struct curving_task
{
  wuss_t             *wuss;   /* borrowed; for wuss_get_font in the redraw */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_saveas_t       *saveas;
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[6]; /* per-instance: a shared static would
                                       * leak one instance's .window pointer
                                       * into another's menu */
  wuss_menu_t         menu;
  wuss_menu_item_t    type_items[CURVING_NKINDS]; /* one row per curve type */
  wuss_menu_t         type_menu;
  wuss_menu_item_t    width_items[CURVING_NWIDTHS]; /* one row per line width */
  wuss_menu_t         width_menu;
  colour_t             bg, line, blob;
  wuss_colourlist_t    colours; /* Menu > Colours, its one row Background */
  point_t              points[CURVING_MAXCONTROLPTS];
  int                  npoints;     /* CURVING_MINCONTROLPTS..CURVING_MAXCONTROLPTS */
  int                  nsegments;
  int                  width;       /* line width in pixels, from curving_widths */
  int                  dragging;    /* index into points, or -1 if not dragging */
  int                  hull;        /* draw the control points' convex hull */
}
curving_task_t;

wuss_window_fn_t curving_handle;

/* create the curve window against the given wuss instance; if out is
 * non-NULL, the task block is also returned through it */
result_t curving_create(wuss_t *wuss, curving_task_t **out);

/* install the icon bar app that opens curve windows on demand */
result_t curving_app_create(wuss_t *wuss);

/* free a task block allocated by curving_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void curving_destroy(curving_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CURVING_H */
