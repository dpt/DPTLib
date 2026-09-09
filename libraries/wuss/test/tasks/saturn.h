/* wuss/test/tasks/saturn.h -- Elite loading-screen planet, recreated */

#ifndef TASKS_SATURN_H
#define TASKS_SATURN_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/window.h"

/* window task: recreates the ringed planet from the loading screen of
 * Elite (Ian Bell and David Braben, Acornsoft, 1984). A cryptic BBC BASIC
 * one-liner stipples it out of three rejection-sampled point clouds - a
 * ring of dots, a sheared streak across it and a filled disc for the
 * planet body. Select re-seeds the RNG for a fresh sketch; the idle
 * handler re-seeds every null event so it churns. The plot is
 * deterministic in the seed. */
typedef struct saturn_task
{
  wuss_window_t *window;
  colour_t       bg, fg;
  unsigned long  seed; /* RNG state; a Select click bumps it */
}
saturn_task_t;

wuss_window_fn_t saturn_handle;

/* create the planet window against the given wuss instance; "task" is a
 * per-instance block owned by the window and freed when it closes */
result_t saturn_create(wuss_t *wuss, saturn_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_SATURN_H */
