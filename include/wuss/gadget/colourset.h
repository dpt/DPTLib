/* wuss/gadget/colourset.h -- a titled colour well with a colour menu */

/**
 * \file colourset.h
 *
 * A wuss gadget: a title, a sunken field filled with the chosen colour, and
 * a pop-up button at its right edge that opens the shared colour menu (see
 * wuss/component/colourmenu.h) -- the colour sibling of \ref stringset.h.
 *
 * The gadget is three icons on the caller's window: the title (see \ref
 * wuss_icon_spec_label), the field (a grooved label filled with the colour)
 * and the button, drawn from the "grightc" / "grightc-p" icon-set entries,
 * or as an ACTION button labelled ">" when the icon set lacks them. The
 * icons belong to the window and are freed with it.
 *
 * A Select or Adjust click on the button opens the colour menu beside it,
 * retitled to the gadget's title, its None row hidden and the current colour
 * ticked. The task forwards its wuss_EVENT_ICON, wuss_EVENT_MENU_SELECT and
 * wuss_EVENT_MENU_CLOSED events to \ref wuss_colourset_handle_event; a pick
 * refills the field and calls the changed callback.
 *
 * Built only when WUSS_COMPONENTS is defined, since it borrows the colour
 * menu component.
 */

#ifndef WUSS_GADGET_COLOURSET_H
#define WUSS_GADGET_COLOURSET_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"

#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Opaque handle: the gadget's state. */
typedef struct wuss_colourset wuss_colourset_t;

/**
 * Changed callback: the user picked a different colour. Called by \ref
 * wuss_colourset_handle_event after the field is refilled. Not called by
 * \ref wuss_colourset_set_colour.
 *
 * \param[in] colourset The gadget.
 * \param[in] colour    The new colour, a concrete palette index.
 * \param[in] opaque    As passed to \ref wuss_colourset_create.
 * \return \ref result_OK, or an appropriate result code; propagated back
 *         through \ref wuss_colourset_handle_event.
 */
typedef result_t (wuss_colourset_changed_fn_t)(wuss_colourset_t *colourset,
                                               wuss_colour_t     colour,
                                               void             *opaque);

/* ----------------------------------------------------------------------- */

/**
 * Create a colour set on \p window.
 *
 * \param[out] out     Filled with the new handle on success, untouched on
 *                     failure.
 * \param[in]  window  Window to put the gadget's icons on.
 * \param[in]  bbox    Bounding box, virtual document space, for the title,
 *                     field and button together. The title takes the left
 *                     edge, \p title_width wide; the button takes the right
 *                     edge, vertically centred; the field fills the rest,
 *                     less wuss_STD_GAP either side.
 * \param[in]  title   Title text, also the colour menu's title; NULL or ""
 *                     for no title icon (the menu is then titled "Colour").
 *                     Borrowed; must outlive the gadget.
 * \param[in]  title_width Width of the title, so a column of gadgets can
 *                         line their fields up; 0 to fit its text.
 * \param[in]  colour  Initial colour: a palette index or a symbolic
 *                     wuss_COLOUR_* value, resolved now.
 * \param[in]  changed Called when the user picks a different colour, or NULL
 *                     for none.
 * \param[in]  opaque  Passed back to \p changed.
 * \return \ref result_OK on success, \ref result_NULL_ARG if \p out or \p
 *         window is NULL, \ref result_WUSS_BAD_COLOUR if \p colour is out of
 *         range for the palette, \ref result_OOM, or a wuss_icon_create
 *         code.
 */
result_t wuss_colourset_create(wuss_colourset_t           **out,
                               wuss_window_t               *window,
                               box_t                        bbox,
                               const char                  *title,
                               int                          title_width,
                               wuss_colour_t                colour,
                               wuss_colourset_changed_fn_t *changed,
                               void                        *opaque);

/**
 * The width a colour set's button takes at the right of its bounding box, so
 * a caller can line the field's right edge up with other icons.
 *
 * \param[in] wuss   The wuss instance, for its icon set.
 * \param[in] height Height of the gadget's bounding box; the button is
 *                   square at this height when the icon set lacks "grightc".
 * \return The button's width in pixels.
 */
int wuss_colourset_button_width(const wuss_t *wuss, int height);

/**
 * Free a colour set, closing its menu if open. Its icons are left on the
 * window, so this is safe to call before or after the window closes. Safe to
 * pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_colourset_destroy(wuss_colourset_t *doomed);

/**
 * Offer an event from the task's own handler: a wuss_EVENT_ICON on the
 * gadget's button, or a wuss_EVENT_MENU_SELECT from the colour menu while
 * the gadget opened it. An Adjust pick keeps the menu open with the tick
 * moved. A wuss_EVENT_MENU_CLOSED must be offered too, so the gadget drops
 * its handle on the freed chain; it is noted but never consumed.
 *
 * \param[in]  colourset  Handle.
 * \param[in]  event      The event, of any kind.
 * \param[out] out_result Filled with the outcome -- the changed callback's
 *                        result, or an error from opening the menu -- if
 *                        (and only if) this call returns 1.
 * \return 1 if the event was the gadget's and was consumed, else 0.
 */
int wuss_colourset_handle_event(wuss_colourset_t   *colourset,
                                const wuss_event_t *event,
                                result_t           *out_result);

/**
 * Fetch the current colour.
 *
 * \param[in] colourset Handle.
 * \return The colour, a concrete palette index.
 */
wuss_colour_t wuss_colourset_get_colour(const wuss_colourset_t *colourset);

/**
 * Show colour \p colour. The changed callback is not called -- this is the
 * programmatic path, distinct from a user pick.
 *
 * \param[in] colourset Handle.
 * \param[in] colour    A palette index or a symbolic wuss_COLOUR_* value,
 *                      resolved now.
 * \return \ref result_OK, or \ref result_WUSS_BAD_COLOUR if \p colour is out
 *         of range for the palette (the field keeps its old colour).
 */
result_t wuss_colourset_set_colour(wuss_colourset_t *colourset,
                                   wuss_colour_t     colour);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_GADGET_COLOURSET_H */
