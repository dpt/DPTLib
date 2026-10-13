/* wuss/test/tasks/painting-doc.c -- paint canvas, drawing operations and undo */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/region.h"

#include "painting-doc.h"

/* the unused byte of a bgrx8888 pixel is not significant */
#define RGB(px) ((px) & 0x00FFFFFFu)

/* ----------------------------------------------------------------------- */

struct paintingdoc
{
  bitmap_t bm;
  screen_t scr;
  /* RLE-compressed copies of bm, oldest first */
  bitmap_t undo[PAINTINGDOC_UNDO_MAX];
  int      nundo;
  /* the selection, or NULL for none, and its boundary pixels for the ants.
   * pre is a raw copy of the canvas as of the last checkpoint, kept while
   * there is a selection so drawing outside it can be put back */
  region_t *sel;
  box_t    *outline;
  int       noutline;
  uint8_t  *pre;
};

/* ----------------------------------------------------------------------- */

/* a bgrx8888 bitmap of the given size and colour, with its own pixel store */
static result_t canvas_make(size2d_t size, colour_t colour, bitmap_t *bm)
{
  result_t rc;
  void    *pixels;

  pixels = malloc((size_t) size.w * (size_t) size.h * 4);
  if (pixels == NULL)
    return result_OOM;

  rc = bitmap_init(bm, size, pixelfmt_bgrx8888, size.w * 4, NULL, pixels);
  if (rc != result_OK)
  {
    free(pixels);
    return rc;
  }

  bitmap_clear(bm, colour);

  return result_OK;
}

result_t paintingdoc_create(size2d_t size, paintingdoc_t **doc)
{
  return paintingdoc_create_filled(size, colour_rgb(255, 255, 255), doc);
}

result_t paintingdoc_create_filled(size2d_t        size,
                                   colour_t        colour,
                                   paintingdoc_t **doc)
{
  result_t       rc;
  paintingdoc_t *d;

  if (size.w <= 0 || size.h <= 0)
    return result_BAD_ARG;

  d = calloc(1, sizeof(*d));
  if (d == NULL)
    return result_OOM;

  rc = canvas_make(size, colour, &d->bm);
  if (rc != result_OK)
  {
    free(d);
    return rc;
  }

  screen_for_bitmap(&d->scr, &d->bm);

  *doc = d;
  return result_OK;
}

/* drop every undo step */
static void undo_clear(paintingdoc_t *doc)
{
  int i;

  for (i = 0; i < doc->nundo; i++)
    free(doc->undo[i].base);

  doc->nundo = 0;
}

void paintingdoc_destroy(paintingdoc_t *doc)
{
  if (doc == NULL)
    return;

  undo_clear(doc);
  paintingdoc_select_none(doc);
  free(doc->bm.base);
  free(doc);
}

/* free a bitmap that bitmap_load_png or bitmap_convert made */
static void bitmap_discard(bitmap_t *bm, int heap_struct)
{
  free(bm->base);
  free(bm->palette);
  if (heap_struct)
    free(bm);
}

result_t paintingdoc_load_png(paintingdoc_t *doc, const char *path)
{
  result_t  rc;
  bitmap_t  src;
  bitmap_t *deep;
  bitmap_t  next;
  screen_t  scr;

  rc = bitmap_load_png(&src, path);
  if (rc != result_OK)
    return rc;

  /* a paletted image blits only once converted; bgrx8888 is the only deep
   * target bitmap_convert offers for it, so palette transparency is lost */
  deep = NULL;
  if (src.format >= pixelfmt_p1 && src.format <= pixelfmt_p8)
  {
    rc = bitmap_convert(&src, pixelfmt_bgrx8888, &deep);
    if (rc != result_OK)
    {
      bitmap_discard(&src, 0);
      return rc;
    }
  }

  rc = canvas_make(src.size, colour_rgb(255, 255, 255), &next);
  if (rc == result_OK)
  {
    screen_for_bitmap(&scr, &next);
    rc = screen_copy_bitmap(&scr, 0, 0, deep != NULL ? deep : &src);
    if (rc != result_OK)
      free(next.base);
  }

  bitmap_discard(&src, 0);
  if (deep != NULL)
    bitmap_discard(deep, 1);

  if (rc != result_OK)
    return rc;

  undo_clear(doc);
  paintingdoc_select_none(doc);
  free(doc->bm.base);
  doc->bm = next;
  screen_for_bitmap(&doc->scr, &doc->bm);

  return result_OK;
}

const bitmap_t *paintingdoc_bitmap(const paintingdoc_t *doc)
{
  return &doc->bm;
}

/* ----------------------------------------------------------------------- */

