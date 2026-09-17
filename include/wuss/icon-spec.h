/* wuss/icon-spec.h -- fill helpers for common wuss_icon_spec_t shapes */

/**
 * \file icon-spec.h
 *
 * A task laying out a dialogue with wuss_icon_create_array typically builds
 * several wuss_icon_spec_t entries that only differ in bbox, text and a
 * couple of type-specific fields. These helpers fill one spec in place for
 * the three shapes that recur across the test tasks: a plain or
 * right-justified label, a horizontal or vertical slider, and an action
 * button (optionally the dialogue's default action). Each takes the spec to
 * fill plus its distinguishing fields; the caller is still responsible for
 * memset(specs, 0, sizeof(specs)) up front and for wuss_icon_create_array.
 */

#ifndef WUSS_ICON_SPEC_H
#define WUSS_ICON_SPEC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"

#include "wuss/icon.h"
#include "wuss/window.h"

#ifdef WUSS_ICONS

/* ----------------------------------------------------------------------- */

/**
 * A slider paired with a label that echoes its current value, the recurring
 * "Label: [slider] value" row seen in dialogues across the test tasks (see
 * saturn.c's size dialogue and icons.c's Sliders group). The value label's
 * text is formatted by \c fmt (a single printf-style "%d" conversion; NULL
 * defaults to "%d") every time \ref wuss_slider_row_set or \ref
 * wuss_slider_row_event changes the value, so callers never format it by
 * hand. If \c step is non-zero, both functions also snap the value to the
 * nearest multiple of \c step above \c min, so callers never hand-roll that
 * either.
 */
typedef struct wuss_slider_row
{
  /** The slider icon. Not owned; destroyed with its window as normal. */
  wuss_icon_t *slider;
  /** The label icon echoing the slider's value. Not owned. */
  wuss_icon_t *value;
  /** printf-style format for the value label, one "%d" conversion.
   *  Borrowed; must outlive the row (a string literal in practice). NULL
   *  means "%d". */
  const char  *fmt;
  /** Value at the slider's groove start and end; snapping rounds to a
   *  multiple of \c step away from \c min and clamps to [min,max]. Match
   *  the \c min/\c max passed to \ref wuss_icon_spec_slider_row. */
  int          min, max;
  /** Snapping step; 0 means "no snapping" (any value is kept as-is). */
  int          step;
}
wuss_slider_row_t;

/**
 * Fill \p slider_spec and \p value_spec as a wuss_ICON_TYPE_SLIDER and its
 * paired wuss_ICON_TYPE_LABEL value echo, ready for \ref
 * wuss_icon_create_array alongside the row's own text label (built
 * separately with \ref wuss_icon_spec_label). Call \ref wuss_slider_row_bind
 * on the two created icons afterwards to get a usable \ref
 * wuss_slider_row_t.
 *
 * Drawn with wuss_COLOUR_BLACK groove/fill and text.
 *
 * \param[out] slider_spec   Spec to fill as the slider; overwritten.
 * \param[out] value_spec    Spec to fill as the value label; overwritten.
 * \param[in]  slider_bbox   Slider's bounding box.
 * \param[in]  value_bbox    Value label's bounding box.
 * \param[in]  orientation   Groove direction.
 * \param[in]  min           Value at the groove's start.
 * \param[in]  max           Value at the groove's end.
 * \param[in]  default_value Initial value, clamped to [min,max] and (if \p
 *                           step is non-zero) snapped to a multiple of it.
 * \param[in]  fmt           printf-style format for the value label, one
 *                           "%d" conversion; NULL for "%d". Borrowed; must
 *                           outlive the row.
 * \param[in]  step          Snapping step passed through to \ref
 *                           wuss_slider_row_bind; 0 for none.
 */
void wuss_icon_spec_slider_row(wuss_icon_spec_t         *slider_spec,
                               wuss_icon_spec_t         *value_spec,
                               box_t                     slider_bbox,
                               box_t                     value_bbox,
                               wuss_slider_orientation_t orientation,
                               int                       min,
                               int                       max,
                               int                       default_value,
                               const char               *fmt,
                               int                       step);

/**
 * Bind a \ref wuss_slider_row_t to the icons \ref wuss_icon_create_array
 * made from a pair of specs filled by \ref wuss_icon_spec_slider_row.
 *
 * \param[out] row    Row to fill.
 * \param[in]  slider The created slider icon.
 * \param[in]  value  The created value-label icon.
 * \param[in]  fmt    Same format string passed to \ref
 *                    wuss_icon_spec_slider_row; borrowed, must outlive the
 *                    row. NULL for "%d".
 * \param[in]  min    Same \c min passed to \ref wuss_icon_spec_slider_row.
 * \param[in]  max    Same \c max passed to \ref wuss_icon_spec_slider_row.
 * \param[in]  step   Same \c step passed to \ref wuss_icon_spec_slider_row;
 *                    0 for no snapping.
 */
