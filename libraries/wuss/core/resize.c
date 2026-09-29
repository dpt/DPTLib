/* wuss/resize.c -- change the screen wuss draws onto at runtime */

#include <assert.h>

#include "base/utils.h"
#include "geom/box.h"

#include "impl.h"
#ifdef WUSS_ICONBAR
#include "../iconbar.h"
#endif

/* Fit one window to the new screen: shrink it only if it is bigger than
 * the whole screen (never growing a window that already fits), then nudge
 * it back on-screen at its current size. Resizing against the current
 * top-left instead would shrink a window hanging off the far edge, which a
 * window without furniture has no way to undo. The nudge uses the same rule
 * as wuss__nudge_visible_onscreen -- computed here, rather than calling it
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

  wuss__max_content_anywhere_on_screen(window, &max);
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

/* A pinned window (the icon bar) is never nudged/shrunk like an ordinary
 * window -- its box is fully determined by the new screen size, so just
 * recompute it directly and invalidate the change. */
static void resize_pinned_window(wuss_window_t *window)
{
  box_t before;

  before = window->visible;

  window->visible.x0 = 0;
  window->visible.y0 = window->wuss->scr->size.h - WUSS_ICONBAR_HEIGHT;
  window->visible.x1 = window->wuss->scr->size.w;
  window->visible.y1 = window->wuss->scr->size.h;

  if (before.x0 != window->visible.x0 || before.y0 != window->visible.y0 ||
      before.x1 != window->visible.x1 || before.y1 != window->visible.y1)
  {
    box_t dirty;

    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
}

result_t wuss_resize(wuss_t *wuss, screen_t *scr)
{
  wuss_window_t *win;
  box_t          screen;

  assert(wuss != NULL);
  assert(scr  != NULL);

  wuss->scr = scr;

#ifdef WUSS_ICONS
  {
    result_t rc;

    rc = wuss__icons_match_screen(wuss);
    if (rc != result_OK)
      return rc;
  }
#endif

  for (win = wuss__z_first(wuss); win != NULL; win = wuss__z_below(win))
  {
    if (win->flags & wuss_WINDOW_PINNED)
      resize_pinned_window(win);
    else
      resize_one_window(win);
  }

  screen.x0 = 0;
  screen.y0 = 0;
  screen.x1 = scr->size.w;
  screen.y1 = scr->size.h;
  wuss_invalidate(wuss, &screen);

  return result_OK;
}
