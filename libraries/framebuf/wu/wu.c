/* framebuf/wu/wu.c -- Wu's colour quantiser */

#include <stdint.h>
#include <stdlib.h>

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/wu.h"

/* ----------------------------------------------------------------------- */

#define WU_BINS  (32) /* per channel */
#define WU_SIDE  (WU_BINS + 1) /* plane 0 of each axis stays zero */
#define WU_CELLS (WU_SIDE * WU_SIDE * WU_SIDE)

/* a pixel count and its channel sums. Cumulative cells may wrap while being
 * added and subtracted: a box's total always fits, so comes out right. */
typedef struct wu_cell
{
  uint32_t w, r, g, b;
}
wu_cell_t;

/* (lo, hi] bins of red, green and blue, and the box's best cut */
typedef struct wu_box
{
  int    lo[3], hi[3];
  int    axis, cut;
  double gain; /* squared error the cut removes; 0 if it can't be cut */
}
wu_box_t;

static const int wu_stride[3] = { WU_SIDE * WU_SIDE, WU_SIDE, 1 };

/* ----------------------------------------------------------------------- */

static void wu_histogram(const bitmap_t *src, wu_cell_t *cells)
{
  const unsigned char       *row;
  const pixelfmt_rgbx8888_t *px;
  int                        x, y;
  unsigned int               r, g, b;
  wu_cell_t                 *cell;

  row = src->base;
  for (y = 0; y < src->size.h; y++)
  {
    px = (const pixelfmt_rgbx8888_t *) row;
    for (x = 0; x < src->size.w; x++)
    {
      r = PIXELFMT_Rxxx8888(px[x]);
      g = PIXELFMT_xGxx8888(px[x]);
      b = PIXELFMT_xxBx8888(px[x]);

      cell = &cells[((r >> 3) + 1) * wu_stride[0] +
                    ((g >> 3) + 1) * wu_stride[1] +
                    ((b >> 3) + 1)];
      cell->w++;
      cell->r += r;
      cell->g += g;
      cell->b += b;
    }
    row += src->rowbytes;
  }
}

/* Turn the histogram into running totals along each axis in turn, after which
 * a cell holds the total of every bin at or below it on all three axes. */
static void wu_accumulate(wu_cell_t *cells)
{
  int              axis;
  int              c[3];
  wu_cell_t       *cell;
  const wu_cell_t *prev;

  for (axis = 0; axis < 3; axis++)
    for (c[0] = (axis == 0); c[0] < WU_SIDE; c[0]++)
      for (c[1] = (axis == 1); c[1] < WU_SIDE; c[1]++)
        for (c[2] = (axis == 2); c[2] < WU_SIDE; c[2]++)
        {
          cell = &cells[c[0] * wu_stride[0] +
                        c[1] * wu_stride[1] +
                        c[2]];
          prev = cell - wu_stride[axis];

          cell->w += prev->w;
          cell->r += prev->r;
          cell->g += prev->g;
          cell->b += prev->b;
        }
}

static wu_cell_t wu_sub(wu_cell_t a, wu_cell_t b)
{
  a.w -= b.w;
  a.r -= b.r;
  a.g -= b.g;
  a.b -= b.b;

  return a;
}

/* The total of the box's bins at or below plane `pos` of `axis`. */
static wu_cell_t wu_plane(const wu_cell_t *cells,
                          const wu_box_t  *box,
                          int              axis,
                          int              pos)
{
  static const int others[3][2] = { { 1, 2 }, { 0, 2 }, { 0, 1 } };

  int              a, b;
  const wu_cell_t *base;
  const wu_cell_t *hh, *hl, *lh, *ll;
  wu_cell_t        sum;

  a    = others[axis][0];
  b    = others[axis][1];
  base = cells + pos * wu_stride[axis];

  hh = base + box->hi[a] * wu_stride[a] + box->hi[b] * wu_stride[b];
  hl = base + box->hi[a] * wu_stride[a] + box->lo[b] * wu_stride[b];
  lh = base + box->lo[a] * wu_stride[a] + box->hi[b] * wu_stride[b];
  ll = base + box->lo[a] * wu_stride[a] + box->lo[b] * wu_stride[b];

  sum.w = hh->w - hl->w - lh->w + ll->w;
  sum.r = hh->r - hl->r - lh->r + ll->r;
  sum.g = hh->g - hl->g - lh->g + ll->g;
  sum.b = hh->b - hl->b - lh->b + ll->b;

  return sum;
}

