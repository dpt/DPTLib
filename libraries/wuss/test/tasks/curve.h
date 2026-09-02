/* wuss/test/tasks/curve.h -- draggable Bezier curve task */

#ifndef TASKS_CURVE_H
#define TASKS_CURVE_H

#ifdef USE_SDL

#include "framebuf/colour.h"
#include "geom/point.h"
#include "wuss/window.h"

#define CURVE_NCONTROLPTS 4 /* cubic Bezier: start, 2 control points, end */

/* a single cubic Bezier curve with draggable control points, redrawn as
 * nsegments straight line segments; the mouse wheel adjusts nsegments */
typedef struct curve_task
{
  wuss_window_t *window;
  colour_t       bg, line, blob;
  point_t        points[CURVE_NCONTROLPTS];
  int            nsegments;
  int            dragging;    /* index into points, or -1 if not dragging */
}
curve_task_t;

wuss_window_fn_t curve_handle;

/* create the curve window against the given wuss instance */
result_t curve_create(wuss_t *wuss, curve_task_t *task);

#endif /* USE_SDL */

#endif /* TASKS_CURVE_H */
