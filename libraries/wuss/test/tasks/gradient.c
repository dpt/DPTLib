/* gradient.c -- wuss test - gradient fill task */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/colour.h"
#include "geom/box.h"

#include "gradient.h"

#define GRADIENT_DOC_WIDTH  400
#define GRADIENT_DOC_HEIGHT 400
#define GRADIENT_OPEN_WIDTH  100
#define GRADIENT_OPEN_HEIGHT 100

result_t gradient_create(wuss_t *wuss, gradient_task_t *task)
{
  wuss_task_t delegate;
  box_t       box;

  delegate = wuss_task_start(gradient_handle, task, wuss_NO_BACKGROUND); /* gradient_redraw paints every pixel itself */
  box      = (box_t) BOX_POS_SIZE(620, 300, GRADIENT_OPEN_WIDTH, GRADIENT_OPEN_HEIGHT);

  return wuss_window_create(wuss,
                            &box,
                            "Gradient",
                            wuss_WINDOW_NONE,
                            &delegate,
                            GRADIENT_DOC_WIDTH,
                            GRADIENT_DOC_HEIGHT,
                            &task->window);
}

void gradient_destroy(gradient_task_t *task)
{
  wuss_window_destroy(task->window);
}

static result_t gradient_redraw(wuss_window_t *window,
                                screen_t      *scr,
                                const box_t   *content,
                                void          *task_data)
{
  box_t bounds;
  int   sx, sy, x, y, lx, ly;

  NOT_USED(task_data);

  wuss_window_get_content_bounds(window, &bounds);
  wuss_window_get_scroll(window, &sx, &sy);

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = content->x0; x < content->x1; x++)
    {
      lx = x - bounds.x0 + sx;
      ly = y - bounds.y0 + sy;

      screen_draw_pixel(scr, x, y,
                        colour_rgb(lx * 255 / GRADIENT_DOC_WIDTH,
                                   ly * 255 / GRADIENT_DOC_HEIGHT,
                                   255 - (lx + ly) * 255 / (GRADIENT_DOC_WIDTH + GRADIENT_DOC_HEIGHT)));
    }
  }

  return result_OK;
}

result_t gradient_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  gradient_task_t *gc;

  gc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return gradient_redraw(window, event->data.redraw.scr, event->data.redraw.content, task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_destroy(window);
    gc->window = NULL;
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
