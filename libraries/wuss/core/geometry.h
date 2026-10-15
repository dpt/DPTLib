/* wuss/core/geometry.h -- wuss - window geometry and furniture metrics */

#ifndef WUSS_CORE_GEOMETRY_H
#define WUSS_CORE_GEOMETRY_H

#include "types.h"


/* clamp "desired" to the window's scrollable range; step the current offset
 * by "delta" and apply it. Core (furniture-independent) -- used by the wheel
 * and, when built, the scrollbar furniture. */
point_t wuss__scroll_clamp(const wuss_window_t *window, point_t desired);
void    wuss__scroll_step(wuss_window_t *window, point_t delta);

#ifdef WUSS_FURNITURE
void            wuss__titlebar_box(const wuss_window_t *window, box_t *out);
void            wuss__title_hit_box(const wuss_window_t *window, box_t *out);
void            wuss__close_box(const wuss_window_t *window, box_t *out);
void            wuss__close_hit_box(const wuss_window_t *window, box_t *out);
void            wuss__content_box(const wuss_window_t *window, box_t *out);
#else
/* No furniture: the content area is the whole visible footprint. */
static inline void wuss__content_box(const wuss_window_t *window, box_t *out)
{
  *out = window->visible;
}
#endif

/* Map screen point "p" to "window"'s virtual document space: the one place
 * the content origin and scroll offset are applied, so hit-testing,
 * delivery and hover cannot drift apart. */
static inline point_t wuss__screen_to_doc(const wuss_window_t *window,
                                          point_t              p)
{
  box_t content;

  wuss__content_box(window, &content);
  return POINT(p.x - content.x0 + window->scroll.x,
               p.y - content.y0 + window->scroll.y);
}

static inline int wuss__size_ok(int width, int height)
{
  return width > 0 && height > 0;
}

#ifdef WUSS_FURNITURE
static inline int wuss__titlebar_height_for(const wuss_t       *wuss,
                                            wuss_window_flags_t flags)
{
  return (flags & wuss_WINDOW_NO_TITLEBAR) ? 0 : wuss->titlebar_height;
}

static inline int wuss__titlebar_height(const wuss_window_t *window)
{
  return wuss__titlebar_height_for(window->wuss, window->flags);
}

static inline int wuss__window_toggled(const wuss_window_t *window)
{
  return (window->state & wuss_WINDOW_STATE_TOGGLED) != 0;
}

static inline void wuss__window_set_toggled(wuss_window_t *window,
                                            int            toggled)
{
  if (toggled)
    window->state |= wuss_WINDOW_STATE_TOGGLED;
  else
    window->state &= (wuss_window_state_t) ~wuss_WINDOW_STATE_TOGGLED;
}

static inline int wuss__outline_px_for(wuss_window_flags_t flags)
{
  return (flags & wuss_WINDOW_NO_OUTLINE) ? 0 : 1;
}

static inline int wuss__outline_px(const wuss_window_t *window)
{
  return wuss__outline_px_for(window->flags);
}

/* ponytail: falls back to wuss's own titlebar height when the window has
 * none, so titleless windows that still opt into scrollbars/resize match
 * their titled siblings instead of a hardcoded size; the hardcoded default
 * is only a last-resort floor if even that isn't positive */
static inline int wuss__button_size_for(const wuss_t       *wuss,
                                        wuss_window_flags_t flags)
{
  int size;

  size = wuss__titlebar_height_for(wuss, flags) - 2 * WUSS_BUTTON_INSET;
  if (size > 0)
    return size;

  size = wuss->titlebar_height - 2 * WUSS_BUTTON_INSET;

  return (size > 0) ? size : WUSS_DEFAULT_TITLEBAR_HEIGHT - 2 * WUSS_BUTTON_INSET;
}

static inline int wuss__button_size(const wuss_window_t *window)
{
  return wuss__button_size_for(window->wuss, window->flags);
}

