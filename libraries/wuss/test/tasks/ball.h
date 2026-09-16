/* wuss/test/tasks/ball.h -- bouncing ball task */

#ifndef TASKS_BALL_H
#define TASKS_BALL_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/window.h"

#define BALL_MAX 32 /* Select clicks beyond this are ignored */

/* one bouncing ball, centre in local content coords */
typedef struct ball
{
  int      x, y;
  int      dx, dy;
  int      radius;
  colour_t colour;
}
ball_t;

/* window A's task: balls that bounce off the content box's edges, so the
 * redraw loop has something moving to repaint every frame. A Select click
 * adds a ball at the click position; an Adjust click removes the most
 * recently added one. */
typedef struct ball_task
{
  wuss_window_t *window;
  colour_t       bg;
  ball_t         balls[BALL_MAX];
  int            nballs;
}
ball_task_t;

wuss_window_fn_t ball_handle;

/* create the bouncing-ball window against the given wuss instance; the task
 * block is allocated here, owned by the window, and freed when it closes.
 * if out is non-NULL, the task block is also returned through it */
result_t ball_create(wuss_t *wuss, ball_task_t **out);

/* free a task block allocated by ball_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void ball_destroy(ball_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_BALL_H */
