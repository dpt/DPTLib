/* blank.c -- wuss test - colour-cycling task */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
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

  delegate = wuss_task_start(blank_handle, task); /* wuss fills the content area itself */
  box      = (box_t) BOX_POS_SIZE(260, 60, 200, 160);

  return wuss_window_create(wuss,
                            &box,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            palette_PICO8_GREEN,
                            &delegate,
                            box_size(&box),
                            (size2d_t) { 0, 0 },
                            &task->window);
}

void blank_destroy(blank_task_t *task)
{
  wuss_window_close(task->window);
}

static result_t blank_idle(void *task_data)
{
  blank_task_t *bc;
  result_t      rc;

  bc = task_data;

  if (++bc->frame_count < BLANK_CYCLE_FRAMES)
    return result_OK;

  bc->frame_count = 0;
  bc->index       = (bc->index + 1) % bc->npalette;

  rc = wuss_window_set_background(bc->window, bc->index);
  if (rc != result_OK)
    logf_warning("blank_idle: wuss_window_set_background(%d) failed", bc->index);

  return rc;
}

result_t blank_handle(wuss_window_t     *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  NOT_USED(window);

  if (event->kind != wuss_EVENT_IDLE)
    return result_OK;

  return blank_idle(task_data);
}

#endif /* USE_SDL */
