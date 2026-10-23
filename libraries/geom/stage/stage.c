/* geom/stage/stage.c -- page background layout: pasteboard, border, margin,
 * shadow */

#include <assert.h>
#include <stdlib.h>

#include "base/utils.h"
#include "geom/stage.h"

/* Grid lines, 10 apart so a spec reads as a position: C30 is the fourth
 * column line. There are 9 columns/rows, hence 10 lines. */
typedef enum
{
  C00, C10, C20, C30, C40, C50, C60, C70, C80, C90
}
column_t;

typedef enum
{
  R00, R10, R20, R30, R40, R50, R60, R70, R80, R90
}
row_t;

typedef struct stageboxspec
{
  stageboxkind_t kind;
  column_t       x0;
  row_t          y0;
  column_t       x1;
  row_t          y1;
}
stageboxspec_t;

/* The dimensions table, indexed by dims_t. Even entries are horizontal. */
typedef enum dims
{
  ContentW,
  ContentH,
  StrokeW,
  StrokeH,
  MarginW,
  MarginH,
  ShadowW,
  ShadowH,
  dims__LIMIT
}
dims_t;

#define GRID_CELLS 9 /* columns and rows */

static int stagebox_compare(const void *va, const void *vb)
{
  const stagebox_t *a = va;
  const stagebox_t *b = vb;
  int               delta;

  /* Sort the content box to be first in the list */
  if (a->kind == stageboxkind_CONTENT || b->kind == stageboxkind_CONTENT)
    return (a->kind == stageboxkind_CONTENT) ? -1 : 1;

  /* Sort the highest, leftmost boxes to earlier positions in the array */
  delta = a->box.y1 - b->box.y1;
  if (delta == 0)
    delta = a->box.x0 - b->box.x0;

  return (delta < 0) ? 1 : (delta > 0) ? -1 : 0;
}

/* Sum the input sizes (widths/heights) and output an array of positions.
 * Note that the output is zero prefixed, so is one longer than the input,
 * e.g. 10,20,10 becomes 0,10,30,40. */
static void makepos(const int *sizes, size_t nsizes, int *pos)
{
  int t;

  t = 0;
  while (nsizes--)
  {
    *pos++ = t;
    t += *sizes++;
  }
  *pos++ = t;
}

/* Resolve arrays of stageboxspecs into a list of stageboxes.
 * Input specs are in any order. Output boxes are sorted by their top left
 * position. */
static void resolve_specs(const stageboxspec_t *speclists[],
                          const size_t          lenlist[],
                          size_t                nspecs,
                          const int            *xsizes,
                          const int            *ysizes,
                          stagebox_t           *boxes,
                          size_t               *nboxes)
{
  int         xpos[GRID_CELLS + 1];
  int         ypos[GRID_CELLS + 1];
  size_t      i;
  size_t      j;
  stagebox_t *boxp;

  makepos(xsizes, GRID_CELLS, xpos);
  makepos(ysizes, GRID_CELLS, ypos);

  /* Resolve the lists of box specs to a single list of boxes */
  boxp = boxes;
  for (j = 0; j < nspecs; j++)
  {
    const stageboxspec_t *spec = speclists[j];

    for (i = 0; i < lenlist[j]; i++)
    {
      boxp->kind   = spec->kind;
      boxp->box.x0 = xpos[spec->x0];
      boxp->box.y0 = ypos[spec->y0];
      boxp->box.x1 = xpos[spec->x1];
      boxp->box.y1 = ypos[spec->y1];
      spec++;
      boxp++;
    }
  }

  *nboxes = boxp - boxes;

  /* Sort laid out boxes by their top left coordinate */
  qsort(boxes, *nboxes, sizeof(*boxes), stagebox_compare);
}

