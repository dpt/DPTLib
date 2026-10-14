/* wuss/gadget/gridview.h -- a glyph+label grid on a caller's window */

/**
 * \file gridview.h
 *
 * A wuss gadget: a reflowing grid of glyph-over-label cells on a caller's
 * window, e.g. a file/resource picker. Unlike \ref wuss_stringset_t, this
 * gadget owns no icons -- it draws its cells itself, so the task must
 * forward wuss_EVENT_REDRAW, wuss_EVENT_MOUSE, wuss_EVENT_OPEN,
 * wuss_EVENT_PALETTE and wuss_EVENT_RELAYOUT to \ref
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
 * on wuss_EVENT_OPEN and at create; the gadget calls wuss_window_set_doc to
 * match, with a width reaching the widest layout the screen allows.
 * Hit-testing is arithmetic -- gaps between cells miss -- so no per-cell
 * icon exists to click.
 *
 * Selection follows RISC OS rules: Select on an unselected item selects only
 * it, and on a selected item changes nothing; Adjust toggles the item;
 * Select on empty space clears the selection and Adjust there changes
 * nothing. A double-click (wuss_BUTTON_DOUBLE) on an item deselects it, then
 * calls the activate callback.
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
#include "framebuf/colour.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

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
 * Activate callback: the user double-clicked item \p index. The callback may
 * close the gadget's window (and so destroy the gadget).
 *
 * \param[in] gridview The gadget.
 * \param[in] index    The activated item's index.
 * \param[in] button   The double-click's buttons: wuss_BUTTON_SELECT or
 *                     wuss_BUTTON_ADJUST, with wuss_BUTTON_DOUBLE.
 * \param[in] opaque   As passed to \ref wuss_gridview_create.
 */
typedef void (wuss_gridview_activate_fn_t)(wuss_gridview_t *gridview,
                                           int              index,
                                           wuss_button_t    button,
                                           void            *opaque);

/**
 * Optional cell draw callback: draws item \p index in place of the default
 * glyph-and-label. The screen's clip is already limited to the cell.
 *
 * \param[in] gridview   The gadget.
 * \param[in] event      The wuss_EVENT_REDRAW being handled.
 * \param[in] index      Item index.
 * \param[in] screen_box The cell's box in screen coordinates.
 * \param[in] opaque     As passed to \ref wuss_gridview_create.
 */
typedef void (wuss_gridview_draw_fn_t)(wuss_gridview_t    *gridview,
                                       const wuss_event_t *event,
                                       int                 index,
                                       const box_t        *screen_box,
                                       void               *opaque);

/**
 * Drag callback: the user pressed Select or Adjust on item \p index and
 * moved the pointer past the drag threshold.
 *
 * \param[in] gridview The grid view.
 * \param[in] index    The pressed item's index.
 * \param[in] opaque   As passed to \ref wuss_gridview_create.
 * \return Non-zero to start a core drag (see wuss_drag_start) sized to the
 *         cell, originating from the grid's window and holding the cell
 *         where it was pressed; the wuss_EVENT_DRAG_END then goes to the
 *         window's task. Zero to decline, so the item cannot be dragged.
 */
