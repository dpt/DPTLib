/* wuss/test/tasks/swatches.h -- fill-pattern swatch grid task */

#ifndef TASKS_SWATCHES_H
#define TASKS_SWATCHES_H

#ifdef WUSS_APP

#include "framebuf/screen.h"
#include "wuss/icon.h"
#include "wuss/window.h"

/* one 16x16 PATTERN icon per built-in screen fill pattern, laid out as a
 * grid down a document taller than the window so the swatches scroll
 * through it and stay phase-locked while doing so */
enum { SWATCHES_NSPECS = 1 + screen_PATTERN__LIMIT };

typedef struct swatches_task
{
  wuss_t        *wuss;   /* for re-resolving swatch colours on a palette swap */
  wuss_window_t *window;
  wuss_icon_t   *icons[SWATCHES_NSPECS]; /* current icons, deleted on rebuild */
}
swatches_task_t;

wuss_window_fn_t swatches_handle;

/* create the swatches window against the given wuss instance */
result_t swatches_create(wuss_t *wuss, swatches_task_t *task);


#endif /* WUSS_APP */

#endif /* TASKS_SWATCHES_H */
