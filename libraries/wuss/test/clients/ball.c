/* ball.c -- wuss test - bouncing ball client */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"

#include "ball.h"

result_t ball_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  ball_client_t *bc;

  NOT_USED(window);

  bc = client_data;

  screen_draw_rect(scr, content->x0, content->y0,
                   content->x1 - content->x0,
                   content->y1 - content->y0,
                   bc->bg);

  screen_draw_rect(scr, content->x0 + bc->x - bc->radius, content->y0 + bc->y - bc->radius,
                   bc->radius * 2, bc->radius * 2, bc->ball);

  return result_OK;
}

void ball_step(wuss_window_t *window, ball_client_t *bc)
{
  box_t content, local;
  int   width, height;
  int   old_x, old_y;

  wuss_window_get_content_bounds(window, &content);
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

  wuss_window_invalidate(window, &local);
}

#endif /* USE_SDL */
