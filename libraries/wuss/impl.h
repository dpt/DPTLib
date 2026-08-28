/* impl.h -- wuss - minimal window manager */

#ifndef IMPL_H
#define IMPL_H

#include "datastruct/list.h"
#include "geom/box.h"
#include "framebuf/screen.h"
#include "framebuf/bmfont.h"

#include "wuss/wuss.h"
#include "wuss/window.h"

#include "furniture.h"

#define WUSS_TITLE_MAX               63
#define WUSS_DEFAULT_TITLEBAR_HEIGHT 20

#define WUSS_MAX_DIRTY 16 /* dirty regions tracked before further invalidations get merged into the last entry */

/* ponytail: fixed cap; if hit, remaining pieces are carried through
 * unclipped against further occluders rather than growing storage -- safe,
 * just some avoidable redraw work, never wrong */
#define WUSS_MAX_INVALIDATE_PIECES 32

#define WUSS_ICON_INSET   3  /* shared by close/back/toggle/resize icons and scrollbar breadth */
#define WUSS_MIN_CONTENT  20 /* resize-drag floor: content can never be squeezed smaller than this */

struct wuss
{
  screen_t                   *scr;
  bmfont_t                   *font;      /* nullable, not owned */
  colour_t                   *palette;   /* owned */
  int                         npalette;
  wuss_palette_t              furniture_colours;
  wuss_colour_t               backdrop;  /* wuss_NO_BACKGROUND for none */
  int                         titlebar_height;
  list_t                      z_order;   /* anchor; head = topmost window */
  struct wuss__furniture      furniture;
  box_t                       dirty[WUSS_MAX_DIRTY]; /* accumulated by wuss_invalidate; reset by a redraw */
  int                         ndirty;
};

struct wuss_window
{
  list_t              link;   /* must be first member */
  wuss_t             *wuss;
  box_t               visible; /* full on-screen footprint: content expanded
                                * outward by any titlebar/outline furniture */
  wuss_task_t         task;
  wuss_colour_t       bg;
  wuss_window_flags_t flags;
  point_t             scroll; /* offset into virtual content space of the
                               * content box's top-left; see wuss_window_set_scroll */
  int                 doc_width, doc_height; /* virtual document extent, set at creation */
  int                 toggled;      /* currently at TOGGLE_SIZE's "full" size? */
  box_t               pre_toggle;   /* visible bounds to restore on the next toggle */
  char                title[WUSS_TITLE_MAX + 1];
};

wuss_window_t *wuss__window_at(wuss_t *wuss, point_t p);
void            wuss__titlebar_box(const wuss_window_t *window, box_t *out);
void            wuss__close_box(const wuss_window_t *window, box_t *out);
void            wuss__content_box(const wuss_window_t *window, box_t *out);
void            wuss__invalidate_clipped(wuss_window_t *window,
                                         const box_t   *box);
void            wuss__invalidate_minus(wuss_t      *wuss,
                                       const box_t *whole,
                                       const box_t *keep);
void            wuss__invalidate_uncovered(wuss_window_t *window);

/* Clip "box" (screen space) down to the parts not already covered by
 * windows above "window" in the z-order, writing the surviving pieces to
 * "out" (capacity WUSS_MAX_INVALIDATE_PIECES) and returning their count. */
int             wuss__clip_to_visible(wuss_window_t *window,
                                      const box_t   *box,
                                      box_t         *out);

/* Subtract each of "cuts" (an array of "ncuts" boxes) from "whole", writing
 * the surviving pieces to "out" (capacity WUSS_MAX_INVALIDATE_PIECES) and
 * returning their count. */
int             wuss__subtract_boxes(const box_t *whole,
                                     const box_t *cuts,
                                     int          ncuts,
                                     box_t       *out);

/* Notify a window's task that it has been moved or resized, via
 * wuss_EVENT_OPEN; the return value is discarded, matching how furniture
 * drawing and other in-line notifications are treated. */
static inline void wuss__notify_open(wuss_window_t *window)
{
  wuss_event_t event;

  if (window->task.handle == NULL)
    return;

  event.kind = wuss_EVENT_OPEN;
  (void) window->task.handle(window, &event, window->task.task_data);
}

static inline int wuss__size_ok(int width, int height)
{
  return width > 0 && height > 0;
}

static inline int wuss__titlebar_height_for(const wuss_t        *wuss,
                                            wuss_window_flags_t flags)
{
  return (flags & wuss_WINDOW_NO_TITLEBAR) ? 0 : wuss->titlebar_height;
}

static inline int wuss__titlebar_height(const wuss_window_t *window)
{
  return wuss__titlebar_height_for(window->wuss, window->flags);
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
 * none, so NO_TITLEBAR windows that still opt into scrollbars/resize match
 * their titled siblings instead of a hardcoded size; the hardcoded default
 * is only a last-resort floor if even that isn't positive */
static inline int wuss__icon_size_for(const wuss_t *wuss, wuss_window_flags_t flags)
{
  int size;

  size = wuss__titlebar_height_for(wuss, flags) - 2 * WUSS_ICON_INSET;
  if (size > 0)
    return size;

  size = wuss->titlebar_height - 2 * WUSS_ICON_INSET;

  return (size > 0) ? size : WUSS_DEFAULT_TITLEBAR_HEIGHT - 2 * WUSS_ICON_INSET;
}

static inline int wuss__icon_size(const wuss_window_t *window)
{
  return wuss__icon_size_for(window->wuss, window->flags);
}

/* how much of a content box's width/height is furniture (scrollbars, the
 * resize icon's corner), reserved outside the content area rather than
 * carved out of it -- shared by wuss__content_box (subtracts it back off
 * visible) and window creation/resize (add it to visible up front) so the
 * two stay consistent with each other */
static inline void wuss__furniture_carve_for(wuss_window_flags_t flags,
                                             int                 icon_size,
                                             point_t            *carve)
{
  carve->x = (flags & wuss_WINDOW_NO_VSCROLL) ? 0 : icon_size;
  carve->y = (flags & wuss_WINDOW_NO_HSCROLL) ? 0 : icon_size;

  if (!(flags & wuss_WINDOW_NO_RESIZE) &&
      (flags & wuss_WINDOW_NO_VSCROLL) &&
      (flags & wuss_WINDOW_NO_HSCROLL))
  {
    carve->x = icon_size;
    carve->y = icon_size;
  }
}

#endif /* IMPL_H */
