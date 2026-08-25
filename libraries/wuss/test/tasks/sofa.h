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
  bool           spinning;
}
sofa_task_t;

wuss_redraw_fn_t sofa_redraw;
wuss_mouse_fn_t  sofa_mouse;

/* create the sofa window against the given wuss instance */
result_t sofa_create(wuss_t *wuss, const colour_t *palette, sofa_task_t *task);

/* destroy the sofa window created by sofa_create */
void sofa_destroy(sofa_task_t *task);

/* advance the spin and invalidate the whole window; called once per frame
 * from the main loop, not from a wuss callback */
void sofa_step(sofa_task_t *task);

#endif /* USE_SDL */

#endif /* TASKS_SOFA_H */