/* the canvas box */
static box_t canvas_box(const paintingdoc_t *doc)
{
  box_t box = BOX_POS_SIZE(0, 0, doc->bm.size.w, doc->bm.size.h);

  return box;
}

/* box covering a and b, grown by margin, clipped to the canvas */
static box_t clipped_box(const paintingdoc_t *doc,
                         point_t              a,
                         point_t              b,
                         int                  margin)
{
  box_t box;
  box_t canvas;
  box_t out;

  box.x0 = MIN(a.x, b.x) - margin;
  box.y0 = MIN(a.y, b.y) - margin;
  box.x1 = MAX(a.x, b.x) + margin + 1;
  box.y1 = MAX(a.y, b.y) + margin + 1;

  canvas = canvas_box(doc);
  if (box_intersection(&box, &canvas, &out))
    out = (box_t) BOX_INIT;

  return out;
}

/* ----------------------------------------------------------------------- */

void paintingdoc_select_none(paintingdoc_t *doc)
{
  region_destroy(doc->sel);
  free(doc->outline);
  free(doc->pre);
  doc->sel      = NULL;
  doc->outline  = NULL;
  doc->noutline = 0;
  doc->pre      = NULL;
}

/* take a freshly built region as the selection (NULL selects nothing) */
static result_t sel_adopt(paintingdoc_t *doc, region_t *sel)
{
  result_t rc;

  paintingdoc_select_none(doc);

  if (sel == NULL)
    return result_OK;

  rc = region_outline(sel, &doc->outline, &doc->noutline);
  if (rc != result_OK)
  {
    region_destroy(sel);
    return rc;
  }

  doc->sel = sel;
  return result_OK;
}

result_t paintingdoc_select_rect(paintingdoc_t *doc, box_t box)
{
  result_t  rc;
  box_t     canvas, r;
  region_t *sel;

  canvas = canvas_box(doc);
  if (box_intersection(&box, &canvas, &r))
    r = (box_t) BOX_INIT;

  rc = region_from_box(&r, &sel);
  if (rc != result_OK)
    return rc;

  return sel_adopt(doc, sel);
}

result_t paintingdoc_select_all(paintingdoc_t *doc)
{
  return paintingdoc_select_rect(doc, canvas_box(doc));
}

result_t paintingdoc_select_invert(paintingdoc_t *doc)
{
  result_t  rc;
  box_t     canvas;
  region_t *sel;

  canvas = canvas_box(doc);
  rc     = region_invert(doc->sel, &canvas, &sel);
  if (rc != result_OK)
    return rc;

  return sel_adopt(doc, sel);
}

result_t paintingdoc_select_poly(paintingdoc_t *doc,
                                 const point_t *pts,
                                 int            n)
{
  result_t  rc;
  box_t     canvas;
  region_t *sel;

  canvas = canvas_box(doc);
  rc     = region_from_polygon(pts, n, &canvas, &sel);
  if (rc != result_OK)
    return rc;

  return sel_adopt(doc, sel);
}

int paintingdoc_has_selection(const paintingdoc_t *doc)
{
  return doc->sel != NULL;
}

box_t paintingdoc_selection_bounds(const paintingdoc_t *doc)
{
  return region_bounds(doc->sel);
}

const box_t *paintingdoc_selection_outline(const paintingdoc_t *doc, int *n)
{
  *n = doc->noutline;
  return doc->outline;
}

/* put back, from the last checkpoint, the pixels of dirty that lie outside the
 * selection */
static void sel_clip(paintingdoc_t *doc, box_t dirty)
{
  box_t      canvas, r;
  uint32_t  *dst;
  uint32_t  *src;
  const int *runs;
  int        nruns, x, y, i;

  if (doc->sel == NULL || doc->pre == NULL)
    return;

  canvas = canvas_box(doc);
  if (box_intersection(&dirty, &canvas, &r))
    return;

  for (y = r.y0; y < r.y1; y++)
  {
    dst   = (uint32_t *) ((uint8_t *) doc->bm.base + (size_t) y * doc->bm.rowbytes);
    src   = (uint32_t *) (doc->pre + (size_t) y * doc->bm.rowbytes);
    nruns = region_row(doc->sel, y, &runs);

    /* restore the gaps between the selected runs */
    x = r.x0;
    for (i = 0; i < nruns && x < r.x1; i++)
    {
      if (runs[i * 2] > x)
        memcpy(dst + x, src + x,
               (size_t) (MIN(runs[i * 2], r.x1) - x) * sizeof(*dst));

      x = MAX(x, runs[i * 2 + 1]);
    }

    if (x < r.x1)
      memcpy(dst + x, src + x, (size_t) (r.x1 - x) * sizeof(*dst));
  }
}

