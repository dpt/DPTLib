/* checker.h -- wuss test - checkerboard task */

#ifndef TASKS_CHECKER_H
#define TASKS_CHECKER_H

#ifdef USE_SDL

#include "framebuf/colour.h"
#include "wuss/window.h"

/* fills the whole content area with a black and white pixel checkerboard */
typedef struct checker_task
{
  wuss_window_t *window, *window2;
  colour_t       black, white;
}
checker_task_t;

wuss_redraw_fn_t checker_redraw;

/* create the two checkerboard windows against the given wuss instance */
result_t checker_create(wuss_t *wuss, const colour_t *palette, checker_task_t *task);

/* destroy the checkerboard windows created by checker_create */
void checker_destroy(checker_task_t *task);

#endif /* USE_SDL */

#endif /* TASKS_CHECKER_H */