static wu_cell_t wu_box_total(const wu_cell_t *cells, const wu_box_t *box)
{
  return wu_sub(wu_plane(cells, box, 0, box->hi[0]),
                wu_plane(cells, box, 0, box->lo[0]));
}

/* A group's squared error is the sum of its pixels' squares less this term,
 * so comparing this term needs no pixel squares: the histogram stays
 * 32-bit. c must hold at least one pixel. */
static double wu_energy(const wu_cell_t *c)
{
  return ((double) c->r * c->r +
          (double) c->g * c->g +
          (double) c->b * c->b) / c->w;
}

/* Find the cut which most reduces the box's squared error. A cut with pixels
 * on both sides always reduces it, so gain is 0 only for a box with one
 * occupied bin. */
static void wu_find_cut(const wu_cell_t *cells, wu_box_t *box)
{
  wu_cell_t whole;
  double    best;
  int       axis, pos;
  wu_cell_t base;
  wu_cell_t lower, upper;
  double    energy;

  whole = wu_box_total(cells, box);
  best  = 0.0;

  for (axis = 0; axis < 3; axis++)
  {
    base = wu_plane(cells, box, axis, box->lo[axis]);
    for (pos = box->lo[axis] + 1; pos < box->hi[axis]; pos++)
    {
      lower = wu_sub(wu_plane(cells, box, axis, pos), base);
      if (lower.w == 0)
        continue;
      if (lower.w == whole.w)
        break; /* so too for the remaining planes */

      upper  = wu_sub(whole, lower);
      energy = wu_energy(&lower) + wu_energy(&upper);
      if (energy > best)
      {
        best      = energy;
        box->axis = axis;
        box->cut  = pos;
      }
    }
  }

  box->gain = best > 0.0 ? best - wu_energy(&whole) : 0.0;
}

/* ----------------------------------------------------------------------- */

result_t wu_make_palette(const bitmap_t *src,
                         int             ncolours,
                         colour_t       *palette,
                         int            *nfound)
{
  wu_cell_t *cells;
  wu_box_t   boxes[wu_MAX_COLOURS];
  int        nboxes;
  int        i;
  int        best;
  wu_cell_t  total;

  *nfound = 0;

  if (src->format != pixelfmt_rgbx8888 && src->format != pixelfmt_rgba8888)
    return result_BAD_ARG;
  if (ncolours < wu_MIN_COLOURS || ncolours > wu_MAX_COLOURS)
    return result_BAD_ARG;

  /* a cell sums every pixel's channels and rounding below adds half a pixel
   * count more */
  if ((uint64_t) src->size.w * src->size.h > UINT32_MAX / 256)
    return result_TOO_BIG;

  if (src->size.w == 0 || src->size.h == 0)
    return result_OK;

  cells = calloc(WU_CELLS, sizeof(*cells));
  if (cells == NULL)
    return result_OOM;

  wu_histogram(src, cells);
  wu_accumulate(cells);

  for (i = 0; i < 3; i++)
  {
    boxes[0].lo[i] = 0;
    boxes[0].hi[i] = WU_BINS;
  }
  wu_find_cut(cells, &boxes[0]);
  nboxes = 1;

  while (nboxes < ncolours)
  {
    best = 0;
    for (i = 1; i < nboxes; i++)
      if (boxes[i].gain > boxes[best].gain)
        best = i;

    if (boxes[best].gain <= 0.0)
      break; /* every box is one occupied bin */

    boxes[nboxes] = boxes[best];
    boxes[best].hi[boxes[best].axis]     = boxes[best].cut;
    boxes[nboxes].lo[boxes[nboxes].axis] = boxes[nboxes].cut;
    wu_find_cut(cells, &boxes[best]);
    wu_find_cut(cells, &boxes[nboxes]);
    nboxes++;
  }

  for (i = 0; i < nboxes; i++)
  {
    total = wu_box_total(cells, &boxes[i]);
    palette[i] = colour_rgb((total.r + total.w / 2) / total.w,
                            (total.g + total.w / 2) / total.w,
                            (total.b + total.w / 2) / total.w);
  }
  *nfound = nboxes;

  free(cells);

  return result_OK;
}