/* ----------------------------------------------------------------------- */

result_t paintingdoc_checkpoint(paintingdoc_t *doc)
{
  result_t rc;
  size_t   n;
  void    *pixels;
  bitmap_t snap;

  n = (size_t) doc->bm.rowbytes * (size_t) doc->bm.size.h;

  if (doc->sel != NULL)
  {
    if (doc->pre == NULL)
      doc->pre = malloc(n);
    if (doc->pre != NULL)
      memcpy(doc->pre, doc->bm.base, n);
    else
      logf_warning("painting: no memory to clip drawing to the selection");
  }

  pixels = malloc(n);
  if (pixels == NULL)
    return result_OOM;

  memcpy(pixels, doc->bm.base, n);

  rc = bitmap_init(&snap, doc->bm.size, pixelfmt_bgrx8888, doc->bm.rowbytes,
                   NULL, pixels);
  if (rc == result_OK)
    rc = bitmap_compress(&snap);
  if (rc != result_OK)
  {
    free(pixels);
    return rc;
  }

  if (doc->nundo == PAINTINGDOC_UNDO_MAX)
  {
    free(doc->undo[0].base);
    memmove(&doc->undo[0], &doc->undo[1],
            (PAINTINGDOC_UNDO_MAX - 1) * sizeof(doc->undo[0]));
    doc->nundo--;
  }

  doc->undo[doc->nundo++] = snap;
  return result_OK;
}

/* a drawing operation that takes its own checkpoint, but still draws if it
 * cannot: no undo for that step beats losing the user's edit */
static void checkpoint_or_warn(paintingdoc_t *doc)
{
  if (paintingdoc_checkpoint(doc) != result_OK)
    logf_warning("painting: no memory for an undo step");
}

int paintingdoc_can_undo(const paintingdoc_t *doc)
{
  return doc->nundo > 0;
}

box_t paintingdoc_undo(paintingdoc_t *doc)
{
  bitmap_t *snap;
  size_t    n;

  if (doc->nundo == 0)
    return (box_t) BOX_INIT;

  snap = &doc->undo[doc->nundo - 1];
  if (bitmap_decompress(snap) != result_OK)
    return (box_t) BOX_INIT;

  n = (size_t) doc->bm.rowbytes * (size_t) doc->bm.size.h;
  memcpy(doc->bm.base, snap->base, n);

  free(snap->base);
  doc->nundo--;

  return canvas_box(doc);
}

/* ----------------------------------------------------------------------- */

void paintingdoc_draw_stroke(screen_t *scr,
                             point_t   a,
                             point_t   b,
                             int       width,
                             colour_t  colour)
{
  width = MAX(width, 1);

  if (width == 1)
  {
    screen_draw_line(scr, a.x, a.y, b.x, b.y, colour);
    return;
  }

  /* a zero-length thick line is a square, not a dab */
  if (a.x != b.x || a.y != b.y)
    screen_draw_thick_line(scr, a.x, a.y, b.x, b.y, width, colour);
  screen_fill_circle(scr, a.x, a.y, width / 2, colour);
  screen_fill_circle(scr, b.x, b.y, width / 2, colour);
}

box_t paintingdoc_stroke(paintingdoc_t *doc,
                         point_t        a,
                         point_t        b,
                         int            width,
                         colour_t       colour)
{
  box_t dirty;

  width = MAX(width, 1);

  paintingdoc_draw_stroke(&doc->scr, a, b, width, colour);

  dirty = clipped_box(doc, a, b, width / 2);
  sel_clip(doc, dirty);

  return dirty;
}

/* ----------------------------------------------------------------------- */

box_t paintingdoc_spray(paintingdoc_t *doc,
                        point_t        p,
                        int            radius,
                        colour_t       colour,
                        rng_t         *rng)
{
  box_t dirty;
  int   n, x, y, w;

  radius = MAX(radius, 1);
  w      = 2 * radius + 1;

  /* about one dot per four pixels of the disc's area, at least a few */
  n = MAX(radius * radius * 3 / 4, 3);

  while (n-- > 0)
  {
    x = (int) (rng_xorshift32(rng) % (uint32_t) w) - radius;
    y = (int) (rng_xorshift32(rng) % (uint32_t) w) - radius;
    if (x * x + y * y > radius * radius)
      continue;

    x += p.x;
    y += p.y;
    if (x < 0 || y < 0 || x >= doc->bm.size.w || y >= doc->bm.size.h)
      continue;

    screen_set_pixel(&doc->scr, x, y, colour);
  }

  dirty = clipped_box(doc, p, p, radius);
  sel_clip(doc, dirty);

  return dirty;
}

