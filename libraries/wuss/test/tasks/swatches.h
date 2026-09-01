/* wuss/test/tasks/swatches.h -- fill-pattern swatch grid task */

#ifndef TASKS_SWATCHES_H
#define TASKS_SWATCHES_H

#ifdef USE_SDL

#include "wuss/window.h"

/* one 16x16 PATTERN icon per built-in screen fill pattern, laid out as a
 * grid down a document taller than the window so the swatches scroll
 * through it and stay phase-locked while doing so */
typedef struct swatches_task
{
  wuss_window_t *window;
}
swatches_task_t;

wuss_event_fn_t swatches_handle;

/* create the swatches window against the given wuss instance */
result_t swatches_create(wuss_t *wuss, swatches_task_t *task);


#endif /* USE_SDL */

#endif /* TASKS_SWATCHES_H */
