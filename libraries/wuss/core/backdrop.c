/* wuss/backdrop.c -- shared backdrop validation and fill */

#include "framebuf/screen.h"

#include "impl.h"

result_t wuss__validate_backdrop(const wuss_t          *wuss,
                                 const wuss_backdrop_t *backdrop)
{
  if (backdrop->colour == wuss_NO_BACKGROUND)
    return result_OK;

  if (backdrop->colour >= wuss->npalette)
    return result_WUSS_BAD_COLOUR;

  if (backdrop->pattern >= screen_PATTERN__LIMIT)
    return result_WUSS_BAD_COLOUR;

  if (backdrop->pattern != screen_PATTERN_SOLID &&
      backdrop->pattern_bg >= wuss->npalette)
    return result_WUSS_BAD_COLOUR;

  return result_OK;
}

void wuss__fill_backdrop(screen_t              *scr,
                         const colour_t        *palette,
                         const wuss_backdrop_t *backdrop,
                         const box_t           *area,
                         int                    origin_x,
                         int                    origin_y)
{
  if (backdrop->colour == wuss_NO_BACKGROUND)
    return;

  if (backdrop->pattern == screen_PATTERN_SOLID)
  {
    screen_fill_rect(scr, area->x0, area->y0, box_size(area),
                     palette[backdrop->colour]);
  }
  else
  {
    pattern_t pat;

    pat = pattern_from_preset(backdrop->pattern,
                              palette[backdrop->colour],
                              palette[backdrop->pattern_bg]);
    pat.origin = POINT(origin_x, origin_y);
    screen_fill_pattern(scr, area, &pat);
  }

  if (backdrop->image != NULL)
  {
    int cx, cy;

    /* centred on the whole screen, not "area" -- area is only whatever
     * piece of the backdrop this call is repainting (e.g. one exposed
     * sliver around a window), so the image must sit at a position fixed
     * relative to the screen for pieces to line back up into one picture.
     * scr->clip (set by the caller to "area") crops the blit to it. */
    cx = (scr->size.w - backdrop->image->size.w) / 2;
    cy = (scr->size.h - backdrop->image->size.h) / 2;
    screen_copy_bitmap(scr, cx, cy, backdrop->image);
  }
}
