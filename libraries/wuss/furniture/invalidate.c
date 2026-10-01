/* wuss/furniture/invalidate.c -- wuss - minimal window manager */

#include "../core/impl.h"

/* Same as wuss__furniture_invalidate, but against an arbitrary box rather
 * than window->visible -- lets a caller that's just resized the window
 * (toggle-size) also mark the *old* furniture strips dirty, since a blit
 * that reused the old pixels leaves stale titlebar/scrollbar/outline
 * pixels sitting wherever those strips used to be. The dirty rects marked
 * here -- the titlebar strip, the right carve column (below the titlebar),
 * the bottom carve row (stopping short of the column) and the four outline
 * edges -- union to every pixel of "visible" without overlapping each
 * other, so they also cover the grown furniture hit boxes
 * (furniture/hit-test.c) without wuss_redraw_dirty ever painting the same
 * pixel via two separate dirty rects. */
void wuss__furniture_invalidate_for(wuss_window_t *window,
                                    const box_t   *visible)
{
  int     outline_px;
  point_t carve;

  /* Every geometry change routes through here, so it is also the one place
   * the cached furniture layout is dropped. */
  window->furniture_layout.flags &= ~wuss_FURNITURE_LAYOUT__VALID;

  outline_px = wuss__outline_px(window);
  /* Ask for the same carve the layout uses rather than reading the
   * scrollbar flags directly: a window with both scrollbars off but resize
   * on still reserves both strips, for the resize icon and the rules. */
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  if (!(window->flags & wuss_WINDOW_NO_TITLEBAR))
  {
    box_t titlebar;

    titlebar.x0 = visible->x0 + outline_px;
    titlebar.y0 = visible->y0 + outline_px;
    titlebar.x1 = visible->x1 - outline_px;
    titlebar.y1 = titlebar.y0 + wuss__titlebar_height(window);
    wuss__invalidate_clipped(window, &titlebar);
  }

  if (carve.x > 0)
  {
    box_t column;

    column.x1 = visible->x1 - outline_px;
    column.x0 = column.x1 - carve.x; /* includes the interior rule */
    /* starts below the titlebar, not at visible->y0 -- the titlebar strip
     * below already covers that span across the full width (see
     * wuss__titlebar_box), and the real painted carve band starts here too
     * (furniture/layout.c); starting at visible->y0 instead would overlap
     * the titlebar rect and double-paint that span. */
    column.y0 = (window->flags & wuss_WINDOW_NO_TITLEBAR)
              ? visible->y0
              : visible->y0 + outline_px + wuss__titlebar_height(window);
    /* stops short of the bottom outline edge, which is invalidated as its
     * own strip below -- running to visible->y1 would double-invalidate
     * that strip's width. */
    column.y1 = visible->y1 - outline_px;
    wuss__invalidate_clipped(window, &column);
  }

  if (carve.y > 0)
  {
    box_t row;

    row.y1 = visible->y1 - outline_px;
    row.y0 = row.y1 - carve.y; /* includes the interior rule */
    /* starts after the left outline edge, which is invalidated as its own
     * strip below -- starting at visible->x0 would double-invalidate that
     * strip's height. */
    row.x0 = visible->x0 + outline_px;
    /* stops short of the right carve column rather than running the full
     * width -- the vertical column owns that bottom-right corner in the
     * real paint layout (furniture/layout.c's resize-only band stops at
     * content.x1 too), so running row all the way to visible->x1 would
     * double-invalidate the corner square both boxes would otherwise
     * cover. */
    row.x1 = (carve.x > 0) ? (visible->x1 - outline_px - carve.x)
                          : visible->x1;
    wuss__invalidate_clipped(window, &row);
  }

  if (outline_px > 0)
  {
    box_t edge;

    /* left and right own the full height including the corners; top and
     * bottom stop between them rather than running the full width, or the
     * four edges would double-invalidate each corner pixel. */
    edge = *visible;
    edge.x0 += outline_px;
    edge.x1 -= outline_px;
    edge.y1  = edge.y0 + outline_px;
    wuss__invalidate_clipped(window, &edge); /* top */

    edge = *visible;
    edge.x0 += outline_px;
    edge.x1 -= outline_px;
    edge.y0  = edge.y1 - outline_px;
    wuss__invalidate_clipped(window, &edge); /* bottom */

    edge = *visible;
    edge.x1 = edge.x0 + outline_px;
    wuss__invalidate_clipped(window, &edge); /* left */

    edge = *visible;
    edge.x0 = edge.x1 - outline_px;
    wuss__invalidate_clipped(window, &edge); /* right */
  }
}

void wuss__furniture_invalidate(wuss_window_t *window)
{
  wuss__furniture_invalidate_for(window, &window->visible);
}
