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

/* Compute (a * b) / c, truncating toward zero, without overflow and
 * without using a 64-bit or floating-point intermediate. Assumes the
 * true result fits in an int (guaranteed here as it's a coordinate). */
static int muldiv(int a, int b, int c)
{
  unsigned int a_lo, a_hi, b_lo, b_hi;
  unsigned int lo_lo, hi_lo, lo_hi, hi_hi;
  unsigned int cross, cross_carry, lo_carry;
  unsigned int hi, lo;
  unsigned int ua, ub, uc;
  unsigned int rem, quot, bit;
  int          neg;
  int          i;

  neg = 0;
  if (a < 0) { neg = !neg; ua = 0u - (unsigned int) a; } else ua = (unsigned int) a;
  if (b < 0) { neg = !neg; ub = 0u - (unsigned int) b; } else ub = (unsigned int) b;
  if (c < 0) { neg = !neg; uc = 0u - (unsigned int) c; } else uc = (unsigned int) c;

  /* widen ua * ub into a 64-bit result held as two 32-bit halves */
  a_lo = ua & 0xFFFFu; a_hi = ua >> 16;
  b_lo = ub & 0xFFFFu; b_hi = ub >> 16;

  lo_lo = a_lo * b_lo;
  hi_lo = a_hi * b_lo;
  lo_hi = a_lo * b_hi;
  hi_hi = a_hi * b_hi;

  cross       = hi_lo + lo_hi;
  cross_carry = (cross < hi_lo) ? (1u << 16) : 0u;
  lo          = lo_lo + (cross << 16);
  lo_carry    = (lo < lo_lo) ? 1u : 0u;
  hi          = hi_hi + (cross >> 16) + cross_carry + lo_carry;

  /* long-divide the 64-bit (hi:lo) dividend by uc, one bit at a time */
  rem  = 0;
  quot = 0;
  for (i = 63; i >= 0; i--)
  {
    bit = (i >= 32) ? ((hi >> (i - 32)) & 1u) : ((lo >> i) & 1u);
    rem = (rem << 1) | bit;
    if (rem >= uc)
    {
      rem -= uc;
      if (i < 32)
        quot |= (1u << i);
    }
  }

  return neg ? -(int) quot : (int) quot;
}

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
  int       x = 0, y = 0;

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
        x = x0 + muldiv(w, clip->y1 - 1 - y0, h);
        y = clip->y1 - 1;
      }
      else if (oc & outcode_BOTTOM)
      {
        x = x0 + muldiv(w, clip->y0 - y0, h);
        y = clip->y0;
      }
      else if (oc & outcode_RIGHT)
      {
        x = clip->x1 - 1;
        y = y0 + muldiv(h, clip->x1 - 1 - x0, w);
      }
      else if (oc & outcode_LEFT)
      {
        x = clip->x0;
        y = y0 + muldiv(h, clip->x0 - x0, w);
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
