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

/* 4x4 ordered (Bayer) dither matrix, values 0..15 */
static const int bayer4x4[4][4] =
{
  {  0,  8,  2, 10 },
  { 12,  4, 14,  6 },
  {  3, 11,  1,  9 },
  { 15,  7, 13,  5 },
};

static int dither(int v, int x, int y)
{
  return CLAMP(v + bayer4x4[y & 3][x & 3] - 8, 0, 255);
}

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
  wuss_window_close(task->window);
}

static result_t gradient_redraw(const wuss_event_t *event, void *task_data)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  int          sx, sy, x, y, lx, ly;

  NOT_USED(task_data);

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll_x;
  sy      = event->data.redraw.scroll_y;

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = content->x0; x < content->x1; x++)
    {
      lx = x - bounds->x0 + sx;
      ly = y - bounds->y0 + sy;

      screen_draw_pixel(scr, x, y,
                        colour_rgb(dither(lx * 255 / GRADIENT_DOC_WIDTH, lx, ly),
                                   dither(ly * 255 / GRADIENT_DOC_HEIGHT, lx, ly),
                                   dither(255 - (lx + ly) * 255 / (GRADIENT_DOC_WIDTH + GRADIENT_DOC_HEIGHT), lx, ly)));
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
    return gradient_redraw(event, task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_close(window);
    gc->window = NULL;
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
