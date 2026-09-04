/* wuss/test/tasks/gradient.h -- gradient fill task */

#ifndef TASKS_GRADIENT_H
#define TASKS_GRADIENT_H

#ifdef WUSS_APP

#include "wuss/window.h"

/* fills its whole content area with a two-axis colour gradient; opens
 * small (100x100) against a large (400x400) document, so scrollbars
 * appear and the fill can be scrolled around */
typedef struct gradient_task
{
  wuss_window_t *window;
}
gradient_task_t;

wuss_window_fn_t gradient_handle;

/* create the gradient window against the given wuss instance */
result_t gradient_create(wuss_t *wuss, gradient_task_t *task);


#endif /* WUSS_APP */

#endif /* TASKS_GRADIENT_H */
