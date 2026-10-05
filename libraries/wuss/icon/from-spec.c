/* wuss/icon/from-spec.c -- validate an icon spec into a detached icon */

#include <assert.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

result_t wuss__icon_from_spec(const wuss_t           *w,
                              const wuss_task_t      *task,
                              const wuss_icon_spec_t *spec,
                              wuss_icon_t            *out)
{
  wuss_colour_t   fg, bg, swatch;
  int             has_swatch;
  int             is_bitmap, name_missing;
  const bitmap_t *bitmap, *pressed;

  assert(w    != NULL);
  assert(spec != NULL);
  assert(out  != NULL);

  fg = wuss__resolve_colour(w, spec->fg);
  bg = wuss__resolve_colour(w, spec->bg);

  has_swatch = (spec->type == wuss_ICON_TYPE_MENU_ENTRY) &&
               (spec->flags & wuss_ICON_FLAGS_SWATCH);
  swatch = has_swatch ? wuss__resolve_colour(w, spec->u.menu_entry.swatch)
                      : wuss_NO_BACKGROUND;

  if ((unsigned) spec->type >= wuss__ICON_TYPE_COUNT)
    return result_WUSS_BAD_ICON;

  /* reserved: accepted (drawn as a label) but not implemented -- flag any
   * caller building one in debug builds */
  if (wuss__icon_types[spec->type].reserved)
    assert(!"reserved wuss icon type constructed");

  if (spec->type == wuss_ICON_TYPE_SLIDER &&
      spec->u.slider.orientation != wuss_SLIDER_HORIZONTAL &&
      spec->u.slider.orientation != wuss_SLIDER_VERTICAL)
    return result_WUSS_BAD_ICON;

  if ((spec->flags & wuss_ICON_FLAGS_DEFAULT) &&
      spec->type != wuss_ICON_TYPE_ACTION)
    return result_BAD_ARG;

  if ((spec->flags & wuss_ICON_FLAGS_INTERACTIVE) &&
      spec->type != wuss_ICON_TYPE_BITMAP)
    return result_BAD_ARG;

  if ((spec->flags & (wuss_ICON_FLAGS_SUBMENU | wuss_ICON_FLAGS_SEPARATOR |
                       wuss_ICON_FLAGS_SWATCH)) &&
      spec->type != wuss_ICON_TYPE_MENU_ENTRY)
    return result_BAD_ARG;

  /* an ACTION may leave bg unset -- it then draws on the config button face
   * (wuss->button.bg); a PATTERN needs a concrete clear-bit colour */
  if (spec->type == wuss_ICON_TYPE_PATTERN && bg == wuss_NO_BACKGROUND)
    return result_WUSS_BAD_ICON;

  /* A BITMAP (or DRAGGABLE) icon draws spec->u.bitmap.image, or -- when that
   * is NULL -- the icon pool entry spec->u.bitmap.set names
   * (wuss_ICONPOOL_SET encodes the 0-based index +1, so 0 means "no entry"),
   * or failing that the one spec->u.bitmap.name names (the owning task's
   * private pool first, then the shared one). A name the pool lacks
   * is warned about and leaves the bitmap NULL, so the icon draws the red
   * placeholder box rather than failing creation. A
   * DRAGGABLE may have neither: it is still a drag handle, drawn as the red
 * box that stands in for any bitmap that can't be drawn. */
  is_bitmap = (spec->type == wuss_ICON_TYPE_BITMAP ||
               spec->type == wuss_ICON_TYPE_DRAGGABLE);

  bitmap       = spec->u.bitmap.image;
  pressed      = spec->u.bitmap.pressed_image;
  name_missing = 0;

  if (is_bitmap && bitmap == NULL && spec->u.bitmap.set == 0 &&
      spec->u.bitmap.name != NULL)
  {
    bitmap = wuss__iconpool_find(w, task, spec->u.bitmap.name);
    if (bitmap == NULL)
    {
      logf_warning("wuss: icon pool has no \"%s\"", spec->u.bitmap.name);
      name_missing = 1;
    }
  }

  if (is_bitmap && pressed == NULL && spec->u.bitmap.pressed_set == 0 &&
      spec->u.bitmap.pressed_name != NULL)
  {
    pressed = wuss__iconpool_find(w, task, spec->u.bitmap.pressed_name);
    if (pressed == NULL)
      logf_warning("wuss: icon pool has no \"%s\"",
                   spec->u.bitmap.pressed_name);
  }

  if (is_bitmap && bitmap == NULL && spec->u.bitmap.set > 0)
  {
    bitmap = wuss_iconpool_bitmap(w, spec->u.bitmap.set - 1);
    if (bitmap == NULL)
      return result_WUSS_BAD_INDEX;
  }

  if (spec->type == wuss_ICON_TYPE_BITMAP && bitmap == NULL && !name_missing)
    return result_WUSS_BAD_ICON;

  if (spec->type == wuss_ICON_TYPE_BITMAP && pressed == NULL &&
      spec->u.bitmap.pressed_set > 0)
  {
    pressed = wuss_iconpool_bitmap(w, spec->u.bitmap.pressed_set - 1);
    if (pressed == NULL)
      return result_WUSS_BAD_INDEX;
  }

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
    const bitmap_t *state_bm;
    int             idx;
    int             gw, gh;
    int             i;

    gw = 0;
    gh = 0;
    for (i = 0; i < 2; i++)
    {
      idx = wuss_iconpool_lookup(w, wuss__icon_radio_option_name(spec->type, i));
      if (idx < 0)
        continue;
      state_bm = wuss_iconpool_bitmap(w, idx);
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
  case wuss_ICON_TYPE_DRAGGABLE:
    out->spec.u.bitmap.image        = bitmap;
    out->spec.u.bitmap.pressed_image = pressed;
    out->spec.u.bitmap.name         = NULL; /* borrowed; resolved above */
    out->spec.u.bitmap.pressed_name = NULL;
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
