/* blank.c -- wuss test - colour-cycling task */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "blank.h"

#define BLANK_CYCLE_FRAMES 30 /* colour advances every half-second at 60fps */

result_t blank_create(wuss_t *wuss, int npalette, blank_task_t *task)
{
  wuss_task_t delegate;
  box_t       box;

  task->npalette    = npalette;
  task->index       = palette_PICO8_GREEN;
  task->frame_count = 0;

  delegate = wuss_task_make(NULL, NULL, task, palette_PICO8_GREEN); /* wuss fills the content area itself */
  box      = (box_t) BOX_POS_SIZE(260, 60, 200, 160);

  return wuss_window_create(wuss, &box, NULL, wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE, &delegate, &task->window);
}

void blank_destroy(blank_task_t *task)
{
  wuss_window_destroy(task->window);
}

void blank_step(blank_task_t *bc)
{
  result_t rc;

  if (++bc->frame_count < BLANK_CYCLE_FRAMES)
    return;

  bc->frame_count = 0;
  bc->index       = (bc->index + 1) % bc->npalette;

  rc = wuss_window_set_background(bc->window, bc->index);
  if (rc != result_OK)
    logf_warning("blank_step: wuss_window_set_background(%d) failed", bc->index);
}

#endif /* USE_SDL */
