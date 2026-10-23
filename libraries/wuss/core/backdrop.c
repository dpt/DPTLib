/* wuss/backdrop.c -- shared fill/backdrop validation and fill */

#include "framebuf/screen.h"

#include "impl.h"

result_t wuss__validate_fill(const wuss_t      *wuss,
                             const wuss_fill_t *fill)
{
  if (fill->colour == wuss_NO_BACKGROUND)
    return result_OK;

  if (fill->colour >= wuss->npalette)
    return result_WUSS_BAD_COLOUR;

  if (fill->pattern >= screen_PATTERN__LIMIT)
    return result_WUSS_BAD_COLOUR;

  if (fill->pattern != screen_PATTERN_SOLID &&
      fill->pattern_bg >= wuss->npalette)
    return result_WUSS_BAD_COLOUR;

  return result_OK;
}

static int physical_entries(const screen_t *scr)
{
  switch (scr->format)
  {
  case pixelfmt_p1: return 2;
  case pixelfmt_p2: return 4;
  case pixelfmt_p4: return 16;
  case pixelfmt_p8: return 256;
  default:          return 0; /* direct colour */
  }
}

void wuss__rebuild_fills(wuss_t *wuss)
{
  const screen_t *scr;
  int             nphys;
  int             i;

  scr   = wuss->scr;
  nphys = (scr->palette != NULL) ? physical_entries(scr) : 0;

  for (i = 0; i < wuss->npalette; i++)
  {
    pattern_t pat;
    unsigned  r0, g0, b0, r1, g1, b1;
    bool      uniform;

    if (nphys == 0)
    {
      pat = pattern_from_preset(screen_PATTERN_SOLID, wuss->palette[i],
                                wuss->palette[i]);
      wuss->autofill[i]  = pat;
      wuss->autosolid[i] = true;
      continue;
    }

    pat = pattern_from_colour(scr->palette, nphys, wuss->palette[i]);

    colour_get_rgb(&pat.fg, &r0, &g0, &b0);
    colour_get_rgb(&pat.bg, &r1, &g1, &b1);
    uniform = pat.bits[0] == 0x00 || pat.bits[0] == 0xFF;
    if (uniform)
    {
      int k;

      for (k = 1; k < 8; k++)
        if (pat.bits[k] != pat.bits[0])
          uniform = false;
    }

    wuss->autosolid[i] = uniform || (r0 == r1 && g0 == g1 && b0 == b1);
    if (wuss->autosolid[i] && pat.bits[0] == 0x00)
      pat.fg = pat.bg; /* solid colour always lives in fg */
    wuss->autofill[i] = pat;
  }
}

void wuss__fill_index(const wuss_t *wuss,
                      screen_t     *scr,
                      int           index,
                      const box_t  *area,
                      int           origin_x,
                      int           origin_y)
{
  const wuss_fill_t *user;
  pattern_t          pat;

  user = &wuss->fills[index];
  if (user->colour != wuss_NO_BACKGROUND)
  {
    if (user->pattern != screen_PATTERN_SOLID)
    {
      pat = pattern_from_preset(user->pattern,
                                wuss->palette[user->colour],
                                wuss->palette[user->pattern_bg]);
      pat.origin = POINT(origin_x, origin_y);
      screen_fill_pattern(scr, area, &pat);
      return;
    }

    index = user->colour; /* one level only: its automatic fill */
  }

  if (wuss->autosolid[index])
  {
    screen_fill_rect(scr, area->x0, area->y0, box_size(area),
                     wuss->autofill[index].fg);
    return;
  }

  pat        = wuss->autofill[index];
  pat.origin = POINT(origin_x, origin_y);
  screen_fill_pattern(scr, area, &pat);
}

void wuss__fill_colour(const wuss_t *wuss,
                       screen_t     *scr,
                       const box_t  *area,
                       colour_t      colour,
                       point_t       origin)
{
  unsigned r, g, b;
  int      i;

  colour_get_rgb(&colour, &r, &g, &b);

  for (i = 0; i < wuss->npalette; i++)
  {
    unsigned pr, pg, pb;

    colour_get_rgb(&wuss->palette[i], &pr, &pg, &pb);
    if (pr == r && pg == g && pb == b)
    {
      wuss__fill_index(wuss, scr, i, area, origin.x, origin.y);
      return;
    }
  }

  screen_fill_rect(scr, area->x0, area->y0, box_size(area), colour);
}

void wuss__fill_area(const wuss_t      *wuss,
                     const wuss_fill_t *fill,
                     const box_t       *area,
                     int                origin_x,
                     int                origin_y)
{
  pattern_t pat;

  if (fill->colour == wuss_NO_BACKGROUND)
    return;

  if (fill->pattern == screen_PATTERN_SOLID)
  {
    wuss__fill_index(wuss, wuss->scr, fill->colour, area, origin_x, origin_y);
    return;
  }

  pat = pattern_from_preset(fill->pattern,
                            wuss->palette[fill->colour],
                            wuss->palette[fill->pattern_bg]);
  pat.origin = POINT(origin_x, origin_y);
  screen_fill_pattern(wuss->scr, area, &pat);
}

void wuss__fill_backdrop(const wuss_t          *wuss,
                         const wuss_backdrop_t *backdrop,
                         const box_t           *area,
                         int                    origin_x,
                         int                    origin_y)
{
  screen_t *scr;
  int       cx, cy;

  if (backdrop->fill.colour == wuss_NO_BACKGROUND)
    return;

  wuss__fill_area(wuss, &backdrop->fill, area, origin_x, origin_y);

  if (backdrop->image == NULL)
    return;

  /* centred on the whole screen, not "area" -- area is only whatever
   * piece of the backdrop this call is repainting (e.g. one exposed
   * sliver around a window), so the image must sit at a position fixed
   * relative to the screen for pieces to line back up into one picture.
   * scr->clip (set by the caller to "area") crops the blit to it. */
  scr = wuss->scr;
  cx  = (scr->size.w - backdrop->image->size.w) / 2;
  cy  = (scr->size.h - backdrop->image->size.h) / 2;
  screen_copy_bitmap(scr, cx, cy, backdrop->image);
}
