/* ball.h -- wuss test - bouncing ball client */

#ifndef CLIENTS_BALL_H
#define CLIENTS_BALL_H

#ifdef USE_SDL

#include "framebuf/colour.h"
#include "wuss/window.h"

/* window A's client: a ball that bounces off the content box's edges, so
 * the redraw loop has something moving to repaint every frame */
typedef struct ball_client
{
  colour_t bg, ball;
  int      x, y;   /* centre, local content coords */
  int      dx, dy;
  int      radius;
}
ball_client_t;

wuss_redraw_fn_t ball_redraw;

/* move the ball on and invalidate the union of its old and new positions;
 * called once per frame from the main loop, not from a wuss callback */
void ball_step(wuss_window_t *window, ball_client_t *bc);

#endif /* USE_SDL */

#endif /* CLIENTS_BALL_H */
