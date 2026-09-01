/* wuss/icon/screen-box.c -- work-area icon bbox to screen space */

#include "geom/box.h"
#include "geom/point.h"

#include "../impl.h"

/* Map an icon bbox (virtual document space) into screen space, given the
 * owning window's content box and scroll offset. wuss__icon_draw uses this
 * for the box it paints into; keeping the transform here means hit-testing
 * and invalidation cannot drift from what is drawn. */
void wuss__icon_box_to_screen(const box_t *content,
                              point_t      scroll,
                              const box_t *bbox,
                              box_t       *out)
{
  box_translated(bbox, content->x0 - scroll.x, content->y0 - scroll.y, out);
}
