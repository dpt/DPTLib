/* wuss/test/tasks/curve.h -- draggable Bezier curve task */

#ifndef TASKS_CURVE_H
#define TASKS_CURVE_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "geom/point.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

#define CURVE_MINCONTROLPTS 2 /* a straight line */
#define CURVE_MAXCONTROLPTS 6 /* a quintic Bezier */

/* a single Bezier curve with draggable control points, redrawn as nsegments
 * straight line segments. The mouse wheel adjusts nsegments; an Adjust click
 * cycles the curve type (line, quadratic, cubic, quartic, quintic) by
 * stepping npoints, the count of points[] actually in play. */
typedef struct curve_task
{
  wuss_t             *wuss;   /* borrowed; for wuss_get_font in the redraw */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_proginfo_t    *proginfo;
  wuss_menu_item_t    menu_items[1]; /* per-instance: a shared static would
                                       * leak one instance's proginfo window
                                       * pointer into another's menu */
  wuss_menu_t         menu;
  colour_t             bg, line, blob;
  point_t              points[CURVE_MAXCONTROLPTS];
  int                  npoints;     /* CURVE_MINCONTROLPTS..CURVE_MAXCONTROLPTS */
  int                  nsegments;
  int                  dragging;    /* index into points, or -1 if not dragging */
}
curve_task_t;

wuss_window_fn_t curve_handle;

/* create the curve window against the given wuss instance; if out is
 * non-NULL, the task block is also returned through it */
result_t curve_create(wuss_t *wuss, curve_task_t **out);

/* free a task block allocated by curve_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void curve_destroy(curve_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CURVE_H */
