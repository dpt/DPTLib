/* line.c -- Cohen-Sutherland line clipping algorithm */

#include "base/utils.h"
#include "geom/box.h"

#include "geom/line.h"

typedef unsigned int outcode_t;

#define outcode_INSIDE (0)
#define outcode_LEFT   (1u << 0)
#define outcode_RIGHT  (1u << 1)
#define outcode_BOTTOM (1u << 2)
#define outcode_TOP    (1u << 3)

static INLINE outcode_t compute_outcode(const box_t *clip, int x, int y)
{
  outcode_t code;

  code = outcode_INSIDE;

  if (x < clip->x0)
    code |= outcode_LEFT;
  else if (x >= clip->x1)
    code |= outcode_RIGHT;

  if (y < clip->y0)
    code |= outcode_BOTTOM;
  else if (y >= clip->y1)
    code |= outcode_TOP;

  return code;
}

int line_clip(const box_t *clip,
              int         *px0,
              int         *py0,
              int         *px1,
              int         *py1)
{
  int       x0, y0, x1, y1;
  outcode_t oc0, oc1;
  outcode_t oc;
  int       w, h;
  int       x, y;

  x0 = *px0;
  y0 = *py0;
  x1 = *px1;
  y1 = *py1;

  oc0 = compute_outcode(clip, x0, y0);
  oc1 = compute_outcode(clip, x1, y1);

  /* loop until either both points are inside the clip region (in which case
   * draw) or both points are outside the clip (in which case don't). */
  for (;;)
  {
    if ((oc0 | oc1) == outcode_INSIDE)
    {
      /* both points lie inside clip - draw */

      *px0 = x0;
      *py0 = y0;
      *px1 = x1;
      *py1 = y1;

      return 1;
    }
    else if ((oc0 & oc1) != 0)
    {
      /* both points lie outside clip - don't draw */
      return 0;
    }
    else
    {
      oc = oc1 > oc0 ? oc1 : oc0;
      w  = x1 - x0;
      h  = y1 - y0;

      if (oc & outcode_TOP)
      {
        x = x0 + w * (clip->y1 - 1 - y0) / h;
        y = clip->y1 - 1;
      }
      else if (oc & outcode_BOTTOM)
      {
        x = x0 + w * (clip->y0 - y0) / h;
        y = clip->y0;
      }
      else if (oc & outcode_RIGHT)
      {
        x = clip->x1 - 1;
        y = y0 + h * (clip->x1 - 1 - x0) / w;
      }
      else if (oc & outcode_LEFT)
      {
        x = clip->x0;
        y = y0 + h * (clip->x0 - x0) / w;
      }

      if (oc == oc0)
      {
        x0 = x;
        y0 = y;
        oc0 = compute_outcode(clip, x0, y0);
      }
      else
      {
        x1 = x;
        y1 = y;
        oc1 = compute_outcode(clip, x1, y1);
      }
    }
  }
}
