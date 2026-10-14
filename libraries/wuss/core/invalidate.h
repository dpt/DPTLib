/* wuss/core/invalidate.h -- wuss - invalidation, clipping and blit helpers */

#ifndef WUSS_CORE_INVALIDATE_H
#define WUSS_CORE_INVALIDATE_H

#include "types.h"


/* Thin wrappers over the furniture_ops dispatch so core call sites that only
 * poke furniture for a redraw don't each need their own #ifdef WUSS_FURNITURE
 * guard -- they compile to nothing in a no-furniture build. The routing hooks
 * (hit_test, toggle_size, drag_*) stay unwrapped: their call sites sit inside
 * furniture-only logic that is already whole-block guarded. Named "chrome"
 * rather than "furniture" to steer clear of the wuss__furniture_* dispatch
 * targets these forward to. */
#ifdef WUSS_FURNITURE
static inline void wuss__chrome_draw(wuss_t        *wuss,
                                     wuss_window_t *window,
                                     const box_t   *full)
{
  wuss->furniture_ops->draw(wuss, window, full);
}

static inline void wuss__chrome_repaint(wuss_window_t *window)
{
  window->wuss->furniture_ops->invalidate(window);
}

static inline void wuss__chrome_repaint_for(wuss_window_t *window,
                                            const box_t   *visible)
{
  window->wuss->furniture_ops->invalidate_for(window, visible);
}

void wuss__invalidate_clipped(wuss_window_t *window, const box_t *box);

/* Just the scrollbar well(s) whose axis actually moved -- the sausage
 * position depends on scroll, so its well needs redrawing when its own axis
 * scrolls, but nothing else in the furniture does, and the other axis's
 * well (if any) is untouched. Narrower than wuss__chrome_repaint, which
 * invalidates the whole titlebar/outline/carve strips. */
static inline void wuss__chrome_repaint_scroll(wuss_window_t *window,
                                               int            dx,
                                               int            dy)
{
  box_t well;

  if (dy != 0 && (window->flags & wuss_WINDOW_VSCROLL))
  {
    wuss__vscroll_well_box(window, &well);
    wuss__invalidate_clipped(window, &well);
  }

  if (dx != 0 && (window->flags & wuss_WINDOW_HSCROLL))
  {
    wuss__hscroll_well_box(window, &well);
    wuss__invalidate_clipped(window, &well);
  }
}

/* Drop just the cached furniture layout, without queuing any dirty region --
 * for a window move, where the caller already handles the repaint but the
 * cache (absolute coords) must be rebuilt for the new position. */
static inline void wuss__chrome_invalidate_layout(wuss_window_t *window)
{
  window->furniture_layout.flags &= ~wuss_FURNITURE_LAYOUT__VALID;
}
#else
static inline void wuss__chrome_draw(wuss_t        *wuss,
                                     wuss_window_t *window,
                                     const box_t   *full)
{
  (void) wuss;
  (void) window;
  (void) full;
}

static inline void wuss__chrome_repaint(wuss_window_t *window)
{
  (void) window;
}

static inline void wuss__chrome_repaint_for(wuss_window_t *window,
                                            const box_t   *visible)
{
  (void) window;
  (void) visible;
}

static inline void wuss__chrome_repaint_scroll(wuss_window_t *window,
                                               int            dx,
                                               int            dy)
{
  (void) window;
  (void) dx;
  (void) dy;
}

static inline void wuss__chrome_invalidate_layout(wuss_window_t *window)
{
  (void) window;
}
#endif

void            wuss__invalidate_clipped(wuss_window_t *window,
                                         const box_t   *box);
void            wuss__invalidate_minus(wuss_t      *wuss,
                                       const box_t *whole,
                                       const box_t *keep);
void            wuss__invalidate_uncovered(wuss_window_t *window);
void            wuss__touch(wuss_t *wuss, const box_t *box);

/* Clip "box" (screen space) down to the parts not already covered by
 * windows above "window" in the z-order, writing the surviving pieces to
 * "out" (capacity WUSS_MAX_INVALIDATE_PIECES) and returning their count.
 *
 * "overpaint_safe" picks the overflow fallback direction, same as
 * wuss__subtract_boxes: pass 1 when the result only feeds a paint or
 * invalidate (over-including is just wasted repaint work), 0 when it
 * feeds a blit source (over-including would copy occluded pixels). */
int             wuss__clip_to_visible(wuss_window_t *window,
                                      const box_t   *box,
                                      box_t         *out,
                                      int            overpaint_safe);

/* Subtract each of "cuts" (an array of "ncuts" boxes) from "whole", writing
 * the surviving pieces to "out" (capacity WUSS_MAX_INVALIDATE_PIECES) and
 * returning their count.
 *
 * A carve that needs more than WUSS_MAX_INVALIDATE_PIECES pieces falls back
 * to either "whole" unfragmented (overpaint_safe true: the result only ever
 * gets painted/invalidated, and nothing downstream depends on it excluding
 * the cuts, so claiming the cuts contributed nothing just repaints a bit
 * more) or to zero pieces (overpaint_safe false: the result is a blit
 * source or other "known-good pixels" set that must exclude every cut, so
 * claiming survivors where fragmentation was actually too complex to prove
 * would copy or keep pixels the cuts should have excluded). */
int             wuss__subtract_boxes(const box_t *whole,
                                     const box_t *cuts,
                                     int          ncuts,
                                     box_t       *out,
                                     int          overpaint_safe);

/* Filter "clean" (nclean pieces already clipped clear of occluders) down to
 * the parts not also covered by "stale" (nstale pending-dirty boxes not yet
 * repainted), writing survivors back into "clean" and returning the new
 * count. Each piece is subtracted independently, so the result can hold more
 * than "nclean" entries; excess beyond WUSS_MAX_INVALIDATE_PIECES is
 * dropped. */
int             wuss__filter_settled(box_t       *clean,
                                     int          nclean,
                                     const box_t *stale,
                                     int          nstale);

/* Given "n" single-rect blits, each moving "clean[i]" to "dest[i]", find an
 * order in which no blit's destination overwrites a still-unread source of a
 * later blit. Writes the piece indices to "order" (capacity
 * WUSS_MAX_INVALIDATE_PIECES) and returns non-zero on success; returns zero
 * when the overlap graph has a cycle and no safe order exists. */
int             wuss__order_pieces(const box_t *clean,
                                   const box_t *dest,
                                   int          n,
                                   int         *order);

/* Slide the pixels of "src" (nsrc pieces the caller has already carved clear
 * of occluders and stale regions) by (dx, dy), clipping each destination
 * against the screen and the windows above "window", clobber-ordered so no
 * blit steps on another's source; if "clip" is non-NULL scr->clip is pinned
 * to it for the copy (and restored after), else the clip is left alone. On
 * success
 * fills "copied" (capacity WUSS_MAX_INVALIDATE_PIECES) with the pieces
 * actually blitted, sets "*ncopied" and returns 1 -- the caller repaints
 * whatever "copied" misses. Returns 0 (with "*ncopied" zeroed) when there is
 * no safe fast path, and the caller must fall back to a full invalidate. */
int             wuss__blit_pieces(wuss_window_t *window,
                                  const box_t   *src,
                                  int            nsrc,
                                  int            dx,
                                  int            dy,
                                  const box_t   *clip,
                                  box_t         *copied,
                                  int           *ncopied);

#endif /* WUSS_CORE_INVALIDATE_H */
