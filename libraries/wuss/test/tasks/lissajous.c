/* wuss/test/tasks/lissajous.c -- Lissajous figure task */

#ifdef WUSS_APP

#include <math.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "lissajous.h"

/* frequency pairs cycled by a Select click */
static const int lissajous_freqs[][2] =
{
  { 3, 2 }, { 5, 4 }, { 3, 4 }, { 5, 6 }, { 1, 2 }, { 7, 4 }
};

result_t lissajous_create(wuss_t *wuss, lissajous_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t         rc;

  task->bg         = colour_rgb(0x00, 0x00, 0x00);
  task->fg         = colour_rgb(0x00, 0xFF, 0x00);
  task->freq_index = 0;
  task->a          = lissajous_freqs[0][0];
  task->b          = lissajous_freqs[0][1];
  task->phase      = 0.0;
  task->drift      = 0.01;

  /* redraw paints its own background every frame */
  delegate_desc.handle    = lissajous_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "lissajous";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(220, 220),
                                 "Lissajous",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 SIZE2D(220, 220),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */

  return rc;
}

static result_t lissajous_redraw(const wuss_event_t *event, void *task_data)
{
  lissajous_task_t *lc;
  screen_t         *scr;
  const box_t      *content, *bounds;
  int               sx, sy;
  int               width, height, cx, cy, rx, ry;
  int               i;

  lc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content), lc->bg);

  width  = bounds->x1 - bounds->x0;
  height = bounds->y1 - bounds->y0;
  cx     = bounds->x0 - sx + width  / 2;
  cy     = bounds->y0 - sy + height / 2;
  rx     = width  / 2 - 10;
  ry     = height / 2 - 10;

  for (i = 0; i < LISSAJOUS_POINTS; i++)
  {
    double t;
    int    px, py;

    t  = (double) i / LISSAJOUS_POINTS * 2.0 * M_PI;
    px = cx + (int) (rx * sin(lc->a * t + lc->phase));
    py = cy + (int) (ry * sin(lc->b * t));

    screen_set_pixel(scr, px, py, lc->fg);
  }

  return result_OK;
}

static result_t lissajous_mouse(wuss_window_t      *window,
                                wuss_mouse_action_t action,
                                wuss_button_t       button,
                                void               *task_data)
{
  lissajous_task_t *lc;

  NOT_USED(window);

  lc = task_data;

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_SELECT)
  {
    lc->freq_index = (lc->freq_index + 1) % (int) NELEMS(lissajous_freqs);
    lc->a          = lissajous_freqs[lc->freq_index][0];
    lc->b          = lissajous_freqs[lc->freq_index][1];
    wuss_window_invalidate_all(lc->window); /* whole figure changes */
  }
  else if (button & wuss_BUTTON_ADJUST)
  {
    lc->drift = -lc->drift;
  }

  return result_OK;
}

static result_t lissajous_idle(void *task_data)
{
  lissajous_task_t *lc;

  lc = task_data;

  lc->phase += lc->drift;
  if (lc->phase > 2.0 * M_PI)
    lc->phase -= 2.0 * M_PI;
  else if (lc->phase < 0.0)
    lc->phase += 2.0 * M_PI;

  wuss_window_invalidate_all(lc->window); /* ponytail: repaint whole content; figure fills it and is cheap */

  return result_OK;
}

result_t lissajous_handle(wuss_window_t      *window,
                          const wuss_event_t *event,
                          void               *task_data)
{
  lissajous_task_t *lc;

  lc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return lissajous_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return lissajous_mouse(window, event->data.mouse.action,
                           event->data.mouse.button, task_data);

  case wuss_EVENT_IDLE:
    return lissajous_idle(task_data);

  case wuss_EVENT_QUIT:
    free(lc); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