void stageconfig_init(stageconfig_t *config)
{
  assert(config);

  config->flags          = stage_FLAG_SHADOW;
  config->pasteboard_min = 16;
  config->stroke         = 1;
  config->margin         = 8;
  config->shadow         = 4;
  config->log2_pixel_x   = 0;
  config->log2_pixel_y   = 0;
}

void stage_get_fixed(const stageconfig_t *config,
                     int                 *width,
                     int                 *height)
{
  assert(config);
  assert(width);
  assert(height);

  /* TODO: Rounding */

  *width  = config->margin * 2 + config->pasteboard_min;
  *height = config->margin * 2 + config->pasteboard_min;
}

void stage_generate(const stageconfig_t *config,
                    int                  workarea_width,
                    int                  workarea_height,
                    int                  content_width,
                    int                  content_height,
                    int                 *min_workarea_width,
                    int                 *min_workarea_height,
                    stagebox_t          *boxes,
                    size_t              *nboxes)
{
  static const stageboxspec_t plain_pasteboard[] =
  {
    { stageboxkind_PASTEBOARD, C00, R80, C90, R90 }, /* top */
    { stageboxkind_PASTEBOARD, C00, R10, C10, R80 }, /* left */
    { stageboxkind_PASTEBOARD, C70, R10, C90, R80 }, /* right */
    { stageboxkind_PASTEBOARD, C00, R00, C90, R20 }, /* bottom */
  };

  static const stageboxspec_t pasteboard_with_shadow[] =
  {
    { stageboxkind_PASTEBOARD, C00, R80, C90, R90 }, /* top */
    { stageboxkind_PASTEBOARD, C00, R10, C10, R80 }, /* left */
    { stageboxkind_PASTEBOARD, C80, R10, C90, R80 }, /* right */
    { stageboxkind_PASTEBOARD, C00, R00, C90, R10 }, /* bottom */
    { stageboxkind_PASTEBOARD, C10, R10, C30, R20 }, /* left bottom shim */
    { stageboxkind_PASTEBOARD, C70, R60, C80, R80 }, /* right top shim */

    { stageboxkind_SHADOW,     C70, R20, C80, R60 }, /* right */
    { stageboxkind_SHADOW,     C30, R10, C80, R20 }, /* bottom */
  };

  static const stageboxspec_t common[] =
  {
    { stageboxkind_STROKE,     C10, R70, C70, R80 }, /* top */
    { stageboxkind_STROKE,     C10, R20, C20, R70 }, /* left */
    { stageboxkind_STROKE,     C60, R20, C70, R70 }, /* right */
    { stageboxkind_STROKE,     C10, R20, C70, R30 }, /* bottom */

    { stageboxkind_MARGIN,     C20, R50, C60, R70 }, /* top */
    { stageboxkind_MARGIN,     C20, R40, C40, R50 }, /* left */
    { stageboxkind_MARGIN,     C50, R40, C60, R50 }, /* right */
    { stageboxkind_MARGIN,     C20, R30, C60, R40 }, /* bottom */

    { stageboxkind_CONTENT,    C40, R40, C50, R50 }
  };

  const stageboxspec_t *specs[2];
  size_t                nspecs[2];
  int                   d[dims__LIMIT]; /* [d]imensions */
  int                   xpix, ypix;
  int                   i;
  int                   x_fixed, y_fixed;
  int                   x_variable, y_variable;
  int                   x_sizes[GRID_CELLS], y_sizes[GRID_CELLS];
  int                  *p;

  assert(config);
  assert(boxes);
  assert(nboxes);

  /* Build a list of box specs */
  if (config->flags & stage_FLAG_SHADOW)
  {
    specs[0]  = pasteboard_with_shadow;
    nspecs[0] = NELEMS(pasteboard_with_shadow);
  }
  else
  {
    specs[0]  = plain_pasteboard;
    nspecs[0] = NELEMS(plain_pasteboard);
  }
  specs[1]  = common;
  nspecs[1] = NELEMS(common);

  d[ContentW] = content_width;
  d[ContentH] = content_height;
  d[StrokeW]  = config->stroke;
  d[StrokeH]  = config->stroke;
  d[MarginW]  = config->margin;
  d[MarginH]  = config->margin;
  d[ShadowW]  = config->shadow;
  d[ShadowH]  = config->shadow;

  /* The size of a device pixel */
  xpix = 1 << config->log2_pixel_x;
  ypix = 1 << config->log2_pixel_y;
#define ROUND_X_DOWN(x) ((x) & ~(xpix - 1))
#define ROUND_Y_DOWN(y) ((y) & ~(ypix - 1))
#define ROUND_X_UP(x)   (((x) + (xpix - 1)) & ~(xpix - 1))
#define ROUND_Y_UP(y)   (((y) + (ypix - 1)) & ~(ypix - 1))

  /* Round the dimensions to be whole pixels.
   * Content dimensions are rounded down to match the OS primitives (e.g.
   * sprite plotting). Others are rounded up to whole pixels with the
   * assumption that the pasteboard will absorb any difference. */
  for (i = 0; i < dims__LIMIT; i++)
    if (i >= ContentW && i <= ContentH)
      d[i] = ((i & 1) == 0) ? ROUND_X_DOWN(d[i]) : ROUND_Y_DOWN(d[i]);
    else
      d[i] = ((i & 1) == 0) ? ROUND_X_UP(d[i]) : ROUND_Y_UP(d[i]);

  /* Calculate the fixed and variable portions of the stage.
   * The stroked border outline is considered to be atop the margin so we can
   * ignore it here. The shadow is considered be part of the variable
   * pasteboard area so we can ignore that too. */
  x_fixed    = d[MarginW] + d[ContentW] + d[MarginW];
  y_fixed    = d[MarginH] + d[ContentH] + d[MarginH];
  x_variable = MAX(workarea_width  - x_fixed, config->pasteboard_min);
  y_variable = MAX(workarea_height - y_fixed, config->pasteboard_min);
  if (min_workarea_width)
    *min_workarea_width  = x_variable + x_fixed;
  if (min_workarea_height)
    *min_workarea_height = y_variable + y_fixed;

  /* Build the grid */

  /* Columns */
  p = x_sizes;
  *p++ = ROUND_X_UP(x_variable / 2);              /* left pasteboard */
  *p++ = d[StrokeW];                              /* left border stroke */
  *p++ = d[ShadowW] - d[StrokeW];                 /* left margin - shadow start point */
  *p++ = d[MarginW] - d[ShadowW];                 /* left margin (stroke widths cancel out) */
  *p++ = d[ContentW];                             /* content */
  *p++ = d[MarginW] - d[StrokeW];                 /* right margin */
  *p++ = d[StrokeW];                              /* right border stroke */
  *p++ = d[ShadowW];                              /* right shadow */
  *p++ = x_variable - ROUND_X_UP(x_variable / 2) - d[ShadowW]; /* right pasteboard - takes up any slack */

  /* Rows */
  p = y_sizes;
  *p++ = ROUND_Y_UP(y_variable / 2) - d[ShadowH]; /* bottom pasteboard */
  *p++ = d[ShadowH];                              /* bottom shadow */
  *p++ = d[StrokeH];                              /* bottom border stroke */
  *p++ = d[MarginH] - d[StrokeH];                 /* bottom margin */
  *p++ = d[ContentH];                             /* content */
  *p++ = d[MarginH] - d[ShadowH] - d[StrokeH];    /* top margin - shadow start point */
  *p++ = d[ShadowH];                              /* top margin */
  *p++ = d[StrokeH];                              /* top border stroke */
  *p++ = y_variable - ROUND_Y_UP(y_variable / 2); /* top pasteboard - takes up any slack */

  resolve_specs(specs, nspecs, NELEMS(specs), x_sizes, y_sizes, boxes, nboxes);
}