typedef int (wuss_gridview_drag_fn_t)(wuss_gridview_t *gridview,
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
 * Create a window and a grid view on it, ready for a browser-style task: a
 * 320x240 titled window with the default furniture less the horizontal
 * scrollbar (the grid reflows to the width instead), focusable, with no
 * backdrop. The window is not resized to suit the items; call \ref
 * wuss_gridview_fit_window once the task can route the window's events to
 * wuss_gridview_handle_event.
 *
 * \param[out] out         Filled with the new handle on success, untouched
 *                         on failure.
 * \param[in]  task        Task that owns the window.
 * \param[in]  place       Where to open the window, placed at its initial
 *                         320x240 size; NULL for the next spare region,
 *                         centred, as wuss_window_create_placed.
 * \param[in]  title       Titlebar label.
 * \param[in]  count       As \ref wuss_gridview_create.
 * \param[in]  item_fn     As \ref wuss_gridview_create.
 * \param[in]  label_chars As \ref wuss_gridview_create.
 * \param[in]  activate    As \ref wuss_gridview_create.
 * \param[in]  opaque      As \ref wuss_gridview_create.
 * \param[out] window      Filled with the new window on success.
 * \return \ref result_OK on success, or a result code from
 *         wuss_window_create_at or \ref wuss_gridview_create; no window is
 *         left behind on failure.
 */
result_t wuss_gridview_window_create(wuss_gridview_t            **out,
                                     wuss_task_t                 *task,
                                     const wuss_place_t          *place,
                                     const char                  *title,
                                     int                          count,
                                     wuss_gridview_item_fn_t     *item_fn,
                                     int                          label_chars,
                                     wuss_gridview_activate_fn_t *activate,
                                     void                        *opaque,
                                     wuss_window_t              **window);

/**
 * Resize the grid view's window to \ref wuss_gridview_natural_size, then, if
 * the screen cap stopped it part way across a column, to the columns that
 * fit. Resizing sends the window a wuss_EVENT_OPEN, which the task must
 * already be able to route to \ref wuss_gridview_handle_event.
 *
 * \param[in] gridview Handle.
 * \param[in] columns  Column count wanted; at least 1.
 * \param[in] max_rows Row limit; at least 1.
 */
void wuss_gridview_fit_window(wuss_gridview_t *gridview,
                              int              columns,
                              int              max_rows);

/**
 * Make items draggable: set the callback offered an item once it is pressed
 * and the pointer moves past wuss_config_t::mouse.drag_threshold_px. The
 * task must forward wuss_EVENT_MOUSE and wuss_EVENT_POINTER_EXIT to \ref
 * wuss_gridview_handle_event. None is set at create, so items start out not
 * draggable.
 *
 * \param[in] gridview Handle.
 * \param[in] drag     Callback, or NULL for none.
 */
void wuss_gridview_set_drag(wuss_gridview_t         *gridview,
                            wuss_gridview_drag_fn_t *drag);

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
 * Select every item, invalidating the cells that were not already selected.
 *
 * \param[in] gridview Handle.
 */
void wuss_gridview_select_all(wuss_gridview_t *gridview);

/**
 * Select only item \p index, invalidating the cells whose state changes. An
 * out-of-range index is ignored.
 *
 * \param[in] gridview Handle.
 * \param[in] index    Item index, 0..count-1.
 */
void wuss_gridview_select(wuss_gridview_t *gridview, int index);

/**
 * \param[in] gridview Handle.
 * \return Number of items selected.
 */
int wuss_gridview_selected_count(const wuss_gridview_t *gridview);

/**
 * Find the item under a point.
 *
 * \param[in] gridview Handle.
 * \param[in] point    Point in document coordinates.
 * \return Item index, or -1 over a gap, the inset or past the last item.
 */
int wuss_gridview_hit_test(const wuss_gridview_t *gridview, point_t point);

/**
 * Recompute item count after the underlying item list changes, keeping the
 * selection where indices still apply (a selected index at or beyond the new
 * count is dropped). Unless the cell size is fixed (see \ref
 * wuss_gridview_set_cell_size), the items are walked again to re-measure the
 * cell, so call this whenever the items change, even if their count does
 * not. Reflows and redraws.
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

/**
 * Re-measure the cell after the window's font or palette changed, then
 * reflow and redraw. The items are only walked again if the font or its
 * height differs from the last measure; for changes to the items, call \ref
 * wuss_gridview_set_count. Called by \ref wuss_gridview_handle_event on
 * wuss_EVENT_PALETTE and wuss_EVENT_RELAYOUT.
 *
 * \param[in] gridview Handle.
 */
void wuss_gridview_remeasure(wuss_gridview_t *gridview);

/**
 * Set the column policy. The defaults are no limit, any count and no inset.
 *
 * \param[in] gridview    Handle.
 * \param[in] max_columns Most columns to use, whatever the item count; 0 for
 *                        as many as there are items.
 * \param[in] pow2        Non-zero to restrict columns to 1, 2, 4, 8...
 * \param[in] inset       Margin in pixels round the whole grid.
 */
void wuss_gridview_set_layout(wuss_gridview_t *gridview,
                              int              max_columns,
                              int              pow2,
                              int              inset);

/**
 * Set the padding inside each cell. The defaults are \ref wuss_STD_GAP for
 * all three. Has no effect on a cell size set with \ref
 * wuss_gridview_set_cell_size, other than where the glyph and label sit.
 * Re-measures the cell, reflows and redraws.
 *
 * \param[in] gridview Handle.
 * \param[in] pad_x    Pixels between the cell's left and right edges and its
 *                     contents.
 * \param[in] pad_y    Pixels between the cell's top and bottom edges and its
 *                     contents.
 * \param[in] spacing  Pixels between the glyph and the label.
 */
void wuss_gridview_set_padding(wuss_gridview_t *gridview,
                               int              pad_x,
                               int              pad_y,
                               int              spacing);

/**
 * Fix the cell size, replacing the measured one. Later calls to \ref
 * wuss_gridview_remeasure keep it; call this again to change it.
 *
 * \param[in] gridview Handle.
 * \param[in] cell     Cell size in pixels; each side at least 1.
 */
void wuss_gridview_set_cell_size(wuss_gridview_t *gridview, size2d_t cell);

/**
 * Set the colour the content is cleared to before the cells are drawn,
 * replacing the window background colour.
 *
 * \param[in] gridview Handle.
 * \param[in] colour   Colour to clear to.
 */
void wuss_gridview_set_background(wuss_gridview_t *gridview,
                                  colour_t         colour);

/**
 * Set a custom cell draw callback and whether cells can be selected.
 *
 * \param[in] gridview   Handle.
 * \param[in] draw       Cell draw callback, or NULL for glyph and label.
 * \param[in] selectable Non-zero if clicks select cells.
 */
void wuss_gridview_set_draw(wuss_gridview_t         *gridview,
                            wuss_gridview_draw_fn_t *draw,
                            int                      selectable);

/**
 * \param[in] gridview Handle.
 * \return Current column count; 0 before the first layout.
 */
int wuss_gridview_get_columns(const wuss_gridview_t *gridview);

/**
 * \param[in] gridview Handle.
 * \param[in] index    Item index.
 * \return Item \p index's box in document coordinates.
 */
box_t wuss_gridview_cell_box(const wuss_gridview_t *gridview, int index);

/**
 * \param[in] gridview Handle.
 * \param[in] columns  Column count; at least 1.
 * \return Content width that shows exactly \p columns columns.
 */
int wuss_gridview_width_for_columns(const wuss_gridview_t *gridview,
                                    int                    columns);

/**
 * Content size that suits opening the window: \p columns wide (or one column
 * per item if fewer), as many rows as the items need up to \p max_rows.
 * Never narrower than the window's minimum: one cell wide enough for a
 * ten-character label (or \p label_chars, if fewer), unless the cell size
 * was fixed by \ref wuss_gridview_set_cell_size.
 *
 * \param[in] gridview Handle.
 * \param[in] columns  Column count; at least 1.
 * \param[in] max_rows Row limit; at least 1.
 * \return The content size.
 */
size2d_t wuss_gridview_natural_size(const wuss_gridview_t *gridview,
                                    int                    columns,
                                    int                    max_rows);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_GADGET_GRIDVIEW_H */
