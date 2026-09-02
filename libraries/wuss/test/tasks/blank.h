/* wuss/test/tasks/blank.h -- colour-cycling task */

#ifndef TASKS_BLANK_H
#define TASKS_BLANK_H

#ifdef USE_SDL

#include "wuss/window.h"
#include "wuss/task.h"
#include "wuss/wuss.h"

/* window C's task: no redraw callback at all, relying entirely on wuss's
 * managed background fill; blank_step periodically hands wuss a new
 * palette index so the fill colour cycles over time */
typedef struct blank_task
{
  wuss_window_t *window;
  int            npalette;
  int            index;
  int            frame_count;
}
blank_task_t;

wuss_window_fn_t blank_handle;

/* create the colour-cycling blank window against the given wuss instance */
result_t blank_create(wuss_t *wuss, blank_task_t *task);


#endif /* USE_SDL */

#endif /* TASKS_BLANK_H */
