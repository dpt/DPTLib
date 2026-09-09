/* wuss/test/tasks/gradient.h -- gradient fill task */

#ifndef TASKS_GRADIENT_H
#define TASKS_GRADIENT_H

#ifdef WUSS_APP

#include "wuss/window.h"

/* fills its whole content area with a two-axis colour gradient; opens
 * small (100x100) against a large (400x400) document, so scrollbars
 * appear and the fill can be scrolled around. SELECT/ADJUST clicks cycle
 * the ordered-dither matrix forward/backward through 2x2, 4x4 and 8x8. */
typedef struct gradient_task
{
  wuss_t        *wuss;   /* borrowed; for wuss_get_font in the redraw */
  wuss_window_t *window;
  int            dither_index;
}
gradient_task_t;

wuss_window_fn_t gradient_handle;

/* create the gradient window against the given wuss instance */
result_t gradient_create(wuss_t *wuss, gradient_task_t *task);


#endif /* WUSS_APP */

#endif /* TASKS_GRADIENT_H */
