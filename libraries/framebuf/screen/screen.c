/* screen.c */

#include <assert.h>
#include <string.h>
#include <stddef.h>

#include "framebuf/screen.h"

void screen_init(screen_t  *scr,
                 int        width,
                 int        height,
                 pixelfmt_t fmt,
                 int        rowbytes,
                 colour_t  *palette,
                 box_t      clip,
                 void      *base)
{
  assert(scr);

  scr->width    = width;
  scr->height   = height;
  scr->format   = fmt;
  scr->rowbytes = rowbytes;
  scr->palette  = palette;
  scr->clip     = clip;
  scr->base     = base;
}

void screen_for_bitmap(screen_t *scr, const bitmap_t *bm)
{
  assert(scr);
  assert(bm);

  memcpy(scr, bm, offsetof(screen_t, clip)); /* copy common members */
  box_reset(&scr->clip);
  scr->base = bm->base;
}

int screen_get_clip(const screen_t *scr, box_t *clip)
{
  clip->x0 = 0;
  clip->y0 = 0;
  clip->x1 = scr->width;
  clip->y1 = scr->height;

  if (box_is_empty(&scr->clip))
    return 0; /* not empty */

  return box_intersection(clip, &scr->clip, clip);
}

/* ----------------------------------------------------------------------- */

void screen_draw_pixel(screen_t *scr, int x, int y, colour_t colour)
{
  pixelfmt_any_t fmt;

  fmt = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 5:
    {
      pixelfmt_any32_t *base;

      base = scr->base;
      base += y * scr->rowbytes / sizeof(*base) + x;
      memset_pattern4(base, &fmt, sizeof(*base)); // FIXME: Not portable.
    }
    break;

  default:
    break;
  }
}

void screen_draw_rect(screen_t *scr,
                      int x, int y,
                      int width, int height,
                      colour_t colour)
{
  box_t          screen_box;
  box_t          rect_box;
  box_t          draw_box;
  int            clipped_width, clipped_height;
  pixelfmt_any_t fmt;

  if (screen_get_clip(scr, &screen_box))
    return; /* invalid clipped screen */

  rect_box.x0 = x;
  rect_box.y0 = y;
  rect_box.x1 = x + width;
  rect_box.y1 = y + height;
  if (box_intersection(&screen_box, &rect_box, &draw_box))
    return;

  clipped_width  = draw_box.x1 - draw_box.x0;
  clipped_height = draw_box.y1 - draw_box.y0;

  fmt = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 5:
    {
      pixelfmt_any32_t *base;

      base = scr->base;
      base += draw_box.y0 * scr->rowbytes / sizeof(*base) + draw_box.x0;
      while (clipped_height--)
      {
        memset_pattern4(base, &fmt, clipped_width * sizeof(*base)); // FIXME: Not portable.
        base += scr->rowbytes / sizeof(*base);
      }
    }
    break;

  default:
    break;
  }
}

void screen_draw_square(screen_t *scr, int x, int y, int size, colour_t colour)
{
  screen_draw_rect(scr, x, y, size, size, colour);
}

// TODO: Cope with a clip region.
// TODO: Are (x1,y1) exclusive?
void screen_draw_line(screen_t *scr,
                      int x0, int y0, int x1, int y1,
                      colour_t colour)
{
  //  box_t          screen_box;
  //  box_t          line_box;
  //  box_t          draw_box;
  //  int            clipped_width, clipped_height;
  //  pixelfmt_any_t fmt;
  //
  //  if (screen_get_clip(scr, &screen_box))
  //    return; /* invalid clipped screen */
  //
  //  line_box.x0 = x0;
  //  line_box.y0 = y0;
  //  line_box.x1 = x1;
  //  line_box.y1 = y1;
  //  if (box_intersection(&screen_box, &line_box, &draw_box))
  //    return;
  //
  //  clipped_width  = draw_box.x1 - draw_box.x0;
  //  clipped_height = draw_box.y1 - draw_box.y0;

  int dx, dy;
  int adx, ady;
  int sx, sy;
  int error, e2;

#define ABS(a) ((a) < 0 ? -(a) : (a))
#define SGN(a) ((a) < 0 ? -1 : 1)

  dx  = x1 - x0;
  adx = ABS(dx);
  sx  = SGN(dx);

  dy  = y1 - y0;
  ady = -ABS(dy);
  sy  = SGN(dy);

  error = adx + ady;

  for (;;)
  {
    screen_draw_pixel(scr, x0, y0, colour);

    if (x0 == x1 && y0 == y1)
      break;

    e2 = 2 * error;
    if (e2 >= ady)
    {
      if (x0 == x1) { break; }
      error += ady;
      x0 += sx;
    }
    if (e2 <= adx)
    {
      if (y0 == y1) { break; }
      error += adx;
      y0 += sy;
    }
  }
}
