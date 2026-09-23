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
  case wuss_ICON_TYPE_WRITABLE:
    break;

  /* reserved: accepted (drawn as a label) but not implemented -- flag any
   * caller building one in debug builds */
  case wuss_ICON_TYPE_DISPLAY:
  case wuss_ICON_TYPE_NUMBER:
  case wuss_ICON_TYPE_STRING_SET:
  case wuss_ICON_TYPE_DRAGGABLE:
    assert(!"reserved wuss icon type constructed");
    break;

  case wuss_ICON_TYPE_SLIDER:
    if (spec->u.slider.orientation != wuss_SLIDER_HORIZONTAL &&
        spec->u.slider.orientation != wuss_SLIDER_VERTICAL)
      return result_WUSS_BAD_ICON;
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
      spec->u.label.border > wuss_ICON_BORDER_PLAIN)
    return result_WUSS_BAD_ICON;

  if (spec->type == wuss_ICON_TYPE_WRITABLE && spec->u.writable.size < 1)
    return result_WUSS_BAD_ICON;

  if (fg >= w->npalette)
    return result_WUSS_BAD_COLOUR;

  if (bg != wuss_NO_BACKGROUND && bg >= w->npalette)
    return result_WUSS_BAD_COLOUR;

  if (has_swatch && swatch != wuss_NO_BACKGROUND && swatch >= w->npalette)
    return result_WUSS_BAD_COLOUR;

  out->spec      = *spec;
  out->spec.text = (spec->text != NULL) ? spec->text : "";
  out->spec.fg   = fg;
  out->spec.bg   = bg;

  /* A RADIO/OPTION glyph is drawn from the icon-set bitmap (radon/radoff,
   * opton/optoff) when the set carries one, vertically centred in the bbox
   * and offset from the bbox top-left. If a state bitmap is larger than the
   * caller's bbox the glyph overhangs it, and a select-time invalidate --
   * which only covers the bbox -- then leaves the overhanging edges stale.
   * Grow the bbox to the largest of the two state bitmaps so the drawn glyph
   * always sits inside the box that hit-test, layout and invalidate use. */
  if (spec->type == wuss_ICON_TYPE_RADIO || spec->type == wuss_ICON_TYPE_OPTION)
  {
    const char     *names[2];
    const bitmap_t *state_bm;
    int             idx;
    int             gw, gh;
    int             i;

    names[0] = (spec->type == wuss_ICON_TYPE_RADIO) ? "radon"  : "opton";
    names[1] = (spec->type == wuss_ICON_TYPE_RADIO) ? "radoff" : "optoff";

    gw = 0;
    gh = 0;
    for (i = 0; i < 2; i++)
    {
      idx = wuss_icons_lookup(w, names[i]);
      if (idx < 0)
        continue;
      state_bm = wuss_icons_bitmap(w, idx);
      if (state_bm == NULL)
        continue;
      gw = MAX(gw, state_bm->size.w);
      gh = MAX(gh, state_bm->size.h);
    }

    if (gw > 0 && out->spec.bbox.x1 - out->spec.bbox.x0 < gw)
      out->spec.bbox.x1 = out->spec.bbox.x0 + gw;
    if (gh > 0 && out->spec.bbox.y1 - out->spec.bbox.y0 < gh)
      out->spec.bbox.y1 = out->spec.bbox.y0 + gh;
  }

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

  out->state       = wuss_ICON_STATE_NONE;
  out->text_scroll = 0;

  if (spec->type == wuss_ICON_TYPE_SLIDER)
  {
    int lo, hi;

    lo = MIN(spec->u.slider.min, spec->u.slider.max);
    hi = MAX(spec->u.slider.min, spec->u.slider.max);
    out->value = CLAMP(spec->u.slider.default_value, lo, hi);
  }
  else
  {
    out->value = 0;
  }

  return result_OK;
}