/* how much of a content box's width/height is furniture (scrollbars, the
 * resize icon's corner), reserved outside the content area rather than
 * carved out of it -- shared by wuss__content_box (subtracts it back off
 * visible) and window creation/resize (add it to visible up front) so the
 * two stay consistent with each other */
static inline void wuss__furniture_carve_for(wuss_window_flags_t flags,
                                             int                 button_size,
                                             point_t            *carve)
{
  carve->x = (flags & wuss_WINDOW_VSCROLL) ? button_size : 0;
  carve->y = (flags & wuss_WINDOW_HSCROLL) ? button_size : 0;

  if ((flags & wuss_WINDOW_RESIZE) &&
      !(flags & wuss_WINDOW_VSCROLL) &&
      !(flags & wuss_WINDOW_HSCROLL))
  {
    carve->x = button_size;
    carve->y = button_size;
  }

  /* where furniture abuts the content area, a rule divides the two */
  if (carve->x > 0)
    carve->x += WUSS_DIVIDER_PX;
  if (carve->y > 0)
    carve->y += WUSS_DIVIDER_PX;
}
#else /* !WUSS_FURNITURE */
/* No furniture: every geometry helper collapses to "no chrome", so the core
 * window create/move/resize maths still compiles and yields visible ==
 * content. */
static inline int wuss__titlebar_height_for(const wuss_t       *wuss,
                                            wuss_window_flags_t flags)
{
  (void) wuss; (void) flags;
  return 0;
}

static inline int wuss__titlebar_height(const wuss_window_t *window)
{
  (void) window;
  return 0;
}

static inline int wuss__outline_px_for(wuss_window_flags_t flags)
{
  (void) flags;
  return 0;
}

static inline int wuss__outline_px(const wuss_window_t *window)
{
  (void) window;
  return 0;
}

static inline int wuss__button_size_for(const wuss_t       *wuss,
                                        wuss_window_flags_t flags)
{
  (void) wuss; (void) flags;
  return 0;
}

static inline int wuss__button_size(const wuss_window_t *window)
{
  (void) window;
  return 0;
}

static inline void wuss__furniture_carve_for(wuss_window_flags_t flags,
                                             int                 button_size,
                                             point_t            *carve)
{
  (void) flags; (void) button_size;
  carve->x = 0;
  carve->y = 0;
}
#endif /* WUSS_FURNITURE */

/* The floor a resize-drag or toggle-size will shrink a window's content to:
 * the client's min_doc where it set one, but never below what the window's
 * furniture needs -- its titlebar icons side by side, both arrows of each
 * scrollbar -- nor WUSS_MIN_CONTENT (a window must stay big enough to grab).
 * Nor above the window's own doc extent (a window can't be forced larger than
 * the document it shows) unless the furniture needs more than that. */
static inline void wuss__min_content(const wuss_window_t *window,
                                     size2d_t            *min)
{
  wuss_window_flags_t flags;
  int                 size;
  point_t             carve;
  size2d_t            need;
  int                 icons;

  flags = window->flags;
  size  = wuss__button_size(window);
  wuss__furniture_carve_for(flags, size, &carve);

  need.w = WUSS_MIN_CONTENT;
  need.h = WUSS_MIN_CONTENT;

  /* the titlebar spans the content plus carve.x and lays its icons out
   * WUSS_BUTTON_INSET apart, with an inset at each end */
  if (wuss__titlebar_height(window) > 0)
  {
    icons  = ((flags & wuss_WINDOW_BACK)        != 0) +
             ((flags & wuss_WINDOW_CLOSE)       != 0) +
             ((flags & wuss_WINDOW_TOGGLE_SIZE) != 0);
    need.w = MAX(need.w, WUSS_BUTTON_INSET + icons * (size + WUSS_BUTTON_INSET) - carve.x);
  }

  /* a scrollbar holds an arrow at each end, each behind a divider */
  if (flags & wuss_WINDOW_VSCROLL)
    need.h = MAX(need.h, 2 * (size + WUSS_DIVIDER_PX));
  if (flags & wuss_WINDOW_HSCROLL)
    need.w = MAX(need.w, 2 * (size + WUSS_DIVIDER_PX));

  min->w = CLAMP(window->min_doc.w, need.w, MAX(window->doc.w, need.w));
  min->h = CLAMP(window->min_doc.h, need.h, MAX(window->doc.h, need.h));
}

