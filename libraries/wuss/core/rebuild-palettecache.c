/* wuss/rebuild-palettecache.c -- resolve symbolic colours for the current palette */

#include <stddef.h>

#include "framebuf/colour.h"

#include "impl.h"

/* RGB each named symbolic colour resolves to, in wuss_COLOUR_BLACK.. order. */
static const unsigned char wuss__named_rgb[][3] =
{
  { 0x00, 0x00, 0x00 }, /* wuss_COLOUR_BLACK   */
  { 0xFF, 0xFF, 0xFF }, /* wuss_COLOUR_WHITE   */
  { 0xFF, 0x00, 0x00 }, /* wuss_COLOUR_RED     */
  { 0x00, 0xFF, 0x00 }, /* wuss_COLOUR_GREEN   */
  { 0x00, 0x00, 0xFF }, /* wuss_COLOUR_BLUE    */
  { 0xFF, 0xFF, 0x00 }, /* wuss_COLOUR_YELLOW  */
  { 0x00, 0xFF, 0xFF }, /* wuss_COLOUR_CYAN    */
  { 0xFF, 0x00, 0xFF }, /* wuss_COLOUR_MAGENTA */
  { 0x80, 0x80, 0x80 }  /* wuss_COLOUR_GREY    */
};

void wuss__rebuild_palettecache(wuss_t *wuss)
{
  wuss_colour_t *cache;
  size_t         i;

  cache = wuss->palettecache;

  /* Default every slot to wuss_COLOUR_SYMBOLIC: always >= npalette (palettes
   * hold at most 128 real entries) so an unmapped symbolic still fails every
   * range check, and -- unlike wuss_NO_BACKGROUND -- it is not mistaken for
   * "no background" by wuss__validate_backdrop. */
  for (i = 0; i < NELEMS(wuss->palettecache); i++)
    cache[i] = wuss_COLOUR_SYMBOLIC;

  for (i = 0; i < NELEMS(wuss__named_rgb); i++)
    cache[(wuss_COLOUR_BLACK - wuss_COLOUR_SYMBOLIC) + i] =
      wuss_nearest_colour(wuss,
                          wuss__named_rgb[i][0],
                          wuss__named_rgb[i][1],
                          wuss__named_rgb[i][2]);

#ifdef WUSS_FURNITURE
  cache[wuss_COLOUR_TITLE_BG - wuss_COLOUR_SYMBOLIC] =
    wuss->furniture_colours.title.bg;
  cache[wuss_COLOUR_TITLE_FG - wuss_COLOUR_SYMBOLIC] =
    wuss->furniture_colours.title.fg;
#endif
#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
  cache[wuss_COLOUR_BUTTON_HILIGHT - wuss_COLOUR_SYMBOLIC] = wuss->bevel_light;
  cache[wuss_COLOUR_BUTTON_SHADOW  - wuss_COLOUR_SYMBOLIC] = wuss->bevel_dark;
  cache[wuss_COLOUR_ACCENT_BG      - wuss_COLOUR_SYMBOLIC] = wuss->accent_bg;
  cache[wuss_COLOUR_ACCENT_FG      - wuss_COLOUR_SYMBOLIC] = wuss->accent_fg;
#endif
  if (wuss->backdrop.colour != wuss_NO_BACKGROUND)
    cache[wuss_COLOUR_BACKDROP - wuss_COLOUR_SYMBOLIC] = wuss->backdrop.colour;
}
