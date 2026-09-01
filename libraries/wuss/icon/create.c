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

  assert(window != NULL);
  assert(spec   != NULL);

  w = window->wuss;

  if (spec->type != wuss_ICON_TYPE_LABEL &&
      spec->type != wuss_ICON_TYPE_BUTTON &&
      spec->type != wuss_ICON_TYPE_PATTERN &&
      spec->type != wuss_ICON_TYPE_FRAME &&
      spec->type != wuss_ICON_TYPE_RADIO &&
      spec->type != wuss_ICON_TYPE_OPTION &&
      spec->type != wuss_ICON_TYPE_BITMAP &&
      spec->type != wuss_ICON_TYPE_MENU_ENTRY &&
      spec->type != wuss_ICON_TYPE_RULE)
    return result_WUSS_BAD_ICON;

  if ((spec->type == wuss_ICON_TYPE_BUTTON ||
       spec->type == wuss_ICON_TYPE_PATTERN) &&
      spec->bg == wuss_NO_BACKGROUND)
    return result_WUSS_BAD_ICON;

  if (spec->type == wuss_ICON_TYPE_BITMAP && spec->bitmap == NULL)
    return result_WUSS_BAD_ICON;

  if (spec->type == wuss_ICON_TYPE_PATTERN &&
      (spec->pattern < 0 || spec->pattern >= screen_PATTERN__LIMIT))
    return result_WUSS_BAD_ICON;

  if (spec->fg < 0 || spec->fg >= w->npalette)
    return result_WUSS_BAD_COLOUR;

  if (spec->bg != wuss_NO_BACKGROUND &&
      (spec->bg < 0 || spec->bg >= w->npalette))
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
  it->fg       = spec->fg;
  it->bg       = spec->bg;
  it->pattern  = (spec->type == wuss_ICON_TYPE_PATTERN) ? spec->pattern
                                                        : screen_PATTERN_SOLID;
  it->bitmap   = (spec->type == wuss_ICON_TYPE_BITMAP) ? spec->bitmap : NULL;
  it->group    = (spec->type == wuss_ICON_TYPE_RADIO) ? spec->group : 0;
  it->flags    = spec->flags;
  it->state    = wuss_ICON_STATE_NONE;

  window->icons[window->nicons++] = it;

  wuss__icon_invalidate(it);

  if (icon != NULL)
    *icon = it;

  return result_OK;
}