/* Largest content width/height whose visible box (content + outline +
 * titlebar + scrollbar/resize carve) still fits the screen from the
 * window's current top-left. Never returns below WUSS_MIN_CONTENT: a
 * window jammed hard against the far edge stays grabbable even though it
 * then overhangs. Shared by wuss_window_create and wuss_window_resize so
 * no path can produce a window larger than the screen. */
static inline void wuss__max_content_on_screen(const wuss_window_t *window,
                                               size2d_t            *max)
{
  int     outline_px, titlebar_height;
  point_t carve;

  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  max->w = window->wuss->scr->size.w - window->visible.x0
         - 2 * outline_px - carve.x;
  max->h = window->wuss->scr->size.h - window->visible.y0
         - 2 * outline_px - titlebar_height - carve.y;

  max->w = MAX(max->w, WUSS_MIN_CONTENT);
  max->h = MAX(max->h, WUSS_MIN_CONTENT);
}

/* Largest content width/height whose visible box fits the screen when the
 * window may be repositioned -- i.e. the whole screen less chrome, with no
 * allowance for the window's current top-left. Toggle-size uses this and
 * then nudges the top-left toward the origin by the minimum needed to fit.
 * Floored at WUSS_MIN_CONTENT like wuss__max_content_on_screen.
 * ponytail: a named helper (~6 lines) to stay parallel with its sibling
 * above rather than open-coded into toggle-action.c. */
static inline void wuss__max_content_anywhere_on_screen(const wuss_window_t *window,
                                                        size2d_t            *max)
{
  int     outline_px, titlebar_height;
  point_t carve;

  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  max->w = window->wuss->scr->size.w - 2 * outline_px - carve.x;
  max->h = window->wuss->scr->size.h
         - 2 * outline_px - titlebar_height - carve.y;

  max->w = MAX(max->w, WUSS_MIN_CONTENT);
  max->h = MAX(max->h, WUSS_MIN_CONTENT);
}

/* Nudge window->visible back onto the screen by the minimum needed to bring
 * its top-left (titlebar/close-icon) edge into view: shift right/down if it
 * is off the top or left, shift left/up if it hangs off the right or bottom,
 * but never so far that the top-left goes off the opposite edge -- a window
 * bigger than the screen keeps its top-left on and overhangs bottom-right.
 * Only the position moves, not the size. Shared by wuss_window_create and the
 * menu code's borrowed-window opener so no menu can place a window off-screen.
 */
static inline void wuss__nudge_visible_onscreen(wuss_window_t *window)
{
  int scr_width, scr_height, dx, dy;

  scr_width  = window->wuss->scr->size.w;
  scr_height = window->wuss->scr->size.h;

  dx = 0;
  if (window->visible.x0 < 0)
    dx = -window->visible.x0;
  else if (window->visible.x1 > scr_width)
    dx = scr_width - window->visible.x1;
  if (window->visible.x0 + dx < 0)
    dx = -window->visible.x0;

  dy = 0;
  if (window->visible.y0 < 0)
    dy = -window->visible.y0;
  else if (window->visible.y1 > scr_height)
    dy = scr_height - window->visible.y1;
  if (window->visible.y0 + dy < 0)
    dy = -window->visible.y0;

  box_translated(&window->visible, dx, dy, &window->visible);
}

#endif /* WUSS_CORE_GEOMETRY_H */
