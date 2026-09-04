/* wuss/test/tasks/checker.c -- checkerboard task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "checker.h"

#define CHECKER_BAND_DEFAULT 8  /* pixels per band, so each pattern reads clearly */
#define CHECKER_BAND_MIN     1
#define CHECKER_BAND_MAX     32

result_t checker_create(wuss_t*wuss, checker_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t    rc;

  task->black    = colour_rgb(0x00, 0x00, 0x00);
  task->white    = colour_rgb(0xFF, 0xFF, 0xFF);
  task->pattern  = checker_PATTERN_CHECKERBOARD;
  task->pattern2 = checker_PATTERN_VERTICAL;
  task->band     = CHECKER_BAND_DEFAULT;
  task->band2    = CHECKER_BAND_DEFAULT;

  /* checker_redraw paints every pixel itself */
  delegate_desc.handle    = checker_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "checker";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(160, 160),
                                 "Checker 1",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 SIZE2D(160, 160),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(160, 160),
                                 "Checker 2",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 SIZE2D(160, 160),
                                 SIZE2D(0, 0),
                                 &task->window2);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* closes "Checker 1", QUIT frees the block */
    return rc;
  }

  /* both windows up: from here, closing the last one reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  return result_OK;
}

static result_t checker_redraw(wuss_window_t      *window,
                               const wuss_event_t *event,
                               void               *task_data)
{
  checker_task_t   *cc;
  screen_t         *scr;
  const box_t      *content, *bounds;
  checker_pattern_t pattern;
  int               x, y, lx, ly, sx, sy, band_px, band;

  cc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  pattern = (window == cc->window2) ? cc->pattern2 : cc->pattern;
  band_px = (window == cc->window2) ? cc->band2    : cc->band;

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = content->x0; x < content->x1; x++)
    {
      lx = x - bounds->x0 + sx;
      ly = y - bounds->y0 + sy;

      switch (pattern)
      {
      case checker_PATTERN_HORIZONTAL: band = ly / band_px;                break;
      case checker_PATTERN_VERTICAL:   band = lx / band_px;                break;
      case checker_PATTERN_DIAGONAL:   band = (lx + ly) / band_px;         break;
      default:                         band = lx / band_px + ly / band_px; break; /* CHECKERBOARD */
      }

      screen_set_pixel(scr, x, y, (band & 1) ? cc->black : cc->white);
    }
  }

  return result_OK;
}

static result_t checker_mouse(wuss_window_t *window, void *task_data)
{
  checker_task_t    *cc;
  checker_pattern_t *pattern;

  cc = task_data;

  pattern  = (window == cc->window2) ? &cc->pattern2 : &cc->pattern;
  *pattern = (*pattern + 1) % checker_PATTERN__COUNT;

  wuss_window_invalidate_all(window);

  return result_OK;
}

static result_t checker_scroll(wuss_window_t *window,
                               int            delta,
                               void          *task_data)
{
  checker_task_t *cc;
  int            *band;

  cc   = task_data;
  band = (window == cc->window2) ? &cc->band2 : &cc->band;

  *band += delta;
  *band  = CLAMP(*band, CHECKER_BAND_MIN, CHECKER_BAND_MAX);

  wuss_window_invalidate_all(window);

  return result_OK;
}

result_t checker_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  checker_task_t *cc;

  cc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return checker_redraw(window, event, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_SELECT))
      return result_OK;
    return checker_mouse(window, task_data);

  case wuss_EVENT_SCROLL:
    return checker_scroll(window, event->data.scroll.delta, task_data);

  case wuss_EVENT_CLOSE:
    if (window == cc->window2)
      cc->window2 = NULL;
    else
      cc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    /* one calloc'd block backs both windows; the task autocloses once the
     * second window goes, so free it here */
    free(cc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
