/* toggle-action.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

void wuss__furniture_toggle_size(wuss_window_t *window)
{
  box_t before, new_visible, dirty, copied;

  before = window->visible;

  if (window->toggled)
  {
    new_visible = window->pre_toggle;
  }
  else
  {
    int     outline_px, titlebar_height, available_width, available_height, width, height;
    point_t carve;

    outline_px      = wuss__outline_px(window);
    titlebar_height = wuss__titlebar_height(window);
    wuss__furniture_carve_for(window->flags, wuss__icon_size(window), &carve);

    /* bounded by what's actually left of the screen from the window's
     * current top-left, not the screen's full width/height -- otherwise a
     * window not already at the origin grows straight off the edge. Also
     * account for scrollbar/resize-icon furniture (carve), which sits
     * outside the content area same as create.c/resize.c do, or the window
     * ends up carve.x/carve.y short of the doc size it's meant to reach. */
    available_width  = window->wuss->scr->width  - window->visible.x0 - 2 * outline_px - carve.x;
    available_height = window->wuss->scr->height - window->visible.y0 - 2 * outline_px - titlebar_height - carve.y;

    /* a window dragged far enough off-screen leaves no room at all to the
     * screen edge, so the above can go negative -- floor it like
     * drag-resize.c does, or new_visible ends up with x1/y1 less than
     * x0/y0 (an inverted box), which is never hit-testable again
     * (box_contains_point can't match x1<x0), leaving the window stuck.
     * Floor the available space, not the final width/height below, so a
     * doc smaller than WUSS_MIN_CONTENT still stops at its own size. */
    available_width  = MAX(available_width,  WUSS_MIN_CONTENT);
    available_height = MAX(available_height, WUSS_MIN_CONTENT);

    width  = MIN(window->doc_width,  available_width);
    height = MIN(window->doc_height, available_height);

    window->pre_toggle = window->visible;

    new_visible.x0 = window->visible.x0;
    new_visible.y0 = window->visible.y0;
    new_visible.x1 = new_visible.x0 + width  + 2 * outline_px + carve.x;
    new_visible.y1 = new_visible.y0 + height + titlebar_height + 2 * outline_px + carve.y;
  }

  window->visible = new_visible;
  window->toggled = !window->toggled;

  {
    point_t new_scroll;

    new_scroll = wuss__scroll_clamp(window, window->scroll);
    if (new_scroll.x != window->scroll.x || new_scroll.y != window->scroll.y)
    {
      box_t content;

      /* Don't route this through wuss_window_set_scroll: its topmost
       * live-blit-and-shift optimization assumes the screen already shows
       * this window's content at the old scroll offset, which holds for a
       * genuine scroll but not here -- the window has just been resized and
       * nothing below has repainted yet, so that blit would shift whatever
       * currently happens to be on screen (stale furniture/background
       * pixels included) into the new content area instead of the
       * document. Just store the clamped value and invalidate the content
       * box outright, so the redraw this triggers draws it correctly at
       * the new offset. */
      window->scroll = new_scroll;
      wuss__content_box(window, &content);
      wuss__invalidate_clipped(window, &content);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_RESIZE_BLIT) &&
      window->wuss->z_order.next == &window->link &&
      screen_copy_rect(window->wuss->scr, &before,
                       (point_t) { before.x0, before.y0 }, &copied))
  {
    /* Topmost, and the screen format supports the blit: the window's
     * top-left never moves for a toggle, so re-blitting "before" onto
     * itself is a no-op that just confirms which of its pixels are still
     * on-screen -- only whatever's newly exposed (grown) or newly vacated
     * (shrunk) relative to that needs an actual repaint. */
    wuss__invalidate_minus(window->wuss, &before, &window->visible);
    wuss__invalidate_minus(window->wuss, &window->visible, &copied);

    /* Unlike a move, a toggle changes the window's size, so furniture that
     * lays itself out relative to that size (titlebar icons anchored to its
     * right edge, scrollbar well/sausage proportions, the resize corner)
     * reflows even where the blit reused valid content pixels -- e.g. the
     * old toggle icon location is now mid-titlebar, not redrawn by either
     * invalidate_minus above since it falls inside both "before" and the
     * new "visible". Force it dirty regardless of the blit. */
    if (window->visible.x1 - window->visible.x0 > before.x1 - before.x0 ||
        window->visible.y1 - window->visible.y0 > before.y1 - before.y0)
      /* Growing strands old furniture (e.g. the old vscroll column) inside
       * what's now interior content, a region the blit above treats as
       * already-valid and so never repaints -- force its old position dirty
       * too. Shrinking needs no such help: old furniture positions only
       * ever land outside the new, smaller box, already covered by the
       * invalidate_minus vacated-region call above. */
      wuss__furniture_invalidate_for(window, &before);
    wuss__furniture_invalidate(window);
  }
  else
  {
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
}
