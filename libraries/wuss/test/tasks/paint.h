/* wuss/test/tasks/paint.h -- basic painting task */

#ifndef TASKS_PAINT_H
#define TASKS_PAINT_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "geom/point.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "paint-doc.h"

#define PAINT_SIZE_MIN 1
#define PAINT_SIZE_MAX 32

/* a 640x480 white canvas in a scrollable window. Select paints with the
 * foreground colour, Adjust with the background colour */
typedef struct paint_task
{
  wuss_t        *wuss;
  wuss_task_t   *delegate;
  wuss_window_t *window;   /* the canvas window; NULL once closed */
  paintdoc_t    *doc;
  colour_t       fg, bg;
  int            size;     /* brush width in pixels */
  int            held;     /* button of the stroke in progress, or 0 */
  point_t        last;     /* where the stroke in progress last reached */
}
paint_task_t;

wuss_window_fn_t paint_handle;

/* create the paint window against the given wuss instance;
 * if out is non-NULL, the task block is also returned through it */
result_t paint_create(wuss_t *wuss, paint_task_t **out);

/* install the icon bar app that opens paint windows on demand */
result_t paint_app_create(wuss_t *wuss);

/* free a task block allocated by paint_create; normally called by the
 * task's wuss_EVENT_QUIT handler, not by callers directly */
void paint_destroy(paint_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_PAINT_H */
