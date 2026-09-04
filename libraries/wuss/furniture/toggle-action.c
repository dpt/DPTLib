/* wuss/furniture/toggle-action.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

void wuss__furniture_toggle_size(wuss_window_t *window)
{
  box_t before, new_visible, dirty, copied;

  before = window->visible;

  if (wuss__window_toggled(window))
  {
    new_visible = window->pre_toggle;
  }
  else
  {
    int      outline_px, titlebar_height, target_w, target_h, vis_w, vis_h, x0, y0;
    point_t  carve;
    size2d_t min, screen_max;

    outline_px      = wuss__outline_px(window);
    titlebar_height = wuss__titlebar_height(window);
    wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

    /* Toggle grows the window to the whole screen (not just the space left
     * from its current top-left), capped per axis at the window's own doc
     * extent -- a doc of 0 on an axis means "no cap", fill the screen.
     * screen_max already accounts for outline/titlebar/scrollbar furniture
     * sitting outside the content area. */
    wuss__max_content_anywhere_on_screen(window, &screen_max);

    target_w = window->doc.w ? MIN(window->doc.w, screen_max.w) : screen_max.w;
    target_h = window->doc.h ? MIN(window->doc.h, screen_max.h) : screen_max.h;

    /* a window with a tiny doc still needs to end up big enough to grab;
     * floor the target like drag-resize.c does, or the toggled box could be
     * smaller than WUSS_MIN_CONTENT and awkward to un-toggle. */
    wuss__min_content(window, &min);
    target_w = MAX(target_w, min.w);
    target_h = MAX(target_h, min.h);

    vis_w = target_w + 2 * outline_px + carve.x;
    vis_h = target_h + titlebar_height + 2 * outline_px + carve.y;

    /* nudge the top-left toward the origin by the minimum needed to fit the
     * grown box on screen; a window that already fits stays put. */
    x0 = window->visible.x0;
    y0 = window->visible.y0;
    if (x0 + vis_w > window->wuss->scr->size.w)
      x0 = window->wuss->scr->size.w - vis_w;
    if (y0 + vis_h > window->wuss->scr->size.h)
      y0 = window->wuss->scr->size.h - vis_h;
    if (x0 < 0)
      x0 = 0;
    if (y0 < 0)
      y0 = 0;

    window->pre_toggle = window->visible;

    new_visible.x0 = x0;
    new_visible.y0 = y0;
    new_visible.x1 = x0 + vis_w;
    new_visible.y1 = y0 + vis_h;
  }

  window->visible = new_visible;
  wuss__window_set_toggled(window, !wuss__window_toggled(window));

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

  /* ponytail: the sliver-blit fast path below assumes the top-left didn't
   * move; toggle can now reposition a window to fit it on screen, so gate
   * it on x0/y0 being unchanged and let the general box_union path handle
   * the moved case. Restore (pre_toggle) can move the top-left too -- same
   * gate covers it. */
  if (!(window->flags & wuss_WINDOW_NO_RESIZE_BLIT) &&
      window->wuss->z_order.next == &window->link &&
      before.x0 == window->visible.x0 && before.y0 == window->visible.y0 &&
      screen_copy_rect(window->wuss->scr, &before,
                       POINT(before.x0, before.y0), &copied) == result_OK)
  {
    /* Topmost, the screen format supports the blit, and the top-left has
     * not moved: re-blitting "before" onto itself is a no-op that just
     * confirms which of its pixels are still on-screen -- only whatever's
     * newly exposed (grown) or newly vacated (shrunk) relative to that
     * needs an actual repaint. */
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
