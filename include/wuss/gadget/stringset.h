/* wuss/gadget/stringset.h -- a field with a pop-up menu of fixed strings */

/**
 * \file stringset.h
 *
 * A wuss gadget: a sunken display field showing one of a fixed set of
 * strings, with a pop-up arrow button at its right edge that opens a menu of
 * them -- the RISC OS Toolbox StringSet in miniature.
 *
 * The gadget is two icons on the caller's window: the field (see \ref
 * wuss_icon_spec_display) and the arrow, drawn from the "gright" /
 * "gright-p" icon-set entries, or as an ACTION button labelled ">" when the
 * icon set lacks them. The icons belong to the window and are freed with it.
 *
 * A Select or Adjust click on the arrow opens the menu beside it, the
 * current entry ticked. The task forwards its wuss_EVENT_ICON and
 * wuss_EVENT_MENU_SELECT events to \ref wuss_stringset_handle_event; a pick
 * updates the field and calls the changed callback.
 *
 * Built only when WUSS_GADGETS is defined (which implies WUSS_MENUS).
 */

#ifndef WUSS_GADGET_STRINGSET_H
#define WUSS_GADGET_STRINGSET_H

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

/** Opaque handle: the gadget's state and menu. */
typedef struct wuss_stringset wuss_stringset_t;

/**
 * Changed callback: the user picked a different entry. Called by \ref
 * wuss_stringset_handle_event after the field is updated. Not called by \ref
 * wuss_stringset_set_index.
 *
 * \param[in] stringset The gadget.
 * \param[in] index     The new entry's index.
 * \param[in] opaque    As passed to \ref wuss_stringset_create.
 * \return \ref result_OK, or an appropriate result code; propagated back
 *         through \ref wuss_stringset_handle_event.
 */
typedef result_t (wuss_stringset_changed_fn_t)(wuss_stringset_t *stringset,
                                               int               index,
                                               void             *opaque);

/* ----------------------------------------------------------------------- */

/**
 * Create a string set on \p window, showing entry 0.
 *
 * \param[out] out     Filled with the new handle on success, untouched on
 *                     failure.
 * \param[in]  window  Window to put the gadget's icons on.
 * \param[in]  bbox    Bounding box, virtual document space, for the field
 *                     and arrow together. The arrow takes the right edge,
 *                     vertically centred; the field fills the rest, less
 *                     wuss_STD_GAP.
 * \param[in]  title   Menu title; NULL means "". Borrowed; must outlive the
 *                     gadget.
 * \param[in]  strings Array of \p count entries. Borrowed; the array and its
 *                     strings must outlive the gadget.
 * \param[in]  count   Number of entries; at least 1.
 * \param[in]  changed Called when the user picks a different entry, or NULL
 *                     for none.
 * \param[in]  opaque  Passed back to \p changed.
 * \return \ref result_OK on success, \ref result_NULL_ARG if \p out, \p
 *         window or \p strings is NULL, \ref result_BAD_ARG if \p count is
 *         less than 1, \ref result_OOM, or a wuss_icon_create code.
 */
result_t wuss_stringset_create(wuss_stringset_t           **out,
                               wuss_window_t               *window,
                               box_t                        bbox,
                               const char                  *title,
                               const char *const           *strings,
                               int                          count,
                               wuss_stringset_changed_fn_t *changed,
                               void                        *opaque);

/**
 * Free a string set, closing its menu if open. Its icons are left on the
 * window, so this is safe to call before or after the window closes. Safe to
 * pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_stringset_destroy(wuss_stringset_t *doomed);

/**
 * Offer an event from the task's own handler: a wuss_EVENT_ICON on the
 * gadget's arrow, or a wuss_EVENT_MENU_SELECT from its menu. An Adjust pick
 * keeps the menu open with the tick moved.
 *
 * \param[in]  stringset  Handle.
 * \param[in]  event      The event, of any kind.
 * \param[out] out_result Filled with the outcome -- the changed callback's
 *                        result, or an error from opening the menu or
 *                        updating the field -- if (and only if) this call
 *                        returns 1.
 * \return 1 if the event was the gadget's and was consumed, else 0.
 */
int wuss_stringset_handle_event(wuss_stringset_t   *stringset,
                                const wuss_event_t *event,
                                result_t           *out_result);

/**
 * Fetch the current entry's index.
 *
 * \param[in] stringset Handle.
 * \return The index, 0..count-1.
 */
int wuss_stringset_get_index(const wuss_stringset_t *stringset);

/**
 * Show entry \p index. The changed callback is not called -- this is the
 * programmatic path, distinct from a user pick.
 *
 * \param[in] stringset Handle.
 * \param[in] index     Entry to show, 0..count-1.
 * \return \ref result_OK, \ref result_BAD_ARG if \p index is out of range,
 *         or \ref result_OOM (the field keeps its old text).
 */
result_t wuss_stringset_set_index(wuss_stringset_t *stringset, int index);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_GADGET_STRINGSET_H */
