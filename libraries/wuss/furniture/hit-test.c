/* wuss/furniture/hit-test.c -- wuss - minimal window manager */

#include "../core/impl.h"

/* Attribute a point that is inside window->visible but outside the content box
 * and outside every furniture hit box above to the nearest furniture. Such a
 * point is a bare outline/carve pixel: an edge that has a scrollbar strip
 * running its full length hands off to that strip's well; every other edge
 * pixel (including the carve band left over when only a lone resize corner is
 * present -- the corner itself already has its own hit box) is frame, not a
 * widget, so it resolves to TITLE or CONTENT rather than starting a resize or
 * scrollbar drag from somewhere the user cannot see the handle. */
static wuss_furniture_region_t nearest_edge_region(const wuss_window_t *window,
                                                   point_t              p)
{
  box_t content;
  int   has_v, has_h, has_title;

  wuss__content_box(window, &content);

  has_v     = (window->flags & wuss_WINDOW_VSCROLL) != 0;
  has_h     = (window->flags & wuss_WINDOW_HSCROLL) != 0;
  has_title = (window->flags & wuss_WINDOW_NO_TITLEBAR) == 0;

  /* bottom edge: the hscroll strip runs its full width */
  if (p.y >= content.y1)
    return has_h    ? wuss_FURNITURE_HSCROLL_WELL
         : has_title ? wuss_FURNITURE_TITLE
         : wuss_FURNITURE_CONTENT;

  /* right edge: the vscroll strip runs its full height */
  if (p.x >= content.x1)
    return has_v    ? wuss_FURNITURE_VSCROLL_WELL
         : has_title ? wuss_FURNITURE_TITLE
         : wuss_FURNITURE_CONTENT;

  if (p.y < content.y0)
    return has_title ? wuss_FURNITURE_TITLE : wuss_FURNITURE_CONTENT;

  /* left outline column, level with the content: only the hscroll strip's
   * grown hit box reaches here, and only if there is one */
  return has_h     ? wuss_FURNITURE_HSCROLL_WELL
       : has_title  ? wuss_FURNITURE_TITLE
       : wuss_FURNITURE_CONTENT;
}

/* The furniture hit boxes are grown outward from the content box to tile every
 * pixel of window->visible -- the drawn boxes are the un-suffixed wuss__*_box
 * helpers, these _hit_box variants swallow the outline band, the four corners
 * and the interior divider seam. Grown boxes OVERLAP where they meet, so the
 * first match in wuss__furniture_elements' priority order wins. */
wuss_furniture_region_t wuss__furniture_hit_test(const wuss_window_t *window,
                                                 point_t              p)
{
  box_t box;
  int   i;

  for (i = 0; i < wuss__furniture_nelements; i++)
  {
    const wuss__furniture_element_t *element;

    element = &wuss__furniture_elements[i];
    if (!wuss__furniture_element_present(window, element))
      continue;

    element->hit_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return element->region;
  }

  /* The grown boxes cover every chrome pixel except the outline band on an edge
   * with no scrollbar and no resize icon. Attribute that to the nearest
   * furniture rather than letting it fall through to the workarea. */
  wuss__content_box(window, &box);
  if (box_contains_point(&window->visible, p.x, p.y) &&
      !box_contains_point(&box, p.x, p.y))
    return nearest_edge_region(window, p);

  return wuss_FURNITURE_CONTENT;
}
