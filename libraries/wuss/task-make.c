/* task-make.c -- wuss - minimal window manager */

#include "wuss/window.h"

wuss_task_t wuss_task_make(wuss_redraw_fn_t *redraw, wuss_mouse_fn_t *mouse, void *task_data, wuss_colour_t bg)
{
  wuss_task_t task;

  task.redraw    = redraw;
  task.mouse     = mouse;
  task.task_data = task_data;
  task.bg        = bg;

  return task;
}
