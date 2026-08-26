/* impl.h -- wuss - minimal window manager */

#ifndef IMPL_H
#define IMPL_H

#include "datastruct/list.h"
#include "geom/box.h"
#include "framebuf/screen.h"
#include "framebuf/bmfont.h"

#include "wuss/wuss.h"
#include "wuss/window.h"

#define WUSS_TITLE_MAX               63
#define WUSS_DEFAULT_TITLEBAR_HEIGHT 20

#define WUSS_MAX_DIRTY 16 /* dirty regions tracked before further invalidations get merged into the last entry */

/* ponytail: fixed cap; if hit, remaining pieces are carried through
 * unclipped against further occluders rather than growing storage -- safe,
 * just some avoidable redraw work, never wrong */
#define WUSS_MAX_INVALIDATE_PIECES 32

#define WUSS_ICON_INSET   3  /* shared by close/back/toggle/resize icons and scrollbar breadth */
#define WUSS_MIN_THUMB    6  /* scrollbar thumb never shrinks below this, however small the content fraction */
#define WUSS_MIN_CONTENT  20 /* resize-drag floor: content can never be squeezed smaller than this */
#define WUSS_SCROLL_STEP  20 /* pixels stepped per scrollbar arrow click */

/* ----- furniture ----- */

/* Which region of a window's border (or its content) a point falls in. */
typedef enum wuss_furniture_region
{
  wuss_FURNITURE_NONE,
  wuss_FURNITURE_CONTENT,
  wuss_FURNITURE_BACK,
  wuss_FURNITURE_CLOSE,
  wuss_FURNITURE_TITLE,
  wuss_FURNITURE_TOGGLE_SIZE,
  wuss_FURNITURE_VSCROLL_UP,
  wuss_FURNITURE_VSCROLL_BAR,
  wuss_FURNITURE_VSCROLL_DOWN,
  wuss_FURNITURE_RESIZE,
  wuss_FURNITURE_HSCROLL_RIGHT,
  wuss_FURNITURE_HSCROLL_BAR,
  wuss_FURNITURE_HSCROLL_LEFT
}
wuss_furniture_region_t;

/* Which kind of drag a furniture region starts, if any. */
typedef enum wuss_furniture_drag_kind
{
  wuss_FURNITURE_DRAG_NONE,
  wuss_FURNITURE_DRAG_MOVE,
  wuss_FURNITURE_DRAG_RESIZE,
  wuss_FURNITURE_DRAG_VSCROLL_THUMB,
  wuss_FURNITURE_DRAG_HSCROLL_THUMB
}
wuss_furniture_drag_kind_t;

static inline wuss_furniture_drag_kind_t wuss__furniture_drag_kind(wuss_furniture_region_t region)
{
  switch (region)
  {
  case wuss_FURNITURE_TITLE:
  case wuss_FURNITURE_CLOSE:
    return wuss_FURNITURE_DRAG_MOVE;
  case wuss_FURNITURE_RESIZE:
    return wuss_FURNITURE_DRAG_RESIZE;
  case wuss_FURNITURE_VSCROLL_BAR:
    return wuss_FURNITURE_DRAG_VSCROLL_THUMB;
  case wuss_FURNITURE_HSCROLL_BAR:
    return wuss_FURNITURE_DRAG_HSCROLL_THUMB;
  default:
    return wuss_FURNITURE_DRAG_NONE;
  }
}

struct wuss
{
  screen_t                   *scr;
  bmfont_t                   *font;      /* nullable, not owned */
  colour_t                   *palette;   /* owned */
  int                         npalette;
  wuss_colour_t               titlebar_bg;
  wuss_colour_t               titlebar_fg;
  int                         titlebar_height;
  list_t                      z_order;   /* anchor; head = topmost window */
  wuss_window_t              *dragging;  /* NULL when idle */
  wuss_furniture_drag_kind_t  drag_kind;
  point_t                     drag; /* MOVE: pointer offset within content;
                                     * RESIZE: unused, recomputed each move;
                                     * *_THUMB: pointer position at drag start */
  int                         drag_scroll_start; /* *_THUMB: scroll.x/scroll.y at drag start */
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

/* ----- furniture ----- */

wuss_furniture_region_t wuss__furniture_hit_test(const wuss_window_t *window,
                                                 point_t              p);
void wuss__furniture_draw(wuss_t        *wuss,
                          wuss_window_t *window,
                          const box_t   *full);
void wuss__furniture_invalidate(wuss_window_t *window);
void wuss__furniture_invalidate_for(wuss_window_t *window, const box_t *visible);

/* geometry: titlebar icons */
void wuss__back_box(const wuss_window_t *window, box_t *out);
void wuss__toggle_box(const wuss_window_t *window, box_t *out);
void wuss__resize_box(const wuss_window_t *window, box_t *out);

/* geometry: vertical scrollbar */
void wuss__vscroll_up_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_down_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_bar_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_thumb_box(const wuss_window_t *window, box_t *out);
int  wuss__vscroll_track_px(const wuss_window_t *window);

/* geometry: horizontal scrollbar */
void wuss__hscroll_left_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_right_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_bar_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_thumb_box(const wuss_window_t *window, box_t *out);
int  wuss__hscroll_track_px(const wuss_window_t *window);

/* actions */
void wuss__furniture_toggle_size(wuss_window_t *window);
void wuss__furniture_scroll_step(wuss_window_t *window, point_t delta);
point_t wuss__scroll_clamp(const wuss_window_t *window, point_t desired);
void wuss__furniture_drag_resize(wuss_window_t *window, point_t p);
void wuss__furniture_drag_thumb(wuss_window_t *window,
                                int            delta_px,
                                int            scroll_start,
                                int            horizontal);

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

/* ponytail: falls back to the default titlebar height when the window has
 * none, so NO_TITLEBAR windows that still opt into scrollbars/resize get a
 * sane breadth rather than a negative one */
static inline int wuss__icon_size_for(const wuss_t *wuss, wuss_window_flags_t flags)
{
  int size;

  size = wuss__titlebar_height_for(wuss, flags) - 2 * WUSS_ICON_INSET;

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
