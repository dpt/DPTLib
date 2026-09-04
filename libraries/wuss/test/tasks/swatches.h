/* wuss/test/tasks/swatches.h -- fill-pattern swatch grid task */

#ifndef TASKS_SWATCHES_H
#define TASKS_SWATCHES_H

#ifdef WUSS_APP

/* ponytail: no #ifdef WUSS_COMPONENTS guard -- it is PUBLIC on DPTLib and ON
 * by default, so the wuss app always has the colourmenu component. */
#include "wuss/component/colourmenu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* Grid of every (fill pattern, palette colour) pair: one row per built-in
 * screen fill pattern, one column per system-palette entry, each cell a
 * PATTERN icon packed edge to edge. The document is taller than the window
 * so the grid scrolls through it. A MENU-button click pops a wuss_colourmenu;
 * the picked colour becomes the paper the patterns mix over (white until
 * then). */

typedef struct swatches_task
{
  wuss_t            *wuss;
  wuss_window_t     *window;
  wuss_task_t       *task;      /* delegate; opens the colour menu */
  wuss_colourmenu_t *colourmenu;
  wuss_colour_t      paper;     /* pattern bg; the "mixing" colour */
}
swatches_task_t;

wuss_window_fn_t swatches_handle;

/* create the swatches window against the given wuss instance */
result_t swatches_create(wuss_t *wuss, swatches_task_t *task);


#endif /* WUSS_APP */

#endif /* TASKS_SWATCHES_H */
