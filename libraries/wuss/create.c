/* create.c -- wuss - minimal window manager */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"

#include "impl.h"

/* Built-in fallback palette when the caller passes NULL: black and white.
 * wuss makes no other assumptions about palette contents or length. */
static const colour_t wuss__default_palette[] =
{
  { 0xFF000000 }, /* 0: black */
  { 0xFFFFFFFF }  /* 1: white */
};

#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
/* Range-check the bevel colours and the backdrop against the palette. Shared
 * by the furniture and icons-only paths so the accepted range, the error code
 * and the freed-pointer set stay in one place. */
static result_t validate_bevel_backdrop(const wuss_t *w,
                                        wuss_colour_t blight,
                                        wuss_colour_t bdark,
                                        wuss_colour_t abg,
                                        wuss_colour_t afg)
{
  if (blight < 0 || blight >= w->npalette ||
      bdark  < 0 || bdark  >= w->npalette ||
      abg    < 0 || abg    >= w->npalette ||
      afg    < 0 || afg    >= w->npalette ||
      wuss__validate_backdrop(w, &w->backdrop) != result_OK)
    return result_WUSS_BAD_COLOUR;

  return result_OK;
}
#endif

result_t wuss_create(screen_t            *scr,
                     bmfont_t            *font,
                     const colour_t      *palette,
                     int                  npalette,
                     const wuss_config_t *config,
                     wuss_t             **wuss)
{
  wuss_t        *w;
#ifdef WUSS_FURNITURE
  wuss_palette_t pal;
  wuss_colour_t  bg, fg;
#endif
#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
  wuss_colour_t  blight, bdark;
  wuss_colour_t  abg, afg;
#endif
#ifdef WUSS_FURNITURE
  int            font_height;
  int            font_width;
#endif

  assert(scr  != NULL);
  assert(wuss != NULL);

  w = malloc(sizeof(*w));
  if (w == NULL)
    return result_OOM;

  if (palette == NULL)
  {
    palette  = wuss__default_palette;
    npalette = NELEMS(wuss__default_palette);
  }
  else if (npalette <= 0)
  {
    free(w);
    return result_BAD_ARG;
  }

  w->palette = malloc(npalette * sizeof(*w->palette));
  if (w->palette == NULL)
  {
    free(w);
    return result_OOM;
  }
  memcpy(w->palette, palette, npalette * sizeof(*w->palette));
  w->npalette = npalette;

  /* Cache the palette index nearest to opaque white, for menu backdrops and
   * anything else wanting "paper". Sum-of-squared-channel-distance to white;
   * ties keep the first. */
  {
    unsigned long best_d;
    int           i;

    w->white = 0;
    best_d   = ~0UL;
    for (i = 0; i < npalette; i++)
    {
      unsigned int  px;
      long          dr, dg, db;
      unsigned long d;

      px = w->palette[i].primary;
      dr = 255 - (long) ((px >> 16) & 0xFF);
      dg = 255 - (long) ((px >>  8) & 0xFF);
      db = 255 - (long) ( px        & 0xFF);
      d  = (unsigned long) (dr * dr + dg * dg + db * db);
      if (d < best_d)
      {
        best_d  = d;
        w->white = i;
      }
    }
  }

  if (config != NULL)
  {
    w->backdrop = config->backdrop;
  }
  else
  {
    w->backdrop.colour     = wuss_NO_BACKGROUND;
    w->backdrop.pattern    = screen_PATTERN_SOLID;
    w->backdrop.pattern_bg = wuss_NO_BACKGROUND;
  }

#ifdef WUSS_FURNITURE
  if (config != NULL)
  {
    pal = config->palette;
    blight = config->bevel.light;
    bdark = config->bevel.dark;
    abg = config->accent.bg;
    afg = config->accent.fg;
  }
  else
  {
    bg = 0;
    fg = (w->npalette > 1) ? 1 : 0;

    pal.title.bg        = bg;
    pal.title.fg        = fg;
    pal.back            = fg;
    pal.close           = fg;
    pal.toggle          = fg;
    pal.resize          = bg;
    pal.scroll.arrows   = bg;
    pal.scroll.wells    = bg;
    pal.scroll.sausages = fg;

    blight = 0;
    bdark  = 0;
    abg    = bg; /* default action button: the titlebar colours */
    afg    = fg;
  }

  if (pal.title.bg        < 0 || pal.title.bg        >= w->npalette ||
      pal.title.fg        < 0 || pal.title.fg        >= w->npalette ||
      pal.back            < 0 || pal.back            >= w->npalette ||
      pal.close           < 0 || pal.close           >= w->npalette ||
      pal.toggle          < 0 || pal.toggle          >= w->npalette ||
      pal.resize          < 0 || pal.resize          >= w->npalette ||
      pal.scroll.arrows   < 0 || pal.scroll.arrows   >= w->npalette ||
      pal.scroll.wells    < 0 || pal.scroll.wells    >= w->npalette ||
      pal.scroll.sausages < 0 || pal.scroll.sausages >= w->npalette ||
      validate_bevel_backdrop(w, blight, bdark, abg, afg) != result_OK)
  {
    free(w->palette);
    free(w);
    return result_WUSS_BAD_COLOUR;
  }

  w->furniture_colours = pal;
  w->bevel_light       = blight;
  w->bevel_dark        = bdark;
  w->accent_bg         = abg;
  w->accent_fg         = afg;

  if (config != NULL && config->titlebar_height > 0)
  {
    w->titlebar_height = config->titlebar_height;
  }
  else if (font != NULL)
  {
    bmfont_get_info(font, &font_width, &font_height);
    NOT_USED(font_width);
    w->titlebar_height = font_height + 4;
  }
  else
  {
    w->titlebar_height = WUSS_DEFAULT_TITLEBAR_HEIGHT;
  }
#else /* !WUSS_FURNITURE */
#ifdef WUSS_ICONS
  if (config != NULL)
  {
    blight = config->bevel.light;
    bdark  = config->bevel.dark;
    abg    = config->accent.bg;
    afg    = config->accent.fg;
  }
  else
  {
    blight = 0;
    bdark  = 0;
    abg    = 0;
    afg    = (w->npalette > 1) ? 1 : 0;
  }
  if (validate_bevel_backdrop(w, blight, bdark, abg, afg) != result_OK)
  {
    free(w->palette);
    free(w);
    return result_WUSS_BAD_COLOUR;
  }
  w->bevel_light = blight;
  w->bevel_dark  = bdark;
  w->accent_bg   = abg;
  w->accent_fg   = afg;
#else
  if (wuss__validate_backdrop(w, &w->backdrop) != result_OK)
  {
    free(w->palette);
    free(w);
    return result_WUSS_BAD_COLOUR;
  }
#endif
#endif /* WUSS_FURNITURE */

  w->scr                = scr;
  w->font               = font;
#ifdef WUSS_FURNITURE
  w->furniture.dragging = NULL;
  w->furniture.drag.x   = 0;
  w->furniture.drag.y   = 0;
#endif
#ifdef WUSS_ICONS
  w->pressed_icon       = NULL;
  w->hover_icon         = NULL;
#endif
#ifdef WUSS_MENUS
  w->menu_chain         = NULL;
#endif

  w->ndirty = 0;

  w->layout    = NULL;
  w->cascade.x = 0;
  w->cascade.y = 0;
  w->pointer.x = 0;
  w->pointer.y = 0;

  list_init(&w->z_order);

  *wuss = w;

  return result_OK;
}
