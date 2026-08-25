/* blank.h -- wuss test - colour-cycling task */

#ifndef TASKS_BLANK_H
#define TASKS_BLANK_H

#ifdef USE_SDL

#include "wuss/window.h"
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

/* create the colour-cycling blank window against the given wuss instance */
result_t blank_create(wuss_t *wuss, int npalette, blank_task_t *task);

/* destroy the colour-cycling window created by blank_create */
void blank_destroy(blank_task_t *task);

/* advance the cycle and, every few frames, push the next palette index as
 * the window's background; called once per frame from the main loop */
void blank_step(blank_task_t *bc);

#endif /* USE_SDL */

#endif /* TASKS_BLANK_H */
