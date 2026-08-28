/* ball.c -- wuss test - bouncing ball task */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "ball.h"

result_t ball_create(wuss_t *wuss, const colour_t *palette, ball_task_t *task)
{
  wuss_task_t delegate;
  box_t       box;

  task->bg     = palette[palette_PICO8_RED];
  task->ball   = palette[palette_PICO8_WHITE];
  task->nballs = 1;

  task->balls[0].x      = 50;
  task->balls[0].y      = 50;
  task->balls[0].dx     = 3;
  task->balls[0].dy     = 2;
  task->balls[0].radius = 8;

  delegate = wuss_task_start(ball_handle, task); /* ball_redraw paints its own background every frame */
  box      = (box_t) BOX_POS_SIZE(20, 20, 200, 160);

  return wuss_window_create(wuss,
                            &box,
                            "Bouncing Ball",
                            wuss_WINDOW_NONE,
                            wuss_NO_BACKGROUND,
                            &delegate,
                            box_size(&box),
                            SIZE2D(0, 0),
                            &task->window);
}

void ball_destroy(ball_task_t *task)
{
  wuss_window_close(task->window);
}

static result_t ball_redraw(const wuss_event_t *event, void *task_data)
{
  ball_task_t *bc;
  screen_t    *scr;
  const box_t *content, *bounds;
  int          i, sx, sy;

  bc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_draw_rect(scr, content->x0, content->y0, box_size(content),
                   bc->bg);

  for (i = 0; i < bc->nballs; i++)
  {
    const ball_t *b;

    b = &bc->balls[i];

    screen_draw_rect(scr, bounds->x0 - sx + b->x - b->radius, bounds->y0 - sy + b->y - b->radius, SIZE2D(b->radius * 2, b->radius * 2), bc->ball);
  }

  return result_OK;
}

static result_t ball_mouse(wuss_window_t      *window,
                           wuss_mouse_action_t action,
                           int                 x,
                           int                 y,
                           wuss_button_t       button,
                           void               *task_data)
{
  ball_task_t *bc;
  box_t        local;
  point_t      scroll;

  bc = task_data;

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  wuss_window_get_scroll(window, &scroll);

  if (button & wuss_BUTTON_SELECT)
  {
    ball_t *b;

    if (bc->nballs >= BALL_MAX)
      return result_OK;

    /* x,y already arrive in virtual content space, as ball positions are
     * held; only the invalidation boxes below need the scroll offset taking
     * back off to reach window-local coordinates. */
    b         = &bc->balls[bc->nballs++];
    b->x      = x;
    b->y      = y;
    b->dx     = (bc->nballs & 1) ? 3 : -3;
    b->dy     = (bc->nballs & 2) ? 2 : -2;
    b->radius = 8;

    local.x0 = b->x - b->radius - scroll.x;
    local.y0 = b->y - b->radius - scroll.y;
    local.x1 = b->x + b->radius - scroll.x;
    local.y1 = b->y + b->radius - scroll.y;
    wuss_window_invalidate(bc->window, &local);
  }
  else if (button & wuss_BUTTON_ADJUST)
  {
    ball_t *b;

    if (bc->nballs <= 1)
      return result_OK; /* keep at least one ball on screen */

    b = &bc->balls[--bc->nballs];

    local.x0 = b->x - b->radius - scroll.x;
    local.y0 = b->y - b->radius - scroll.y;
    local.x1 = b->x + b->radius - scroll.x;
    local.y1 = b->y + b->radius - scroll.y;
    wuss_window_invalidate(bc->window, &local);
  }

  return result_OK;
}

static result_t ball_idle(void *task_data)
{
  ball_task_t *bc;
  box_t        content;
  point_t      scroll;
  int          width, height;
  int          i;

  bc = task_data;

  wuss_window_get_content_bounds(bc->window, &content);
  wuss_window_get_scroll(bc->window, &scroll);
  width  = content.x1 - content.x0;
  height = content.y1 - content.y0;

  for (i = 0; i < bc->nballs; i++)
  {
    ball_t *b;
    box_t   local;
    int     old_x, old_y;

    b = &bc->balls[i];

    old_x = b->x;
    old_y = b->y;

    b->x += b->dx;
    b->y += b->dy;

    if (b->x - b->radius < scroll.x)              { b->x = scroll.x + b->radius;          b->dx = -b->dx; }
    else if (b->x + b->radius > scroll.x + width) { b->x = scroll.x + width - b->radius;  b->dx = -b->dx; }
    if (b->y - b->radius < scroll.y)              { b->y = scroll.y + b->radius;          b->dy = -b->dy; }
    else if (b->y + b->radius > scroll.y + height){ b->y = scroll.y + height - b->radius; b->dy = -b->dy; }

    local.x0 = MIN(old_x, b->x) - b->radius - scroll.x;
    local.y0 = MIN(old_y, b->y) - b->radius - scroll.y;
    local.x1 = MAX(old_x, b->x) + b->radius - scroll.x;
    local.y1 = MAX(old_y, b->y) + b->radius - scroll.y;

    wuss_window_invalidate(bc->window, &local);
  }

  return result_OK;
}

result_t ball_handle(wuss_window_t     *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  ball_task_t *bc;

  bc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return ball_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return ball_mouse(window, event->data.mouse.action, event->data.mouse.point.x,
                      event->data.mouse.point.y, event->data.mouse.button, task_data);

  case wuss_EVENT_IDLE:
    return ball_idle(task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_close(window);
    bc->window = NULL;
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
