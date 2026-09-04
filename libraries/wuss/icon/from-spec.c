/* wuss/icon/from-spec.c -- validate an icon spec into a detached icon */

#include <assert.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

result_t wuss__icon_from_spec(const wuss_t           *w,
                              const wuss_icon_spec_t *spec,
                              wuss_icon_t            *out)
{
  wuss_colour_t fg, bg, swatch;
  int           has_swatch;

  assert(w    != NULL);
  assert(spec != NULL);
  assert(out  != NULL);

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

  out->window  = NULL;
  out->bbox    = spec->bbox;
  out->type    = spec->type;
  out->text    = (char *) ((spec->text != NULL) ? spec->text : "");
  out->fg      = fg;
  out->bg      = bg;
  out->pattern = (spec->type == wuss_ICON_TYPE_PATTERN) ? spec->pattern
                                                        : screen_PATTERN_SOLID;
  out->bitmap  = (spec->type == wuss_ICON_TYPE_BITMAP) ? spec->bitmap : NULL;
  out->group   = (spec->type == wuss_ICON_TYPE_RADIO) ? spec->group : 0;
  out->swatch  = swatch;
  out->flags   = spec->flags;
  out->state   = wuss_ICON_STATE_NONE;

  return result_OK;
}
