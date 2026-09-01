/* wuss/backdrop.c -- shared backdrop validation and fill */

#include "framebuf/screen.h"

#include "impl.h"

result_t wuss__validate_backdrop(const wuss_t          *wuss,
                                 const wuss_backdrop_t *backdrop)
{
  if (backdrop->colour == wuss_NO_BACKGROUND)
    return result_OK;

  if (backdrop->colour < 0 || backdrop->colour >= wuss->npalette)
    return result_WUSS_BAD_COLOUR;

  if (backdrop->pattern < 0 || backdrop->pattern >= screen_PATTERN__LIMIT)
    return result_WUSS_BAD_COLOUR;

  if (backdrop->pattern != screen_PATTERN_SOLID &&
      (backdrop->pattern_bg < 0 || backdrop->pattern_bg >= wuss->npalette))
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
    screen_fill_rect(scr, area->x0, area->y0, box_size(area),
                     palette[backdrop->colour]);
  else
    screen_fill_pattern(scr, area, backdrop->pattern, origin_x, origin_y,
                        palette[backdrop->colour],
                        palette[backdrop->pattern_bg]);
}
