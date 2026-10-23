/* wuss/core/palette.h -- wuss - palette, colours and backdrop */

#ifndef WUSS_CORE_PALETTE_H
#define WUSS_CORE_PALETTE_H

#include "types.h"


/* Rebuild wuss->palettecache (white, black and the symbolic[] table) from
 * the current palette and the stored chrome colours. Call after the palette
 * or any chrome colour changes; the chrome fields must already be concrete
 * indices (resolve config through wuss__resolve_colour before storing). */
void wuss__rebuild_palettecache(wuss_t *wuss);

/* Deliver wuss_EVENT_PALETTE to every task. Returns the first handler error,
 * else result_OK; every task is still told. */
result_t wuss__broadcast_palette(wuss_t *wuss);

/* Resolve, validate and store config's colours (and the titlebar height)
 * into w, then rebuild the palette cache. NULL config means the defaults.
 * Needs w's palette and fonts set. Returns result_WUSS_BAD_COLOUR with w
 * part-updated on a bad colour -- the caller discards or restores w. */
result_t wuss__apply_config(wuss_t *w, const wuss_config_t *config);

#ifdef WUSS_FURNITURE
/* Titlebar height derived from w's current fonts: the taller of the regular
 * and bold faces plus a margin, or WUSS_DEFAULT_TITLEBAR_HEIGHT if both
 * slots are empty. Used when the config gives no explicit height. */
int wuss__titlebar_height_auto(const wuss_t *w);
#endif

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

/* Recompute wuss->autofill/autosolid from wuss->palette (the desktop
 * palette) against wuss->scr's physical palette: a colour the physical
 * palette matches exactly -- always so on a direct-colour screen -- or whose
 * best dither is two identical physical colours gets a solid fast path,
 * anything else a Bayer dither of the two nearest physical colours. Needs
 * wuss->scr set; call after the desktop or physical palette changes. */
void wuss__rebuild_fills(wuss_t *wuss);

/* On "scr" (normally wuss->scr, or a clipped copy), fill "area" with the desktop palette colour "index" (a user fill from
 * wuss_set_colour_fill if it has one, else its automatic fill), phased
 * against (origin_x, origin_y). Caller sets scr->clip. */
void wuss__fill_index(const wuss_t *wuss,
                      screen_t     *scr,
                      int           index,
                      const box_t  *area,
                      int           origin_x,
                      int           origin_y);

/* As wuss__fill_index for the desktop colour whose RGB equals "colour"
 * (phased against "origin"); a plain screen_fill_rect if no palette entry
 * matches. For chrome code that holds colour_t values rather than indices.
 * Pass the same origin the window's own background uses (content origin
 * less scroll) so dithered fills line up with it and survive blits.
 * ponytail: linear reverse lookup, fine for <=128 entries; pass indices
 * through if it shows in a profile. */
void wuss__fill_colour(const wuss_t *wuss,
                       screen_t     *scr,
                       const box_t  *area,
                       colour_t      colour,
                       point_t       origin);

/* wuss__fill_area for backdrop->fill, then backdrop->image centred on the
 * screen. */
void wuss__fill_backdrop(const wuss_t          *wuss,
                         const wuss_backdrop_t *backdrop,
                         const box_t           *area,
                         int                    origin_x,
                         int                    origin_y);

#endif /* WUSS_CORE_PALETTE_H */
