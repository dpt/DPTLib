/* wuss/test/tasks/blank.c -- colour-cycling task */

#ifdef USE_SDL

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "blank.h"

#define BLANK_CYCLE_FRAMES 30 /* colour advances every half-second at 60fps */

result_t blank_create(wuss_t *wuss, blank_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t         rc;

  task->npalette    = 16; // TODO: Read max palette index from wuss
  task->index       = 0;
  task->frame_count = 0;

  /* wuss fills the content area itself */
  delegate_desc.handle    = blank_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "blank";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
    return rc;

  return wuss_window_create_placed(delegate,
                                   SIZE2D(200, 160),
                                   NULL,
                                   wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                                   wuss_BACKDROP_COLOUR(task->index),
                                   SIZE2D(200, 160),
                                   SIZE2D(0, 0),
                                   &task->window);
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

  rc = wuss_window_set_background(bc->window,
                                  wuss_BACKDROP_COLOUR(bc->index));
  if (rc != result_OK)
    logf_warning("blank_idle: wuss_window_set_background(%d) failed", bc->index);

  return rc;
}

result_t blank_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  if (event->kind == wuss_EVENT_CLOSE)
  {
    free(task_data); /* calloc'd per instance by the spawner */
    return result_OK;
  }

  if (event->kind != wuss_EVENT_IDLE)
    return result_OK;

  return blank_idle(task_data);
}

#endif /* USE_SDL */
