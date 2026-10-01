/* wuss/window/set-stack.c -- wuss - minimal window manager */

#include <assert.h>

#include "../impl.h"

void wuss_window_set_stack(wuss_window_t *window, wuss_stack_t stack)
{
  wuss_t *wuss;
  int     hidden, forward;

  assert((int) stack >= 0 && (int) stack < WUSS_STACK_COUNT);

  if (window->stack == stack)
    return;

  wuss    = window->wuss;
  hidden  = (window->flags & wuss_WINDOW_HIDDEN) != 0;
  forward = stack < window->stack; /* moving in front of more windows */

  /* same before/after split as wuss_window_restack: a window coming forward
   * invalidates what is about to be uncovered, one going back invalidates
   * what the reorder has just covered. A hidden window paints nothing. */
  if (forward && !hidden)
    wuss__invalidate_uncovered(window);

  list_remove(&wuss->z_order[window->stack], &window->link);
  window->stack = stack;
  list_add_to_head(&wuss->z_order[stack], &window->link);

  if (!forward && !hidden)
    wuss__invalidate_uncovered(window);
}
