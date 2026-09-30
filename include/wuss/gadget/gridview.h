/* wuss/gadget/gridview.h -- a glyph+label grid on a caller's window */

/**
 * \file gridview.h
 *
 * A wuss gadget: a reflowing grid of glyph-over-label cells on a caller's
 * window, e.g. a file/resource picker. Unlike \ref wuss_stringset_t, this
 * gadget owns no icons -- it draws its cells itself, so the task must
 * forward wuss_EVENT_REDRAW, wuss_EVENT_MOUSE and wuss_EVENT_OPEN to \ref
 * wuss_gridview_handle_event.
 *
 * Items come from a count plus an \ref wuss_gridview_item_fn_t callback
 * called on demand; the gadget holds no per-item state beyond the selection.
 * Cell size is fixed at creation: the largest glyph combined with a fixed
 * character width of label text in the window's font, so one long name
 * cannot inflate the grid. A label too wide for its cell is truncated with
 * an ellipsis.
 *
 * Column count follows the window's visible content width and is recomputed
 * on wuss_EVENT_OPEN; the gadget calls wuss_window_set_doc to match.
 * Hit-testing is arithmetic -- gaps between cells miss -- so no per-cell
 * icon exists to click.
 *
 * Selection follows RISC OS rules: Select selects only the clicked item,
 * Adjust toggles it, a click on empty space clears the selection. A
 * double-click (wuss_BUTTON_DOUBLE) on an item calls the activate callback.
 *
 * Built only when WUSS_GADGETS is defined.
 */

#ifndef WUSS_GADGET_GRIDVIEW_H
#define WUSS_GADGET_GRIDVIEW_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bitmap.h"

#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Opaque handle: the gadget's state and selection. */
typedef struct wuss_gridview wuss_gridview_t;

/**
 * One item's appearance, filled by \ref wuss_gridview_item_fn_t.
 */
typedef struct wuss_gridview_item
{
  const bitmap_t *glyph; /**< Drawn centred above the label; NULL for
                          *   none. */
  const char      *label; /**< Drawn centred below the glyph; NULL for none.
                           *   Borrowed -- must stay valid until the next
                           *   call into the gadget. */
}
wuss_gridview_item_t;

/**
 * Item callback: fills \p out with item \p index's appearance.
 *
 * \param[in]  index  Item index, 0..count-1.
 * \param[in]  opaque As passed to \ref wuss_gridview_create.
 * \param[out] out    Filled with the item's glyph and label.
 */
typedef void (wuss_gridview_item_fn_t)(int                    index,
                                       void                  *opaque,
                                       wuss_gridview_item_t  *out);

/**
 * Activate callback: the user double-clicked item \p index.
 *
 * \param[in] gridview The gadget.
 * \param[in] index    The activated item's index.
 * \param[in] opaque   As passed to \ref wuss_gridview_create.
 */
typedef void (wuss_gridview_activate_fn_t)(wuss_gridview_t *gridview,
                                           int              index,
                                           void            *opaque);

/* ----------------------------------------------------------------------- */

/**
 * Create a grid view on \p window, showing \p count items with none
 * selected.
 *
 * \param[out] out          Filled with the new handle on success, untouched
 *                          on failure.
 * \param[in]  window       Window to draw the grid on.
 * \param[in]  count        Number of items; at least 0.
 * \param[in]  item_fn      Called on demand for each item's appearance.
 * \param[in]  label_chars  Label column width, in characters of the window's
 *                          font; at least 1.
 * \param[in]  activate     Called on a double-click on an item, or NULL for
 *                          none.
 * \param[in]  opaque       Passed back to \p item_fn and \p activate.
 * \return \ref result_OK on success, \ref result_NULL_ARG if \p out, \p
 *         window or \p item_fn is NULL, \ref result_BAD_ARG if \p count is
 *         negative or \p label_chars is less than 1, or \ref result_OOM.
 */
result_t wuss_gridview_create(wuss_gridview_t            **out,
                              wuss_window_t               *window,
                              int                          count,
                              wuss_gridview_item_fn_t     *item_fn,
                              int                          label_chars,
                              wuss_gridview_activate_fn_t *activate,
                              void                        *opaque);

/**
 * Free a grid view. Safe to pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_gridview_destroy(wuss_gridview_t *doomed);

/**
 * Offer an event from the task's own handler: a wuss_EVENT_REDRAW,
 * wuss_EVENT_MOUSE or wuss_EVENT_OPEN on the gadget's window.
 * wuss_EVENT_OPEN reflows the grid's columns to the window's current visible
 * width and updates its document extent -- offer it whenever the window's
 * width may have changed, not only on the first open.
 *
 * \param[in] gridview Handle.
 * \param[in] event    The event, of any kind.
 * \return 1 if the event was the gadget's and was consumed, else 0. A REDRAW
 *         is consumed only when it targets this gadget's window; a MOUSE
 *         DOWN outside every cell is consumed too, since it clears the
 *         selection.
 */
int wuss_gridview_handle_event(wuss_gridview_t    *gridview,
                               const wuss_event_t *event);

/**
 * Fetch item \p index's selected state.
 *
 * \param[in] gridview Handle.
 * \param[in] index    Item index, 0..count-1.
 * \return Non-zero if selected.
 */
int wuss_gridview_is_selected(const wuss_gridview_t *gridview, int index);

/**
 * Clear the selection, invalidating every previously-selected cell. Does
 * nothing if nothing is selected.
 *
 * \param[in] gridview Handle.
 */
void wuss_gridview_clear_selection(wuss_gridview_t *gridview);

/**
 * Recompute item count after the underlying item list changes, keeping the
 * selection where indices still apply (a selected index at or beyond the new
 * count is dropped). Reflows and redraws.
 *
 * \param[in] gridview Handle.
 * \param[in] count    New item count; at least 0.
 * \return \ref result_OK, or \ref result_BAD_ARG if \p count is negative.
 */
result_t wuss_gridview_set_count(wuss_gridview_t *gridview, int count);

/**
 * Fetch the cell size fixed at \ref wuss_gridview_create, e.g. for a caller
 * sizing its window to fit a known item count before the first OPEN.
 *
 * \param[in] gridview Handle.
 * \return The glyph+label cell size, including padding.
 */
size2d_t wuss_gridview_get_cell_size(const wuss_gridview_t *gridview);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_GADGET_GRIDVIEW_H */
