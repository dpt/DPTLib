/* ball.h -- wuss test - bouncing ball task */

#ifndef TASKS_BALL_H
#define TASKS_BALL_H

#ifdef USE_SDL

#include "framebuf/colour.h"
#include "wuss/window.h"

/* window A's task: a ball that bounces off the content box's edges, so
 * the redraw loop has something moving to repaint every frame */
typedef struct ball_task
{
  wuss_window_t *window;
  colour_t       bg, ball;
  int            x, y;   /* centre, local content coords */
  int            dx, dy;
  int            radius;
}
ball_task_t;

wuss_redraw_fn_t ball_redraw;

/* create the bouncing-ball window against the given wuss instance */
result_t ball_create(wuss_t *wuss, const colour_t *palette, ball_task_t *task);

/* destroy the bouncing-ball window created by ball_create */
void ball_destroy(ball_task_t *task);

/* move the ball on and invalidate the union of its old and new positions;
 * called once per frame from the main loop, not from a wuss callback */
void ball_step(ball_task_t *bc);

#endif /* USE_SDL */

#endif /* TASKS_BALL_H */
