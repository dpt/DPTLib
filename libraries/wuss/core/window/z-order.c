/* wuss/window/z-order.c -- wuss - minimal window manager */

#include "../impl.h"

/* First window at or after stack "from", or NULL if those stacks are empty. */
static wuss_window_t *first_from(wuss_t *wuss, int from)
{
  int s;

  for (s = from; s < WUSS_STACK_COUNT; s++)
    if (wuss->z_order[s].next != NULL)
      return wuss__window_from_link(wuss->z_order[s].next);

  return NULL;
}

wuss_window_t *wuss__z_first(wuss_t *wuss)
{
  return first_from(wuss, 0);
}

wuss_window_t *wuss__z_below(const wuss_window_t *window)
{
  if (window->link.next != NULL)
    return wuss__window_from_link(window->link.next);

  return first_from(window->wuss, (int) window->stack + 1);
}
