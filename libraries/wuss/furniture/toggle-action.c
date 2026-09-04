/* wuss/furniture/toggle-action.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

void wuss__furniture_toggle_size(wuss_window_t *window)
{
  box_t before, new_visible, dirty, copied, blit_src, blit_dst;
  int   dx, dy, blitted;

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

  /* The window content is just anchored positions plus furniture, so
   * wherever "before" and the new footprint overlap the pixels only need
   * sliding by the top-left delta -- one screen_copy_rect -- with a real
   * repaint of just the newly-exposed and vacated regions. Only for a
   * topmost window (nothing above it, so no occluder pixels to preserve or
   * step on) whose screen format can blit. A pure grow-in-place is the
   * dx==dy==0 case of the same shift. */
  dx      = window->visible.x0 - before.x0;
  dy      = window->visible.y0 - before.y0;
  blitted = 0;

  if (!(window->flags & wuss_WINDOW_NO_RESIZE_BLIT) &&
      window->wuss->z_order.next == &window->link)
  {
    box_t shifted;

    /* the part of "before" that, slid by (dx,dy), lands within the new
     * footprint: its pixels are the valid source for that overlap */
    box_translated(&window->visible, -dx, -dy, &shifted);
    if (box_intersection(&before, &shifted, &blit_src) == 0 &&
        !box_is_empty(&blit_src))
    {
      box_translated(&blit_src, dx, dy, &blit_dst);
      if (screen_copy_rect(window->wuss->scr, &blit_src,
                           POINT(blit_dst.x0, blit_dst.y0),
                           &copied) == result_OK)
        blitted = 1;
    }
  }

  if (blitted)
  {
    /* "before" minus the pixels the blit left valid at their new home: the
     * vacated L-shape (and any bit whose new position fell off-screen). */
    wuss__invalidate_minus(window->wuss, &before, &copied);
    /* the new footprint minus what the blit filled: the newly-exposed area. */
    wuss__invalidate_minus(window->wuss, &window->visible, &copied);

    /* Furniture lays itself out relative to the window's size/position
     * (titlebar icons on the right edge, scrollbar proportions, the resize
     * corner), so it reflows even where the blit reused valid content
     * pixels. Force the new furniture dirty always, and the old furniture
     * positions too when the window grew or moved -- either can strand a
     * stale scrollbar/icon glyph inside what is now interior content, a
     * region neither invalidate_minus above repaints. */
    if (dx != 0 || dy != 0 ||
        window->visible.x1 - window->visible.x0 > before.x1 - before.x0 ||
        window->visible.y1 - window->visible.y0 > before.y1 - before.y0)
      wuss__furniture_invalidate_for(window, &before);
    wuss__furniture_invalidate(window);
  }
  else
  {
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
}
