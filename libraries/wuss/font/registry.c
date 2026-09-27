/* wuss/font/registry.c -- wuss font slot registry and lookups */

#include <assert.h>
#include <limits.h>

#include "framebuf/bmfont.h"

#include "wuss/wuss.h"

#include "../core/impl.h"
#include "font.h"

void wuss__fontset_init(struct wuss_fontset    *set,
                        const wuss_font_desc_t *fonts,
                        int                     nfonts)
{
  int i;

  assert(set != NULL);
  assert(nfonts >= 0 && nfonts <= wuss_MAX_FONTS);

  for (i = 0; i < nfonts; i++)
  {
    set->fonts[i]        = fonts[i].font;
    set->font_classes[i] = fonts[i].font_class;
    set->font_names[i]   = fonts[i].name;
    set->space_widths[i] = 0;
    if (fonts[i].font != NULL)
      wuss__text_measure(fonts[i].font, " ", 1, INT_MAX, NULL,
                         &set->space_widths[i]);
  }
  for (; i < wuss_MAX_FONTS; i++)
  {
    set->fonts[i]        = NULL;
    set->font_classes[i] = wuss_FONT_CLASS_NONE;
    set->font_names[i]   = NULL;
    set->space_widths[i] = 0;
  }
  set->nfonts = nfonts;
}

int wuss__fontset_height(const struct wuss_fontset *set, int slot)
{
  int height;

  assert(set != NULL);

  if (slot < 0 || slot >= wuss_MAX_FONTS || set->fonts[slot] == NULL)
    return 0;

  bmfont_get_info(set->fonts[slot], NULL, &height, NULL, NULL);
  return height;
}

bmfont_width_t wuss__fontset_space_width(const struct wuss_fontset *set,
                                         const bmfont_t            *font)
{
  int i;

  assert(set != NULL);

  for (i = 0; i < wuss_MAX_FONTS; i++)
    if (set->fonts[i] == font)
      return set->space_widths[i];

  return 0;
}

/* ----------------------------------------------------------------------- */

bmfont_t *wuss_get_font(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->fonts.fonts[0];
}

bmfont_t *wuss__bold_font(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return (wuss->fonts.nfonts > 1 && wuss->fonts.fonts[1] != NULL)
       ? wuss->fonts.fonts[1]
       : wuss->fonts.fonts[0];
}

bmfont_t *wuss_get_font_n(const wuss_t *wuss, int index)
{
  assert(wuss != NULL);

  if (index < 0 || index >= wuss_MAX_FONTS)
    return NULL;

  return wuss->fonts.fonts[index];
}

wuss_font_class_t wuss_get_font_class_n(const wuss_t *wuss, int index)
{
  assert(wuss != NULL);

  if (index < 0 || index >= wuss_MAX_FONTS)
    return wuss_FONT_CLASS_NONE;

  return wuss->fonts.font_classes[index];
}

const char *wuss_get_font_name_n(const wuss_t *wuss, int index)
{
  assert(wuss != NULL);

  if (index < 0 || index >= wuss_MAX_FONTS)
    return NULL;

  return wuss->fonts.font_names[index];
}

bmfontcache_t *wuss_get_font_cache(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->font_cache;
}
