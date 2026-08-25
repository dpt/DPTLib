/* sofa.h -- wuss test - rotating wireframe sofa task */

#ifndef TASKS_SOFA_H
#define TASKS_SOFA_H

#ifdef USE_SDL

#include <stdbool.h>

#include "framebuf/colour.h"
#include "wuss/window.h"

/* a wireframe sofa (seat, backrest, two arms) spinning about its vertical
 * axis; a content click pauses/resumes the spin */
typedef struct sofa_task
{
  wuss_window_t *window;
  colour_t       bg, line;
  double         angle;
  double         zoom; /* scroll-adjustable */
  bool           spinning;
}
sofa_task_t;

wuss_event_fn_t sofa_handle;

/* create the sofa window against the given wuss instance */
result_t sofa_create(wuss_t *wuss, const colour_t *palette, sofa_task_t *task);

/* destroy the sofa window created by sofa_create */
void sofa_destroy(sofa_task_t *task);

#endif /* USE_SDL */

#endif /* TASKS_SOFA_H */
