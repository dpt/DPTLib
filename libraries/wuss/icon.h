/* wuss/icon.h -- work-area icons, internal */

#ifndef WUSS_ICON_IMPL_H
#define WUSS_ICON_IMPL_H

#include "geom/box.h"
#include "geom/point.h"

#include "framebuf/bmfont.h"
#include "framebuf/screen.h"

#include "wuss/wuss.h"
#include "wuss/icon.h"

/* Per-instance transient state, kept as bitflags with wuss__icon_* accessors
 * (mirrors wuss_window_state_t) so more can be added without growing the
 * struct. Not part of the public appearance API. */
typedef enum wuss_icon_state
{
  wuss_ICON_STATE_NONE     = 0,
  wuss_ICON_STATE_PRESSED  = 1 << 0, /* button: held with the pointer inside */
  wuss_ICON_STATE_SELECTED = 1 << 1, /* radio/option: latched on; menu entry:
                                      * draw a tick */
  wuss_ICON_STATE_HOVERED  = 1 << 2  /* menu entry: pointer is over it */
}
wuss_icon_state_t;

/* A live icon is its creation spec plus transient runtime state.
 * wuss__icon_from_spec validates a wuss_icon_spec_t and stores it in "spec"
 * with fg/bg/swatch resolved to concrete palette indices and, for a BITMAP,
 * u.bitmap.image resolved from u.bitmap.set. spec.text is owned (strdup'd by
 * wuss_icon_create, aliased by wuss_icon_plot) -- never NULL, "" instead. */
struct wuss_icon
{
  wuss_icon_spec_t  spec;    /* bbox in virtual document space; text owned */
  wuss_icon_state_t state;
  int               value;   /* wuss_ICON_TYPE_SLIDER: current value, in
                              * [spec.u.slider.min,max]; spec.u.slider.default_value
                              * is creation input only, never updated */
  int               text_scroll; /* wuss_ICON_TYPE_WRITABLE: pixels the text
                                  * is scrolled left to keep the caret in
                                  * view */
};

/* A writable's text sits this far inside its bbox: the 1px outline plus a
 * 3px gap. */
#define WUSS_WRITABLE_INSET 4

/* Per-edge widths (px) of the label borders. A bevel is one
 * screen_draw_bevel_edge ring; DIVIDER is two, ACTION three (outset, accent
 * moat, inset). */
#define WUSS_BEVEL_WIDTH          2
#define WUSS_PLAIN_BORDER_WIDTH   1
#define WUSS_DIVIDER_BORDER_WIDTH (2 * WUSS_BEVEL_WIDTH)
#define WUSS_ACTION_BORDER_WIDTH  (3 * WUSS_BEVEL_WIDTH)

/* Clear space (px) between a bordered label's border and its justified
 * text; an unbordered label's text gets WUSS_LABEL_TEXT_PAD instead. */
#define WUSS_LABEL_TEXT_GAP 2
#define WUSS_LABEL_TEXT_PAD 1

/* The font an icon's text is drawn with: its requested slot, falling back to
 * the system font. NULL when wuss has no fonts. */
bmfont_t *wuss__icon_font(const wuss_t *wuss, const wuss_icon_t *icon);

static inline int wuss__icon_pressed(const wuss_icon_t *icon)
{
  return (icon->state & wuss_ICON_STATE_PRESSED) != 0;
}

static inline int wuss__icon_selected(const wuss_icon_t *icon)
{
  return (icon->state & wuss_ICON_STATE_SELECTED) != 0;
}

static inline int wuss__icon_hovered(const wuss_icon_t *icon)
{
  return (icon->state & wuss_ICON_STATE_HOVERED) != 0;
}

static inline void wuss__icon_set_state(wuss_icon_t      *icon,
                                        wuss_icon_state_t bit,
                                        int               on)
{
  if (on)
    icon->state |= bit;
  else
    icon->state &= (wuss_icon_state_t) ~bit;
}

/* Set icon->selected, invalidating it. For a radio with a non-zero group,
 * selecting it also clears every other selected radio on "window" with that
 * group. Ignored for types with no latched state. "icon" must be an icon of
 * "window". */
void wuss__icon_select(wuss_window_t *window,
                       wuss_icon_t   *icon,
                       int            selected);

/* Set icon->value, clamped to spec.u.slider.min/max, invalidating it if the
 * clamped value changed. Ignored for types with no value. "icon" must be an
 * icon of "window". Shared by wuss_icon_set_value and the slider drag path
 * (core/mouse-click.c, core/mouse-move.c), which additionally raise
 * wuss_EVENT_ICON with the new value -- this helper never does. */
void wuss__icon_set_value(wuss_window_t *window,
                          wuss_icon_t   *icon,
                          int            value);

/* Make "icon" (an icon of "window", or NULL) the hovered icon: clears the
 * hovered flag on the previous wuss->hover_icon and sets it on the new one,
 * invalidating whichever of the two changed. "window" is ignored when "icon"
 * is NULL. A no-op if nothing changed. */
void wuss__icon_set_hover(wuss_t        *wuss,
                          wuss_window_t *window,
                          wuss_icon_t   *icon);

/* Map an icon bbox (virtual document space) into screen space:
 * screen = content.x0 - scroll.x + bbox. wuss__icon_draw paints through this
 * so hit-testing and invalidation cannot drift from what is drawn. */
void wuss__icon_box_to_screen(const box_t *content,
                              point_t      scroll,
                              const box_t *bbox,
                              box_t       *out);

