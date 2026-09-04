/* wuss/test/tasks/lissajous.h -- Lissajous figure task */

#ifndef TASKS_LISSAJOUS_H
#define TASKS_LISSAJOUS_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/window.h"

#define LISSAJOUS_POINTS 512 /* samples plotted along the curve */

/* window task: a Lissajous figure x=sin(a*t+phase), y=sin(b*t) plotted as a
 * ring of dots. The phase drifts each idle tick so the figure slowly morphs.
 * Select cycles the frequency pair (a,b); Adjust reverses the drift. */
typedef struct lissajous_task
{
  wuss_window_t *window;
  colour_t       bg, fg;
  int            a, b;    /* frequency ratio */
  double         phase;
  double         drift;
  int            freq_index;
}
lissajous_task_t;

wuss_window_fn_t lissajous_handle;

/* create the Lissajous window against the given wuss instance; "task" is a
 * per-instance block owned by the window and freed when it closes */
result_t lissajous_create(wuss_t *wuss, lissajous_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_LISSAJOUS_H */
