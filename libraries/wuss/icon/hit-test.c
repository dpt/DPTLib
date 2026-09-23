/* wuss/icon/hit-test.c -- work-area icon hit testing */

#include "geom/box.h"

#include "../core/impl.h"

wuss_icon_t *wuss__icon_hit_test(wuss_window_t *window, point_t doc_point)
{
  wuss_icon_t *it;
  box_t        box;
  int          i;

  /* last created wins, matching the draw order in redraw_window */
  for (i = window->nicons - 1; i >= 0; i--)
  {
    it = window->icons[i];

    /* a bitmap icon is interactive only when it asks to be; RULE and the
     * static types are always inert; the other types always click */
    if (it->spec.type == wuss_ICON_TYPE_BITMAP)
    {
      if (!(it->spec.flags & wuss_ICON_FLAGS_INTERACTIVE))
        continue;
    }
    else if (it->spec.type != wuss_ICON_TYPE_ACTION &&
             it->spec.type != wuss_ICON_TYPE_RADIO &&
             it->spec.type != wuss_ICON_TYPE_OPTION &&
             it->spec.type != wuss_ICON_TYPE_MENU_ENTRY &&
             it->spec.type != wuss_ICON_TYPE_SLIDER &&
             it->spec.type != wuss_ICON_TYPE_WRITABLE)
    {
      continue;
    }

    if (it->spec.flags & (wuss_ICON_FLAGS_HIDDEN | wuss_ICON_FLAGS_DISABLED))
      continue;

    /* only the inner rect (bbox inset by the gap) reacts to clicks; the
     * surround and the gap around it are decorative only */
    if (it->spec.type == wuss_ICON_TYPE_SLIDER)
    {
      box.x0 = it->spec.bbox.x0 + WUSS_SLIDER_GAP;
      box.y0 = it->spec.bbox.y0 + WUSS_SLIDER_GAP;
      box.x1 = it->spec.bbox.x1 - WUSS_SLIDER_GAP;
      box.y1 = it->spec.bbox.y1 - WUSS_SLIDER_GAP;
    }
    else
    {
      box = it->spec.bbox;
    }

    if (box_contains_point(&box, doc_point.x, doc_point.y))
      return it;
  }

  return NULL;
}
