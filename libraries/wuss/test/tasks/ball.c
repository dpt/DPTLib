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
  task->x      = 50;
  task->y      = 50;
  task->dx     = 3;
  task->dy     = 2;
  task->radius = 8;

  delegate = wuss_task_make(ball_redraw, NULL, task, wuss_NO_BACKGROUND); /* ball_redraw paints its own background every frame */
  box      = (box_t) BOX_POS_SIZE(20, 20, 200, 160);

  return wuss_window_create(wuss, &box, "Bouncing Ball", wuss_WINDOW_NONE, &delegate, &task->window);
}

void ball_destroy(ball_task_t *task)
{
  wuss_window_destroy(task->window);
}

result_t ball_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *task_data)
{
  ball_task_t *bc;

  NOT_USED(window);

  bc = task_data;

  screen_draw_rect(scr, content->x0, content->y0,
                   content->x1 - content->x0,
                   content->y1 - content->y0,
                   bc->bg);

  screen_draw_rect(scr, content->x0 + bc->x - bc->radius, content->y0 + bc->y - bc->radius,
                   bc->radius * 2, bc->radius * 2, bc->ball);

  return result_OK;
}

void ball_step(ball_task_t *bc)
{
  box_t content, local;
  int   width, height;
  int   old_x, old_y;

  wuss_window_get_content_bounds(bc->window, &content);
  width  = content.x1 - content.x0;
  height = content.y1 - content.y0;

  old_x = bc->x;
  old_y = bc->y;

  bc->x += bc->dx;
  bc->y += bc->dy;

  if (bc->x - bc->radius < 0)           { bc->x = bc->radius;          bc->dx = -bc->dx; }
  else if (bc->x + bc->radius > width)  { bc->x = width - bc->radius;  bc->dx = -bc->dx; }
  if (bc->y - bc->radius < 0)           { bc->y = bc->radius;          bc->dy = -bc->dy; }
  else if (bc->y + bc->radius > height) { bc->y = height - bc->radius; bc->dy = -bc->dy; }

  local.x0 = MIN(old_x, bc->x) - bc->radius;
  local.y0 = MIN(old_y, bc->y) - bc->radius;
  local.x1 = MAX(old_x, bc->x) + bc->radius;
  local.y1 = MAX(old_y, bc->y) + bc->radius;

  wuss_window_invalidate(bc->window, &local);
}

#endif /* USE_SDL */
