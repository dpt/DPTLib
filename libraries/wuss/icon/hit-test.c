/* wuss/icon/hit-test.c -- work-area icon hit testing */

#include "geom/box.h"

#include "../core/impl.h"

wuss_icon_t *wuss__icon_hit_test(wuss_window_t *window, point_t doc_point)
{
  wuss_icon_t *it;
  int          i;

  /* last created wins, matching the draw order in redraw_window */
  for (i = window->nicons - 1; i >= 0; i--)
  {
    it = window->icons[i];

    /* a bitmap icon is interactive only when it asks to be; RULE and the
     * static types are always inert; the other types always click */
    if (it->type == wuss_ICON_TYPE_BITMAP)
    {
      if (!(it->flags & wuss_ICON_FLAGS_INTERACTIVE))
        continue;
    }
    else if (it->type != wuss_ICON_TYPE_BUTTON &&
             it->type != wuss_ICON_TYPE_RADIO &&
             it->type != wuss_ICON_TYPE_OPTION &&
             it->type != wuss_ICON_TYPE_MENU_ENTRY)
    {
      continue;
    }

    if (it->flags & (wuss_ICON_FLAGS_HIDDEN | wuss_ICON_FLAGS_DISABLED))
      continue;

    if (box_contains_point(&it->bbox, doc_point.x, doc_point.y))
      return it;
  }

  return NULL;
}