/* wuss_ICON_TYPE_SLIDER geometry and value/pixel conversion, shared by
 * wuss__icon_draw and the click/drag path (core/mouse-click.c,
 * core/mouse-move.c) so hit-testing cannot drift from what is drawn.
 * "screen_box" is the icon's screen-space box, as wuss__icon_box_to_screen
 * gives it. */

/* The inner rect: "screen_box" (the icon's full bbox) inset by
 * WUSS_SLIDER_GAP on all four sides. This is both the groove drawn inside it
 * and the only area wuss__icon_hit_test accepts clicks/drags on; the gap and
 * the rest of "screen_box" are painted with slider.surround and never
 * interactive. */
void wuss__slider_groove_box(const box_t *screen_box, box_t *out);

/* Convert "value" (clamped to [lo,hi], lo <= hi) to a fill length in pixels
 * along "groove"'s long axis, 0 at lo and the full long-axis extent at hi.
 * For wuss_SLIDER_HORIZONTAL pixel 0 is the groove's left (x0); for VERTICAL
 * it is the groove's bottom (y1), so value grows upward on screen even
 * though screen y grows downward. */
int wuss__slider_value_to_px(const box_t              *groove,
                             wuss_slider_orientation_t orientation,
                             int                       value,
                             int                       lo,
                             int                       hi);

/* Convert a screen-space point's long-axis coordinate back to a value in
 * [lo,hi] (lo <= hi), inverting wuss__slider_value_to_px (same pixel-0 end
 * per orientation). */
int wuss__slider_px_to_value(const box_t              *groove,
                             wuss_slider_orientation_t orientation,
                             point_t                   screen_point,
                             int                       lo,
                             int                       hi);

/* "icon" (a slider icon of "window") maps a raw screen-space point (as passed
 * to wuss_mouse_click/wuss_mouse_move) to the value its groove would read at
 * that point, honouring a min > max reversed fill. Shared by the click-jump
 * and drag-continuation paths (core/mouse-click.c, core/mouse-move.c). */
int wuss__slider_value_for_point(wuss_window_t     *window,
                                 const wuss_icon_t *icon,
                                 point_t            screen_point);

/* Invalidate exactly this icon's bbox on "window", via wuss_window_invalidate,
 * so a set_text / pressed-state / hide change repaints just the icon. "icon"
 * must be an icon of "window". */
void wuss__icon_invalidate(wuss_window_t *window, const wuss_icon_t *icon);

/* Validate a spec against the palette and fill "out" with a detached icon:
 * "out->spec" is a copy of "spec" with fg/bg/swatch resolved to palette
 * indices and, for a BITMAP, u.bitmap.image resolved from u.bitmap.set.
 * out->spec.text is not owned -- it aliases spec->text, or "" when that is
 * NULL. Shared by wuss_icon_create and wuss_icon_plot. Returns
 * result_WUSS_BAD_ICON / result_WUSS_BAD_COLOUR as wuss_icon_create
 * documents, else result_OK. */
result_t wuss__icon_from_spec(const wuss_t           *wuss,
                              const wuss_icon_spec_t *spec,
                              wuss_icon_t            *out);

/* Draw one icon. Called from redraw_window with wuss->scr->clip already set to
 * the surviving content piece and the background already filled. "content" is
 * the window's full (unclipped) content box, screen space; "scroll" is
 * window->scroll. */
void wuss__icon_draw(wuss_t              *wuss,
                     const wuss_window_t *window,
                     const wuss_icon_t   *icon,
                     const box_t         *content,
                     point_t              scroll);

/* Hit-test every visible, enabled button icon of "window" against a point given
 * in virtual document space. Returns the topmost (last-created wins) match, or
 * NULL. Label, hidden and disabled icons are skipped. */
wuss_icon_t *wuss__icon_hit_test(wuss_window_t *window, point_t doc_point);

/* wuss_ICON_TYPE_WRITABLE editing (icon/writable.c). */

/* Move the caret to byte "index" (clamped to the text) of writable "icon" on
 * "window", scrolling the text to keep it in view and invalidating whichever
 * icons changed. Assumes "window" already holds the focus. */
void wuss__writable_place_caret(wuss_window_t *window,
                                wuss_icon_t   *icon,
                                int            index);

/* The caret index nearest document-space x "doc_x" in writable "icon". */
int wuss__writable_index_for_x(const wuss_t      *wuss,
                               const wuss_icon_t *icon,
                               int                doc_x);

/* Copy "text" into a writable's "size"-byte buffer "buf", truncating. */
void wuss__writable_copy(char *buf, int size, const char *text);

/* Remove the caret, invalidating its icon. No-op when there is none. */
void wuss__caret_clear(wuss_t *wuss);

/* Offer a key to the caret icon. Sets *claimed and returns the result of any
 * wuss_EVENT_ICON delivered; a key it does not use leaves *claimed 0. */
result_t wuss__writable_key(wuss_t              *wuss,
                            int                  code,
                            wuss_key_modifiers_t modifiers,
                            int                 *claimed);

/* Free a window's whole icon store (text + nodes + array). Teardown only: does
 * not invalidate or swap-remove. */
void wuss__icons_free(wuss_window_t *window);

/* Free the wuss-wide icon set loaded by wuss_icons_load (the atom_set and the
 * compressed bitmaps), leaving the fields NULL / 0. Safe on an unloaded set.
 * Called by wuss_icons_load before a reload and by wuss_destroy. */
void wuss__icons_registry_free(wuss_t *wuss);

#endif /* WUSS_ICON_IMPL_H */
