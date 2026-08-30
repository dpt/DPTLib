/* checker.c -- wuss test - checkerboard task */

#ifdef USE_SDL

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

result_t checker_create(wuss_t         *wuss,
                        const colour_t *palette,
                        checker_task_t *task)
{
  wuss_task_t delegate;
  result_t    rc;

  task->black    = palette[palette_PICO8_BLACK];
  task->white    = palette[palette_PICO8_WHITE];
  task->pattern  = checker_PATTERN_CHECKERBOARD;
  task->pattern2 = checker_PATTERN_VERTICAL;
  task->band     = CHECKER_BAND_DEFAULT;
  task->band2    = CHECKER_BAND_DEFAULT;

  delegate = wuss_task_start(checker_handle, task); /* checker_redraw paints every pixel itself */

  rc = wuss_window_create_placed(wuss,
                                 SIZE2D(160, 160),
                                 "Checker 1",
                                 wuss_WINDOW_NONE,
                                 wuss_NO_BACKGROUND,
                                 &delegate,
                                 SIZE2D(160, 160),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_create_placed(wuss,
                                 SIZE2D(160, 160),
                                 "Checker 2",
                                 wuss_WINDOW_NONE,
                                 wuss_NO_BACKGROUND,
                                 &delegate,
                                 SIZE2D(160, 160),
                                 SIZE2D(0, 0),
                                 &task->window2);
  if (rc != result_OK)
  {
    wuss_window_close(task->window);
    return rc;
  }

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

      screen_draw_pixel(scr, x, y, (band & 1) ? cc->black : cc->white);
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
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return checker_mouse(window, task_data);

  case wuss_EVENT_SCROLL:
    return checker_scroll(window, event->data.scroll.delta, task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_close(window);
    if (window == cc->window2)
      cc->window2 = NULL;
    else
      cc->window = NULL;
    /* one calloc'd block backs both windows; free it once both are gone */
    if (cc->window == NULL && cc->window2 == NULL)
      free(cc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
