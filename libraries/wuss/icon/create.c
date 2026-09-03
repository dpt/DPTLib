/* wuss/icon/create.c -- create a work-area icon */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

result_t wuss_icon_create(wuss_window_t          *window,
                          const wuss_icon_spec_t *spec,
                          wuss_icon_t           **icon)
{
  wuss_t      *w;
  wuss_icon_t *it;
  wuss_icon_t **grown;
  const char  *src;
  size_t       len;
  int          newcap;
  wuss_colour_t fg, bg, swatch;
  int          has_swatch;

  assert(window != NULL);
  assert(spec   != NULL);

  w = window->wuss;

  fg = wuss__resolve_colour(w, spec->fg);
  bg = wuss__resolve_colour(w, spec->bg);

  has_swatch = (spec->type == wuss_ICON_TYPE_MENU_ENTRY) &&
               (spec->flags & wuss_ICON_FLAGS_SWATCH);
  swatch = has_swatch ? wuss__resolve_colour(w, spec->swatch)
                      : wuss_NO_BACKGROUND;

  switch (spec->type)
  {
  case wuss_ICON_TYPE_LABEL:
  case wuss_ICON_TYPE_BUTTON:
  case wuss_ICON_TYPE_PATTERN:
  case wuss_ICON_TYPE_FRAME:
  case wuss_ICON_TYPE_RADIO:
  case wuss_ICON_TYPE_OPTION:
  case wuss_ICON_TYPE_BITMAP:
  case wuss_ICON_TYPE_MENU_ENTRY:
  case wuss_ICON_TYPE_RULE:
    break;

  default:
    return result_WUSS_BAD_ICON;
  }

  if ((spec->type == wuss_ICON_TYPE_BUTTON ||
       spec->type == wuss_ICON_TYPE_PATTERN) &&
      bg == wuss_NO_BACKGROUND)
    return result_WUSS_BAD_ICON;

  if (spec->type == wuss_ICON_TYPE_BITMAP && spec->bitmap == NULL)
    return result_WUSS_BAD_ICON;

  if (spec->type == wuss_ICON_TYPE_PATTERN &&
      (spec->pattern < 0 || spec->pattern >= screen_PATTERN__LIMIT))
    return result_WUSS_BAD_ICON;

  if (fg < 0 || fg >= w->npalette)
    return result_WUSS_BAD_COLOUR;

  if (bg != wuss_NO_BACKGROUND &&
      (bg < 0 || bg >= w->npalette))
    return result_WUSS_BAD_COLOUR;

  if (has_swatch && (swatch < 0 || swatch >= w->npalette))
    return result_WUSS_BAD_COLOUR;

  if (window->nicons == window->cap_icons)
  {
    newcap = (window->cap_icons == 0) ? 4 : window->cap_icons * 2;
    grown  = wuss__realloc(w, window->icons, newcap * sizeof(*window->icons));
    if (grown == NULL)
      return result_OOM;
    window->icons     = grown;
    window->cap_icons = newcap;
  }

  it = wuss__malloc(w, sizeof(*it));
  if (it == NULL)
    return result_OOM;

  src = (spec->text != NULL) ? spec->text : "";
  len = strlen(src);
  it->text = wuss__malloc(w, len + 1);
  if (it->text == NULL)
  {
    wuss__free(w, it);
    return result_OOM;
  }
  memcpy(it->text, src, len + 1);

  it->window   = window;
  it->bbox     = spec->bbox;
  it->type     = spec->type;
  it->fg       = fg;
  it->bg       = bg;
  it->pattern  = (spec->type == wuss_ICON_TYPE_PATTERN) ? spec->pattern
                                                        : screen_PATTERN_SOLID;
  it->bitmap   = (spec->type == wuss_ICON_TYPE_BITMAP) ? spec->bitmap : NULL;
  it->group    = (spec->type == wuss_ICON_TYPE_RADIO) ? spec->group : 0;
  it->swatch   = swatch;
  it->flags    = spec->flags;
  it->state    = wuss_ICON_STATE_NONE;

  window->icons[window->nicons++] = it;

  wuss__icon_invalidate(it);

  if (icon != NULL)
    *icon = it;

  return result_OK;
}
