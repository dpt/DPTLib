/* wuss/resize.c -- change the screen wuss draws onto at runtime */

#include <assert.h>

#include "base/utils.h"
#include "geom/box.h"

#include "impl.h"

/* Shrink then reposition one window to fit the new screen, mirroring
 * wuss_window_create's own two-step clamp: the far corner is capped from
 * the window's current top-left first (never growing a window that already
 * fits), then the position is nudged back on-screen by the same rule as
 * wuss__nudge_visible_onscreen -- computed here, rather than calling it
 * directly, since that helper mutates window->visible in place and would
 * desync it from the public wuss_window_move/wuss_window_resize calls
 * needed for their invalidation and packer bookkeeping. */
static void resize_one_window(wuss_window_t *window)
{
  box_t    content;
  point_t  origin;
  size2d_t size, max;
  int      scr_width, scr_height;

  wuss_window_get_content_bounds(window, &content);
  origin.x = content.x0;
  origin.y = content.y0;
  size.w   = content.x1 - content.x0;
  size.h   = content.y1 - content.y0;

  wuss__max_content_on_screen(window, &max);
  if (size.w > max.w || size.h > max.h)
  {
    size.w = MIN(size.w, max.w);
    size.h = MIN(size.h, max.h);
    wuss_window_resize(window, size);
  }

  scr_width  = window->wuss->scr->size.w;
  scr_height = window->wuss->scr->size.h;
  wuss_window_get_visible_bounds(window, &content);

  if (content.x0 < 0)
    origin.x -= content.x0;
  else if (content.x1 > scr_width)
    origin.x -= MIN(content.x0, content.x1 - scr_width);

  if (content.y0 < 0)
    origin.y -= content.y0;
  else if (content.y1 > scr_height)
    origin.y -= MIN(content.y0, content.y1 - scr_height);

  wuss_window_move(window, origin);
}

result_t wuss_resize(wuss_t *wuss, screen_t *scr)
{
  list_t *e;
  box_t   screen;

  assert(wuss != NULL);
  assert(scr  != NULL);

  wuss->scr = scr;

  for (e = wuss->z_order.next; e != NULL; e = e->next)
    resize_one_window(wuss__window_from_link(e));

  screen.x0 = 0;
  screen.y0 = 0;
  screen.x1 = scr->size.w;
  screen.y1 = scr->size.h;
  wuss_invalidate(wuss, &screen);

  return result_OK;
}
