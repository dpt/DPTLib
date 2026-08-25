/* checker.c -- wuss test - checkerboard task */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "checker.h"

#define CHECKER_BAND_DEFAULT 8  /* pixels per band, so each pattern reads clearly */
#define CHECKER_BAND_MIN     2
#define CHECKER_BAND_MAX     32

static void invalidate_whole(wuss_window_t *window)
{
  box_t content;

  wuss_window_get_content_bounds(window, &content);
  content.x1 -= content.x0; content.x0 = 0;
  content.y1 -= content.y0; content.y0 = 0;
  wuss_window_invalidate(window, &content);
}

result_t checker_create(wuss_t         *wuss,
                        const colour_t *palette,
                        checker_task_t *task)
{
  wuss_task_t delegate;
  box_t       box;
  result_t    rc;

  task->black    = palette[palette_PICO8_BLACK];
  task->white    = palette[palette_PICO8_WHITE];
  task->pattern  = checker_PATTERN_CHECKERBOARD;
  task->pattern2 = checker_PATTERN_VERTICAL;
  task->band     = CHECKER_BAND_DEFAULT;
  task->band2    = CHECKER_BAND_DEFAULT;

  delegate = wuss_task_start(checker_handle, task, wuss_NO_BACKGROUND); /* checker_redraw paints every pixel itself */
  box      = (box_t) BOX_POS_SIZE(440, 300, 160, 160);

  rc = wuss_window_create(wuss, &box, "Checker 1", wuss_WINDOW_NONE, &delegate, &task->window);
  if (rc != result_OK)
    return rc;

  box = (box_t) BOX_POS_SIZE(440, 10, 160, 160);

  rc = wuss_window_create(wuss, &box, "Checker 2", wuss_WINDOW_NONE, &delegate, &task->window2);
  if (rc != result_OK)
  {
    wuss_window_destroy(task->window);
    return rc;
  }

  return result_OK;
}

void checker_destroy(checker_task_t *task)
{
  wuss_window_destroy(task->window);
  wuss_window_destroy(task->window2);
}

static result_t checker_redraw(wuss_window_t *window,
                               screen_t      *scr,
                               const box_t   *content,
                               void          *task_data)
{
  checker_task_t   *cc;
  box_t             bounds;
  checker_pattern_t pattern;
  int               x, y, lx, ly, band_px, band;

  cc = task_data;

  wuss_window_get_content_bounds(window, &bounds);

  pattern = (window == cc->window2) ? cc->pattern2 : cc->pattern;
  band_px = (window == cc->window2) ? cc->band2    : cc->band;

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = content->x0; x < content->x1; x++)
    {
      lx = x - bounds.x0;
      ly = y - bounds.y0;

      switch (pattern)
      {
      case checker_PATTERN_HORIZONTAL: band = ly / band_px;                    break;
      case checker_PATTERN_VERTICAL:   band = lx / band_px;                    break;
      case checker_PATTERN_DIAGONAL:   band = (lx + ly) / band_px;             break;
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

  invalidate_whole(window);

  return result_OK;
}

static result_t checker_scroll(wuss_window_t *window, int delta, void *task_data)
{
  checker_task_t *cc;
  int            *band;

  cc   = task_data;
  band = (window == cc->window2) ? &cc->band2 : &cc->band;

  *band += delta;
  if (*band < CHECKER_BAND_MIN)
    *band = CHECKER_BAND_MIN;
  else if (*band > CHECKER_BAND_MAX)
    *band = CHECKER_BAND_MAX;

  invalidate_whole(window);

  return result_OK;
}

result_t checker_handle(wuss_window_t     *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return checker_redraw(window, event->data.redraw.scr, event->data.redraw.content, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return checker_mouse(window, task_data);

  case wuss_EVENT_SCROLL:
    return checker_scroll(window, event->data.scroll.delta, task_data);

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