void wuss_slider_row_bind(wuss_slider_row_t *row,
                          wuss_icon_t       *slider,
                          wuss_icon_t       *value,
                          const char        *fmt,
                          int                min,
                          int                max,
                          int                step);

/**
 * Set a slider row's value programmatically -- the slider fill and its
 * formatted value label both update, invalidating each so the next redraw
 * repaints them. No task event is delivered, matching \ref
 * wuss_icon_set_value.
 *
 * \param[in] window Window the row's icons belong to.
 * \param[in] row    Row to change.
 * \param[in] value  New value; snapped to \c row->step (if non-zero) and
 *                   clamped to [row->min,row->max].
 * \return \ref result_OK, or \ref result_OOM from formatting the label.
 */
result_t wuss_slider_row_set(wuss_window_t           *window,
                             const wuss_slider_row_t *row,
                             int                      value);

/**
 * Handle a wuss_EVENT_ICON delivered to the row's window: if it names this
 * row's slider and is a drag (MOUSE_DOWN or MOUSE_MOVE), snaps the value to
 * \c row->step (if non-zero, re-setting the slider itself so it visibly
 * jumps to the snapped position), reformats the value label to match, and
 * reports the new value. Any other icon or action is left untouched.
 *
 * \param[in]  window Window the row's icons belong to.
 * \param[in]  row    Row to check against.
 * \param[in]  event  The wuss_EVENT_ICON event.
 * \param[out] value  Set to the new (already-snapped) value when handled.
 *                    May be NULL.
 * \return Non-zero if \p event was a drag on this row's slider (and so was
 *         handled), zero otherwise.
 */
int wuss_slider_row_event(wuss_window_t           *window,
                          const wuss_slider_row_t *row,
                          const wuss_event_t      *event,
                          int                     *value);

/**
 * Fill \p spec as a wuss_ICON_TYPE_LABEL.
 *
 * Drawn with wuss_COLOUR_BLACK text.
 *
 * \param[out] spec        Spec to fill; any prior contents are overwritten.
 * \param[in]  bbox         Bounding box.
 * \param[in]  text         Label text, borrowed until
 *                          wuss_icon_create(_array) copies it.
 * \param[in]  justify_right Non-zero to right-justify
 *                           (wuss_ICON_FLAGS_JUSTIFY_RIGHT) instead of the
 *                           default left justification.
 */
void wuss_icon_spec_label(wuss_icon_spec_t *spec,
                          box_t             bbox,
                          const char       *text,
                          int               justify_right);

/**
 * Fill \p spec as a wuss_ICON_TYPE_SLIDER.
 *
 * Drawn with wuss_COLOUR_BLACK groove/fill.
 *
 * \param[out] spec          Spec to fill; any prior contents are
 *                           overwritten.
 * \param[in]  bbox          Bounding box.
 * \param[in]  orientation   Groove direction.
 * \param[in]  min           Value at the groove's start.
 * \param[in]  max           Value at the groove's end.
 * \param[in]  default_value Initial value, clamped to [min,max].
 */
void wuss_icon_spec_slider(wuss_icon_spec_t         *spec,
                           box_t                     bbox,
                           wuss_slider_orientation_t orientation,
                           int                       min,
                           int                       max,
                           int                       default_value);

/**
 * Fill \p spec as a wuss_ICON_TYPE_ACTION button.
 *
 * Drawn with wuss_COLOUR_BLACK text on a wuss_COLOUR_WINDOW fill.
 *
 * \param[out] spec    Spec to fill; any prior contents are overwritten.
 * \param[in]  bbox    Bounding box.
 * \param[in]  text    Button text, borrowed until wuss_icon_create(_array)
 *                     copies it.
 * \param[in]  is_default Non-zero to draw as the dialogue's default action
 *                        (wuss_ICON_FLAGS_DEFAULT) instead of an ordinary
 *                        button.
 */
void wuss_icon_spec_action(wuss_icon_spec_t *spec,
                           box_t             bbox,
                           const char       *text,
                           int               is_default);

#endif /* WUSS_ICONS */

#ifdef __cplusplus
}
#endif

#endif /* WUSS_ICON_SPEC_H */
