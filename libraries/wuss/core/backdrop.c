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

void wuss__fill_area(const wuss_t      *wuss,
                     const wuss_fill_t *fill,
                     const box_t       *area,
                     int                origin_x,
                     int                origin_y)
{
  screen_t *scr;
  pattern_t pat;

  if (fill->colour == wuss_NO_BACKGROUND)
    return;

  /* a solid fill of a colour paints that colour's own fill (one level only:
   * a colour's table entry is used as given) */
  if (fill->pattern == screen_PATTERN_SOLID)
    fill = &wuss->fills[fill->colour];

  scr = wuss->scr;

  if (fill->pattern == screen_PATTERN_SOLID)
  {
    screen_fill_rect(scr, area->x0, area->y0, box_size(area),
                     wuss->palette[fill->colour]);
    return;
  }

  pat = pattern_from_preset(fill->pattern,
                            wuss->palette[fill->colour],
                            wuss->palette[fill->pattern_bg]);
  pat.origin = POINT(origin_x, origin_y);
  screen_fill_pattern(scr, area, &pat);
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
