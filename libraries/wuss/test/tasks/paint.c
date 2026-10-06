/* wuss/test/tasks/paint.c -- basic painting task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "paint.h"
#include "taskapp.h"
#include "common.h"

#define PAINT_CANVAS_W 640
#define PAINT_CANVAS_H 480
#define PAINT_VIEW_W   400 /* initial visible size of the canvas window */
#define PAINT_VIEW_H   300

result_t paint_create(wuss_t *wuss, paint_task_t **out)
{
  result_t         rc;
  paint_task_t    *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->fg   = colour_rgb(0x00, 0x00, 0x00);
  task->bg   = colour_rgb(0xFF, 0xFF, 0xFF);
  task->size = 3;

  rc = paintdoc_create(SIZE2D(PAINT_CANVAS_W, PAINT_CANVAS_H), &task->doc);
  if (rc != result_OK)
  {
    free(task);
    return rc;
  }

  /* paint_redraw copies the canvas over the whole content area */
  delegate_desc.handle    = paint_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "paint";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    paintdoc_destroy(task->doc);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(PAINT_VIEW_W, PAINT_VIEW_H),
                                 "Paint",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(PAINT_CANVAS_W, PAINT_CANVAS_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  /* window up: from here, closing it reaps the task and its wuss_EVENT_QUIT
   * frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  if (out)
    *out = task;

  return result_OK;
}

result_t paint_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Paint",
    "Basic painting",
    (taskapp_create_fn_t) paint_create
  };

  return taskapp_install(wuss, &desc);
}

void paint_destroy(paint_task_t *task)
{
  paintdoc_destroy(task->doc);
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t paint_redraw(paint_task_t *pt, const wuss_event_t *event)
{
  const box_t *bounds;

  bounds = event->data.redraw.bounds;

  return screen_copy_bitmap(event->data.redraw.scr,
                            bounds->x0 - event->data.redraw.scroll.x,
                            bounds->y0 - event->data.redraw.scroll.y,
                            paintdoc_bitmap(pt->doc));
}

/* a MOVE always reports the Select button, hovering or not, so a stroke in
 * progress is tracked in pt->held instead */
static result_t paint_mouse(paint_task_t *pt, const wuss_event_t *event)
{
  box_t    dirty;
  point_t  p;
  colour_t colour;

  p = event->data.mouse.point;

  switch (event->data.mouse.action)
  {
  case wuss_MOUSE_DOWN:
    if (!(event->data.mouse.button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      return result_OK;

    if (paintdoc_checkpoint(pt->doc) != result_OK)
      return result_OK; /* no undo step, so no stroke */

    pt->held = (event->data.mouse.button & wuss_BUTTON_SELECT) ?
               wuss_BUTTON_SELECT : wuss_BUTTON_ADJUST;
    pt->last = p;
    break; /* dab at p */

  case wuss_MOUSE_MOVE:
    if (!pt->held)
      return result_OK;
    break;

  case wuss_MOUSE_UP:
    pt->held = 0;
    return result_OK;

  default:
    return result_OK;
  }

  colour = (pt->held == wuss_BUTTON_SELECT) ? pt->fg : pt->bg;
  dirty  = paintdoc_stroke(pt->doc, pt->last, p, pt->size, colour);
  pt->last = p;

  if (!box_is_empty(&dirty))
    wuss_window_invalidate(pt->window, &dirty);

  return result_OK;
}

result_t paint_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  paint_task_t *pt;

  (void) window;

  pt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return paint_redraw(pt, event);

  case wuss_EVENT_MOUSE:
    return paint_mouse(pt, event);

  case wuss_EVENT_CLOSE:
    pt->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    /* the task autocloses once its window goes, so free the block here */
    paint_destroy(pt);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