/* ----------------------------------------------------------------------- */

void paintingdoc_draw_rect(screen_t *scr,
                           point_t   a,
                           point_t   b,
                           int       width,
                           int       fill,
                           colour_t  colour)
{
  int x0, y0, w, h, t;

  x0 = MIN(a.x, b.x);
  y0 = MIN(a.y, b.y);
  w  = MAX(a.x, b.x) - x0 + 1;
  h  = MAX(a.y, b.y) - y0 + 1;
  t  = MAX(width, 1);

  if (fill || 2 * t >= w || 2 * t >= h)
  {
    screen_fill_rect(scr, x0, y0, SIZE2D(w, h), colour);
  }
  else
  {
    screen_fill_rect(scr, x0, y0, SIZE2D(w, t), colour);
    screen_fill_rect(scr, x0, y0 + h - t, SIZE2D(w, t), colour);
    screen_fill_rect(scr, x0, y0 + t, SIZE2D(t, h - 2 * t), colour);
    screen_fill_rect(scr, x0 + w - t, y0 + t, SIZE2D(t, h - 2 * t), colour);
  }
}

box_t paintingdoc_rect(paintingdoc_t *doc,
                       point_t        a,
                       point_t        b,
                       int            width,
                       int            fill,
                       colour_t       colour)
{
  box_t dirty;

  checkpoint_or_warn(doc);

  paintingdoc_draw_rect(&doc->scr, a, b, width, fill, colour);

  dirty = clipped_box(doc, a, b, 0);
  sel_clip(doc, dirty);

  return dirty;
}

/* ----------------------------------------------------------------------- */

static uint32_t *pixel_row(const paintingdoc_t *doc, int y)
{
  return (uint32_t *) ((uint8_t *) doc->bm.base +
                       (size_t) y * doc->bm.rowbytes);
}

box_t paintingdoc_fill(paintingdoc_t *doc, point_t p, colour_t colour)
{
  box_t     dirty = BOX_INIT;
  uint32_t  target, fillpx;
  uint32_t *row;
  point_t  *stack;
  int       n, cap;
  int       w, h;
  int       x, x0, x1, y, dy, in_span;
  point_t  *grown;

  w = doc->bm.size.w;
  h = doc->bm.size.h;

  if (p.x < 0 || p.x >= w || p.y < 0 || p.y >= h)
    return dirty;

  target = pixel_row(doc, p.y)[p.x];
  fillpx = screen_colour_to_pixel(&doc->scr, colour);
  if (RGB(target) == RGB(fillpx))
    return dirty;

  checkpoint_or_warn(doc);

  cap   = 256;
  n     = 0;
  stack = malloc((size_t) cap * sizeof(*stack));
  if (stack == NULL)
    return dirty;

  stack[n++] = p;

  while (n > 0)
  {
    n--;
    x = stack[n].x;
    y = stack[n].y;
    row = pixel_row(doc, y);

    /* a seed pushed earlier may since have been filled over by another span */
    if (RGB(row[x]) != RGB(target))
      continue;

    x0 = x;
    while (x0 > 0 && RGB(row[x0 - 1]) == RGB(target))
      x0--;
    x1 = x;
    while (x1 < w - 1 && RGB(row[x1 + 1]) == RGB(target))
      x1++;

    for (x = x0; x <= x1; x++)
      row[x] = fillpx;

    dirty.x0 = MIN(dirty.x0, x0);
    dirty.x1 = MAX(dirty.x1, x1 + 1);
    dirty.y0 = MIN(dirty.y0, y);
    dirty.y1 = MAX(dirty.y1, y + 1);

    /* one seed per run of target pixels in each adjacent row */
    for (dy = -1; dy <= 1; dy += 2)
    {
      if (y + dy < 0 || y + dy >= h)
        continue;

      row     = pixel_row(doc, y + dy);
      in_span = 0;

      for (x = x0; x <= x1; x++)
      {
        if (RGB(row[x]) != RGB(target))
        {
          in_span = 0;
          continue;
        }
        if (in_span)
          continue;

        in_span = 1;

        if (n == cap)
        {
          grown = realloc(stack, (size_t) cap * 2 * sizeof(*stack));
          if (grown == NULL)
          {
            logf_warning("painting: no memory to finish fill");
            free(stack);
            sel_clip(doc, dirty);
            return dirty;
          }
          stack = grown;
          cap  *= 2;
        }

        stack[n++] = POINT(x, y + dy);
      }
    }
  }

  free(stack);
  sel_clip(doc, dirty);
  return dirty;
}
