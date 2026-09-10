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
  wuss_colour_t   fg, bg, swatch;
  int             has_swatch;
  const bitmap_t *bitmap;

  assert(w    != NULL);
  assert(spec != NULL);
  assert(out  != NULL);

  fg = wuss__resolve_colour(w, spec->fg);
  bg = wuss__resolve_colour(w, spec->bg);

  has_swatch = (spec->type == wuss_ICON_TYPE_MENU_ENTRY) &&
               (spec->flags & wuss_ICON_FLAGS_SWATCH);
  swatch = has_swatch ? wuss__resolve_colour(w, spec->u.menu_entry.swatch)
                      : wuss_NO_BACKGROUND;

  switch (spec->type)
  {
  case wuss_ICON_TYPE_LABEL:
  case wuss_ICON_TYPE_ACTION:
  case wuss_ICON_TYPE_PATTERN:
  case wuss_ICON_TYPE_FRAME:
  case wuss_ICON_TYPE_RADIO:
  case wuss_ICON_TYPE_OPTION:
  case wuss_ICON_TYPE_BITMAP:
  case wuss_ICON_TYPE_MENU_ENTRY:
  case wuss_ICON_TYPE_RULE:
  case wuss_ICON_TYPE_DISPLAY:
  case wuss_ICON_TYPE_WRITABLE:
  case wuss_ICON_TYPE_NUMBER:
  case wuss_ICON_TYPE_STRING_SET:
  case wuss_ICON_TYPE_SLIDER:
  case wuss_ICON_TYPE_DRAGGABLE:
    break;

  default:
    return result_WUSS_BAD_ICON;
  }

  /* an ACTION may leave bg unset -- it then draws on the config button face
   * (wuss->button_bg); a PATTERN needs a concrete clear-bit colour */
  if (spec->type == wuss_ICON_TYPE_PATTERN && bg == wuss_NO_BACKGROUND)
    return result_WUSS_BAD_ICON;

  /* A BITMAP icon draws spec->u.bitmap.image, or -- when that is NULL -- the
   * loaded icon-set entry spec->u.bitmap.set names (wuss_ICON_SET encodes the
   * 0-based index +1, so 0 means "no entry"). */
  bitmap = spec->u.bitmap.image;
  if (spec->type == wuss_ICON_TYPE_BITMAP && bitmap == NULL &&
      spec->u.bitmap.set > 0)
  {
    bitmap = wuss_icons_bitmap(w, spec->u.bitmap.set - 1);
    if (bitmap == NULL)
      return result_WUSS_BAD_INDEX;
  }

  if (spec->type == wuss_ICON_TYPE_BITMAP && bitmap == NULL)
    return result_WUSS_BAD_ICON;

  if (spec->type == wuss_ICON_TYPE_PATTERN &&
      spec->u.pattern.tile >= screen_PATTERN__LIMIT)
    return result_WUSS_BAD_ICON;

  if (spec->type == wuss_ICON_TYPE_LABEL &&
      spec->u.label.border > wuss_ICON_BORDER_DIVIDER)
    return result_WUSS_BAD_ICON;

  if (fg >= w->npalette)
    return result_WUSS_BAD_COLOUR;

  if (bg != wuss_NO_BACKGROUND && bg >= w->npalette)
    return result_WUSS_BAD_COLOUR;

  if (has_swatch && swatch >= w->npalette)
    return result_WUSS_BAD_COLOUR;

  out->spec      = *spec;
  out->spec.text = (spec->text != NULL) ? spec->text : "";
  out->spec.fg   = fg;
  out->spec.bg   = bg;

  /* out->spec.u is already the caller's copy; overwrite only the arms whose
   * value we resolved (bitmap image from the icon set, swatch to a palette
   * index) */
  switch (spec->type)
  {
  case wuss_ICON_TYPE_BITMAP:
    out->spec.u.bitmap.image = bitmap;
    break;
  case wuss_ICON_TYPE_MENU_ENTRY:
    out->spec.u.menu_entry.swatch = swatch;
    break;
  default:
    break;
  }

  out->state = wuss_ICON_STATE_NONE;

  return result_OK;
}
