/* palette.h -- wuss test - desktop palette swatch grid task */

#ifndef TASKS_PALETTE_H
#define TASKS_PALETTE_H

#ifdef USE_SDL

#include "framebuf/colour.h"
#include "wuss/window.h"

/* window D's task: draws every entry of the desktop palette as a square
 * in a grid, so the palette is visible at a glance */
typedef struct palette_task
{
  wuss_window_t   *window;
  const colour_t  *palette;
  int              npalette;
}
palette_task_t;

wuss_event_fn_t palette_handle;

/* create the palette-swatch-grid window against the given wuss instance */
result_t palette_create(wuss_t         *wuss,
                        const colour_t *palette,
                        int             npalette,
                        palette_task_t *task);

/* destroy the palette-swatch-grid window created by palette_create */
void palette_destroy(palette_task_t *task);

#endif /* USE_SDL */

#endif /* TASKS_PALETTE_H */
