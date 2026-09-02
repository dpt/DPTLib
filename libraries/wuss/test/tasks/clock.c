/* wuss/test/tasks/clock.c -- analogue clock task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <time.h>

#include <math.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "utils/fxp.h"

#include "clock.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CLOCK_BEZEL_SEGMENTS 90 /* sides of the polygon standing in for the round bezel */
#define CLOCK_FACE_FRACTION  0.92 /* bezel radius as a fraction of half the smaller side */
#define CLOCK_HOUR_TICK      0.10 /* hour-tick length, fraction of face radius */
#define CLOCK_MINUTE_TICK    0.05 /* minute-tick length, fraction of face radius */
#define CLOCK_HOUR_HAND      0.55 /* hand lengths, fraction of face radius */
#define CLOCK_MINUTE_HAND    0.80
#define CLOCK_SECOND_HAND    0.88

/* a screen-space point in fix8_t, for screen_draw_line_wu_fix8 */
typedef struct fix8_point { fix8_t x, y; } fix8_point_t;

/* point on a circle of radius r about (cx,cy); angle 0 points straight up
 * (12 o'clock) and increases clockwise, matching a clock face */
static fix8_point_t clock_polar(double cx, double cy, double r, double angle)
{
  fix8_point_t p;

  p.x = FLOAT_TO_FIX8(cx + r * sin(angle));
  p.y = FLOAT_TO_FIX8(cy - r * cos(angle));

  return p;
}

static void clock_draw_hand(screen_t *scr,
                            double    cx,
                            double    cy,
                            double    r,
                            double    angle,
                            colour_t  colour)
{
  fix8_point_t tip;

  tip = clock_polar(cx, cy, r, angle);
  screen_draw_line_wu_fix8(scr,
                           FLOAT_TO_FIX8(cx), FLOAT_TO_FIX8(cy),
                           tip.x, tip.y,
                           colour);
}

static result_t clock_create_window(wuss_t       *wuss,
                                    clock_task_t *task,
                                    wuss_task_t  *delegate)
{
  return wuss_window_create_placed(delegate,
                                   SIZE2D(160, 160),
                                   "Clock",
                                   wuss_WINDOW_NONE,
                                   wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                   SIZE2D(160, 160),
                                   SIZE2D(0, 0),
                                   &task->window);
}

result_t clock_create(wuss_t *wuss, clock_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t         rc;

  task->bg          = colour_rgb(0x1D, 0x2B, 0x53);
  task->bezel       = colour_rgb(0xFF, 0xF1, 0xE8);
  task->hand        = colour_rgb(0xFF, 0xF1, 0xE8);
  task->second_hand = colour_rgb(0xFF, 0x00, 0x4D);
  task->show_second = true;

  /* clock_redraw paints its own background every frame */
  delegate_desc.handle    = clock_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "clock";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);

  rc = clock_create_window(wuss, task, delegate);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */

  return rc;
}

static result_t clock_redraw(const wuss_event_t *event, void *task_data)
{
  clock_task_t *cc;
  screen_t     *scr;
  const box_t  *content, *bounds;
  time_t        now;
  struct tm    *lt;
  double        cx, cy, r;
  double        hour_angle, minute_angle, second_angle;
  int           i, sx, sy;

  cc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr,
                   content->x0,
                   content->y0, box_size(content),
                   cc->bg);

  cx = bounds->x0 - sx + (bounds->x1 - bounds->x0) / 2.0;
  cy = bounds->y0 - sy + (bounds->y1 - bounds->y0) / 2.0;
  r  = MIN(bounds->x1 - bounds->x0, bounds->y1 - bounds->y0)
     / 2.0 * CLOCK_FACE_FRACTION;

  /* bezel: a many-sided polygon standing in for the circle */
  {
    fix8_point_t prev, first;

    first = clock_polar(cx, cy, r, 0.0);
    prev  = first;
    for (i = 1; i <= CLOCK_BEZEL_SEGMENTS; i++)
    {
      fix8_point_t p;

      p = clock_polar(cx, cy, r,
                      i * 2.0 * M_PI / CLOCK_BEZEL_SEGMENTS);
      screen_draw_line_wu_fix8(scr, prev.x, prev.y, p.x, p.y, cc->bezel);
      prev = p;
    }
    screen_draw_line_wu_fix8(scr, prev.x, prev.y, first.x, first.y, cc->bezel);
  }

  /* 60 minute ticks, every fifth one an hour tick drawn longer */
  for (i = 0; i < 60; i++)
  {
    double       angle, inner;
    fix8_point_t a, b;

    angle = i * 2.0 * M_PI / 60.0;
    inner = (i % 5 == 0) ? (1.0 - CLOCK_HOUR_TICK) : (1.0 - CLOCK_MINUTE_TICK);
    a = clock_polar(cx, cy, r * inner, angle);
    b = clock_polar(cx, cy, r, angle);
    screen_draw_line_wu_fix8(scr, a.x, a.y, b.x, b.y, cc->bezel);
  }

  now = time(NULL);
  lt  = localtime(&now);

  second_angle = lt->tm_sec * 2.0 * M_PI / 60.0;
  minute_angle = (lt->tm_min + lt->tm_sec / 60.0) * 2.0 * M_PI / 60.0;
  hour_angle   = ((lt->tm_hour % 12) + lt->tm_min / 60.0) * 2.0 * M_PI / 12.0;

  clock_draw_hand(scr, cx, cy, r * CLOCK_HOUR_HAND,   hour_angle,   cc->hand);
  clock_draw_hand(scr, cx, cy, r * CLOCK_MINUTE_HAND, minute_angle, cc->hand);
  if (cc->show_second)
    clock_draw_hand(scr, cx, cy, r * CLOCK_SECOND_HAND, second_angle,
                    cc->second_hand);

  return result_OK;
}

static result_t clock_mouse(clock_task_t *cc, wuss_button_t button)
{
  if (button & wuss_BUTTON_SELECT)
  {
    cc->show_second = !cc->show_second;
    wuss_window_invalidate_all(cc->window);
  }

  return result_OK;
}

result_t clock_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  clock_task_t *cc;

  cc = task_data;

  NOT_USED(window);

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return clock_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return clock_mouse(cc, event->data.mouse.button);

  case wuss_EVENT_IDLE:
    wuss_window_invalidate_all(cc->window);
    return result_OK;

  case wuss_EVENT_QUIT:
    free(cc); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
