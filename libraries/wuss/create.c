/* create.c -- wuss - minimal window manager */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"

#include "impl.h"

result_t wuss_create(screen_t             *scr,
                     bmfont_t             *font,
                     const colour_t       *palette,
                     int                   npalette,
                     const wuss_config_t  *config,
                     wuss_t              **wuss)
{
  wuss_t       *w;
  wuss_colour_t bg, fg;
  int           font_height;
  int           font_width;

  assert(scr  != NULL);
  assert(wuss != NULL);

  w = malloc(sizeof(*w));
  if (w == NULL)
    return result_OOM;

  if (palette != NULL)
  {
    if (npalette <= 0)
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
  }
  else
  {
    w->palette = malloc(palette_PICO8__LENGTH * sizeof(*w->palette));
    if (w->palette == NULL)
    {
      free(w);
      return result_OOM;
    }
    define_pico8_palette(w->palette);
    w->npalette = palette_PICO8__LENGTH;
  }

  if (config != NULL)
  {
    bg = config->titlebar_bg;
    fg = config->titlebar_fg;
  }
  else if (palette == NULL)
  {
    bg = palette_PICO8_DARK_BLUE;
    fg = palette_PICO8_WHITE;
  }
  else
  {
    bg = 0;
    fg = (w->npalette > 1) ? 1 : 0;
  }

  if (bg < 0 || bg >= w->npalette || fg < 0 || fg >= w->npalette)
  {
    free(w->palette);
    free(w);
    return result_WUSS_BAD_COLOUR;
  }

  w->furniture_colours.title.bg = bg;
  w->furniture_colours.title.fg = fg;
  w->furniture_colours.back     = fg;
  w->furniture_colours.close    = fg;
  w->furniture_colours.toggle   = fg;
  w->furniture_colours.resize   = bg;
  w->furniture_colours.arrows   = bg;
  w->furniture_colours.wells    = bg;
  w->furniture_colours.sausages = fg;

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

  w->scr                = scr;
  w->font               = font;
  w->furniture.dragging = NULL;
  w->furniture.drag.x   = 0;
  w->furniture.drag.y   = 0;

  w->ndirty = 0;

  list_init(&w->z_order);

  *wuss = w;

  return result_OK;
}
