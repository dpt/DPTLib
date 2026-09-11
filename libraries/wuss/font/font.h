/* wuss/font/font.h -- wuss font registry and centralised text rendering */

#ifndef WUSS_FONT_H
#define WUSS_FONT_H

#include "geom/point.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"

#include "base/result.h"

#include "wuss/wuss.h"

/* The set of fonts a wuss_t was created with: slot 0 is the system font used
 * for window titlebars and offered to tasks via wuss_get_font; further slots
 * hold optional bold/symbol faces. Populated once by wuss__fontset_init from
 * wuss_create's argument and never changed after. Fonts are borrowed, not
 * owned. */
struct wuss_fontset
{
  /* unset slots NULL */
  bmfont_t         *fonts[wuss_MAX_FONTS];
  /* parallel to fonts[]; wuss_FONT_CLASS_NONE for an unset slot */
  wuss_font_class_t font_classes[wuss_MAX_FONTS];
  /* parallel to fonts[]; borrowed; NULL if unset or given no name */
  const char       *font_names[wuss_MAX_FONTS];
  /* slots filled from the wuss_create argument */
  int               nfonts;
};

/* Copy the first "nfonts" descriptors into "set" and NULL the rest. "nfonts"
 * has already been range-checked by the caller (0..wuss_MAX_FONTS). */
void wuss__fontset_init(struct wuss_fontset     *set,
                        const wuss_font_desc_t  *fonts,
                        int                      nfonts);

/* Pixel height of the font in "slot", or 0 if that slot is empty or out of
 * range. Used by wuss_create to size the titlebar to the tallest of the
 * regular and bold faces. */
int wuss__fontset_height(const struct wuss_fontset *set, int slot);

/* Centralised text rendering. wuss__text_draw is a plain pass-through to
 * bmfont_draw (pos/end_pos are baseline positions, per bmfont_draw's
 * contract), kept alongside wuss__text_measure for a single point of
 * policy. */
result_t wuss__text_measure(bmfont_t       *font,
                            const char     *text,
                            int             len,
                            bmfont_width_t  target_width,
                            int            *split_point,
                            bmfont_width_t *actual_width);
result_t wuss__text_draw(bmfont_t      *font,
                         screen_t      *scr,
                         const char    *text,
                         int            len,
                         colour_t       fg,
                         colour_t       bg,
                         const point_t *pos,
                         point_t       *end_pos);

#endif /* WUSS_FONT_H */
