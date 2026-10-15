/* wuss/core/palette.h -- wuss - palette, colours and backdrop */

#ifndef WUSS_CORE_PALETTE_H
#define WUSS_CORE_PALETTE_H

#include "types.h"


/* Rebuild wuss->palettecache (white, black and the symbolic[] table) from
 * the current palette and the stored chrome colours. Call after the palette
 * or any chrome colour changes; the chrome fields must already be concrete
 * indices (resolve config through wuss__resolve_colour before storing). */
void wuss__rebuild_palettecache(wuss_t *wuss);

/* Resolve, validate and store config's colours (and the titlebar height)
 * into w, then rebuild the palette cache. NULL config means the defaults.
 * Needs w's palette and fonts set. Returns result_WUSS_BAD_COLOUR with w
 * part-updated on a bad colour -- the caller discards or restores w. */
result_t wuss__apply_config(wuss_t *w, const wuss_config_t *config);

/* Concrete 0..npalette-1 palette index for any wuss_colour_t: a symbolic
 * value (>= wuss_COLOUR_SYMBOLIC, bar wuss_NO_BACKGROUND) via the cache,
 * everything else -- raw indices, wuss_NO_BACKGROUND -- unchanged. */
static inline wuss_colour_t wuss__resolve_colour(const wuss_t *wuss,
                                                 wuss_colour_t c)
{
  if (c >= wuss_COLOUR_SYMBOLIC && c != wuss_NO_BACKGROUND)
    return wuss->palettecache[c - wuss_COLOUR_SYMBOLIC];
  return c;
}

/* Range-check a fill against the palette: its colour, and -- when a
 * non-solid pattern is set -- its pattern index and background colour.
 * Returns result_OK or result_WUSS_BAD_COLOUR. */
result_t wuss__validate_fill(const wuss_t      *wuss,
                             const wuss_fill_t *fill);

/* Paint "fill" into "area" on wuss->scr: a flat fill in the SOLID case
 * (through the colour's own wuss->fills[] entry, which may be a pattern),
 * otherwise the 8x8 pattern tiled in colour over pattern_bg, phased against
 * (origin_x, origin_y). No-op when colour is wuss_NO_BACKGROUND. Caller sets
 * scr->clip. */
void wuss__fill_area(const wuss_t      *wuss,
                     const wuss_fill_t *fill,
                     const box_t       *area,
                     int                origin_x,
                     int                origin_y);

/* wuss__fill_area for backdrop->fill, then backdrop->image centred on the
 * screen. */
void wuss__fill_backdrop(const wuss_t          *wuss,
                         const wuss_backdrop_t *backdrop,
                         const box_t           *area,
                         int                    origin_x,
                         int                    origin_y);

#endif /* WUSS_CORE_PALETTE_H */
