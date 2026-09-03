/* wuss/test/tasks/swatches.h -- fill-pattern swatch grid task */

#ifndef TASKS_SWATCHES_H
#define TASKS_SWATCHES_H

#ifdef WUSS_APP

#include "wuss/window.h"

/* Grid of every (fill pattern, palette colour) pair: one row per built-in
 * screen fill pattern, one column per system-palette entry, each cell a 4x4
 * PATTERN icon packed edge to edge. The document is taller than the window
 * so the grid scrolls through it. */

typedef struct swatches_task
{
  wuss_t        *wuss;
  wuss_window_t *window;
}
swatches_task_t;

wuss_window_fn_t swatches_handle;

/* create the swatches window against the given wuss instance */
result_t swatches_create(wuss_t *wuss, swatches_task_t *task);


#endif /* WUSS_APP */

#endif /* TASKS_SWATCHES_H */
