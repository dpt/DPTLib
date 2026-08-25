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

struct wuss
{
  screen_t      *scr;
  bmfont_t      *font;      /* nullable, not owned */
  colour_t      *palette;   /* owned */
  int            npalette;
  wuss_colour_t  titlebar_bg;
  wuss_colour_t  titlebar_fg;
  int            titlebar_height;
  list_t         z_order;   /* anchor; head = topmost window */
  wuss_window_t *dragging;  /* NULL when idle */
  int            drag_dx, drag_dy;
  int            drag_moved; /* set once a mouse-move is delivered mid-drag;
                               * a titlebar mouse-down/up with no move in
                               * between is a click, not a drag */
  box_t          dirty[WUSS_MAX_DIRTY]; /* accumulated by wuss_invalidate; reset by a redraw */
  int            ndirty;
};

struct wuss_window
{
  list_t              link;   /* must be first member */
  wuss_t             *wuss;
  box_t               visible; /* full on-screen footprint: content expanded
                                 * outward by any titlebar/outline furniture */
  wuss_task_t         task;
  wuss_window_flags_t flags;
  int                 scroll_x, scroll_y; /* offset into virtual content
                                            * space of the content box's
                                            * top-left; see
                                            * wuss_window_set_scroll */
  char                title[WUSS_TITLE_MAX + 1];
};

wuss_window_t *wuss__window_at(wuss_t *wuss, int x, int y);
void            wuss__titlebar_box(const wuss_window_t *window, box_t *out);
void            wuss__content_box(const wuss_window_t *window, box_t *out);
void            wuss__invalidate_clipped(wuss_window_t *window,
                                         const box_t   *box);
void            wuss__invalidate_minus(wuss_t      *wuss,
                                       const box_t *whole,
                                       const box_t *keep);
void            wuss__invalidate_uncovered(wuss_window_t *window);

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

#endif /* IMPL_H */
