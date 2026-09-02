/* wuss/test/tasks/ball.c -- bouncing ball task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "ball.h"

#define BALL_BASE_RADIUS 8 /* +/-50% at spawn -> 4..12 */

/* a fresh radius in [BALL_BASE_RADIUS/2, BALL_BASE_RADIUS*3/2] */
static int ball_random_radius(void)
{
  return BALL_BASE_RADIUS / 2 + rand() % (BALL_BASE_RADIUS + 1);
}

/* a fresh fully-opaque colour, red channel is at least 0x40 so it stays
 * visible against the red background */
static colour_t ball_random_colour(void)
{
  int i;
  
  i = 0x40 + rand() % 0xC0;
  return colour_rgb(0xFF, i, i);
}

/* Invalidation box (virtual content space, as ball positions are held)
 * covering a ball -- or its swept range -- whose centre spans
 * [vx0,vx1] x [vy0,vy1]. wuss_window_invalidate maps this to the screen and
 * applies scroll, so this must not. The circle occupies [c-radius,c+radius]
 * inclusive, i.e. 2*radius+1 pixels; box_t is half-open so x1/y1 get the
 * extra 1. */
static box_t ball_local_box(int vx0, int vy0, int vx1, int vy1, int radius)
{
  box_t local;

  local.x0 = vx0 - radius;
  local.y0 = vy0 - radius;
  local.x1 = vx1 + radius + 1;
  local.y1 = vy1 + radius + 1;

  return local;
}

result_t ball_create(wuss_t *wuss, ball_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t         rc;

  task->bg     = colour_rgb(0xFF, 0x00, 0x00);
  task->nballs = 1;

  task->balls[0].x      = 50;
  task->balls[0].y      = 50;
  task->balls[0].dx     = 3;
  task->balls[0].dy     = 2;
  task->balls[0].radius = ball_random_radius();
  task->balls[0].colour = ball_random_colour();

  /* ball_redraw paints its own background every frame */
  delegate_desc.handle    = ball_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "ball";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(200, 160),
                                 "Bouncing Ball",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 SIZE2D(200, 160),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */

  return rc;
}

static result_t ball_redraw(const wuss_event_t *event, void *task_data)
{
  ball_task_t *bc;
  screen_t    *scr;
  const box_t *content, *bounds;
  int          sx, sy;
  int          i;

  bc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content),
                   bc->bg);

  for (i = 0; i < bc->nballs; i++)
  {
    const ball_t *b;

    b = &bc->balls[i];

    screen_fill_circle(scr, bounds->x0 - sx + b->x,
                            bounds->y0 - sy + b->y,
                            b->radius,
                            b->colour);
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

  bc = task_data;

  NOT_USED(window);

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST))
  {
    ball_t *b;
    
    if (button & wuss_BUTTON_SELECT)
    {
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
      b->radius = ball_random_radius();
      b->colour = ball_random_colour();
    }
    else if (button & wuss_BUTTON_ADJUST)
    {
      if (bc->nballs <= 1)
        return result_OK; /* keep at least one ball on screen */
      
      b = &bc->balls[--bc->nballs];
    }
    
    local = ball_local_box(b->x, b->y, b->x, b->y, b->radius);
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

    local = ball_local_box(MIN(old_x, b->x), MIN(old_y, b->y),
                           MAX(old_x, b->x), MAX(old_y, b->y),
                           b->radius);

    wuss_window_invalidate(bc->window, &local);
  }

  return result_OK;
}

result_t ball_handle(wuss_window_t      *window,
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

  case wuss_EVENT_QUIT:
    free(bc); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
