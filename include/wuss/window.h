/* wuss/window.h -- wuss window API */

/**
 * \file window.h
 *
 * A Wuss window: creation, destruction, positioning, sizing and task
 * delegation of content drawing and mouse handling.
 */

#ifndef WUSS_WINDOW_H
#define WUSS_WINDOW_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "framebuf/screen.h"

#include "wuss/task.h"
#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/**
 * Create a window.
 *
 * Furniture (titlebar/outline) is added outside \p content, not carved out
 * of it: before clamping, the window's content area is exactly \p content,
 * and its on-screen footprint (see wuss_window_get_visible_bounds) is \p
 * content expanded outward by whatever furniture flags request. If that
 * footprint would then fall off the top or left edge of the screen, the
 * window (content included) is nudged right/down just enough to bring it
 * flush with the edge, so the titlebar/close icon stay reachable; a window
 * wider or taller than the screen keeps its top-left corner on-screen
 * instead. The bottom/right edges are not clamped.
 *
 * \param[in]  task    Owning task; the window is created on task's window
 *                     manager and delivers all its events to task's handle.
 *                     Immutable once set.
 * \param[in]  content Requested content-area bounds, screen space. Copied
 *                     in.
 * \param[in]  title   Titlebar label, or NULL for none. Copied in, truncated
 *                     if too long. Ignored if flags includes
 *                     wuss_WINDOW_NO_TITLEBAR.
 * \param[in]  flags   Appearance flags, e.g. wuss_WINDOW_NO_TITLEBAR /
 *                     wuss_WINDOW_NO_OUTLINE, OR'd together, or
 *                     wuss_WINDOW_NONE for the default furniture.
 * \param[in]  bg      Content background, filled by Wuss before each redraw:
 *                     a flat colour or an 8x8 fill pattern (see
 *                     wuss_backdrop_t). Set its colour to wuss_NO_BACKGROUND
 *                     for the task to draw its own background (avoids a
 *                     redundant fill behind an opaque task). Any pattern is
 *                     phased to the window's scroll origin so it stays
 *                     locked to the content. Changeable later via
 *                     wuss_window_set_background.
 * \param[in]  doc     Virtual document extent, for the scrollbars' sausage
 *                     proportions; pass content's own width and height for a
 *                     window with nothing to scroll. Also the ceiling a
 *                     resize-drag or toggle-size will grow the content area
 *                     to.
 * \param[in]  min_doc Minimum content size a resize-drag or toggle-size will
 *                     shrink to. Pass (0,0) for the built-in floor. Clamped
 *                     to doc, and to the built-in floor, so it can never
 *                     make a window unusably small or larger than its
 *                     document.
 * \param[out] window  Newly created window. Becomes the topmost window.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if content's
 *         width or height is not positive, \ref result_WUSS_BAD_COLOUR if
 *         any of bg's colours are out of range for the palette, or another
 *         appropriate result code.
 */
result_t wuss_window_create(wuss_task_t        *task,
                            const box_t        *content,
                            const char         *title,
                            wuss_window_flags_t flags,
                            wuss_backdrop_t     bg,
                            size2d_t            doc,
                            size2d_t            min_doc,
                            wuss_window_t     **window);

/**
 * Create a window, letting Wuss choose its position.
 *
 * As wuss_window_create, but instead of a content box you pass just the
 * content size; Wuss places the window (furniture included) in the first
 * free screen region, packed towards the top-left, tracking occupied area
 * across calls so successive auto-placed windows tile rather than stack.
 * When no region is large enough the window is cascaded from the previous
 * placement, stepping by a titlebar height and wrapping at the screen edge.
 *
 * The chosen slot is returned to the pool when the window is closed, or when
 * it is first moved or resized via wuss_window_move / wuss_window_resize
 * (after which Wuss no longer tracks its position). A window dragged by its
 * titlebar counts as moved.
 *
 * \param[in]  task    Owning task, as wuss_window_create.
 * \param[in]  size    Requested content-area size. Width and height must
 *                     both be positive.
 * \param[in]  title   Titlebar label, as wuss_window_create.
 * \param[in]  flags   Appearance flags, as wuss_window_create.
 * \param[in]  bg      Content background, as wuss_window_create.
 * \param[in]  doc     Virtual document extent, as wuss_window_create.
 * \param[in]  min_doc Minimum content size, as wuss_window_create.
 * \param[out] window  Newly created window. Becomes the topmost window.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if size's
 *         width or height is not positive, \ref result_OOM if the layout
 *         tracker could not be created, or another result code from
 *         wuss_window_create.
 */
result_t wuss_window_create_placed(wuss_task_t        *task,
                                   size2d_t            size,
                                   const char         *title,
                                   wuss_window_flags_t flags,
                                   wuss_backdrop_t     bg,
                                   size2d_t            doc,
                                   size2d_t            min_doc,
                                   wuss_window_t     **window);

/**
 * Destroy a window. The forced, unvetoable teardown: fires no
 * wuss_EVENT_PRE_CLOSE / wuss_EVENT_CLOSE. Used by wuss_task_destroy, tests
 * and error paths. For the user close-icon path, which the task can veto,
 * see wuss_window_try_close.
 *
 * \param[in] doomed Window to destroy. NULL is a no-op.
 */
void wuss_window_close(wuss_window_t *doomed);

/**
 * Attempt to close a window, giving its task a chance to veto.
 *
 * Fires wuss_EVENT_PRE_CLOSE to the task; a non-OK return vetoes the close,
 * the window stays open and that result is returned. Otherwise fires
 * wuss_EVENT_CLOSE (while the window is still alive) and then destroys the
 * window as per wuss_window_close.
 *
 * This is the path the titlebar close icon takes.
 *
 * \param[in] window Window to close. NULL is a no-op returning \ref
 *                   result_OK.
 * \return \ref result_OK if the window was closed (or was NULL), else the
 *         non-OK result the task returned from wuss_EVENT_PRE_CLOSE.
 */
result_t wuss_window_try_close(wuss_window_t *window);

/**
 * Move a window, preserving its size.
 *
 * \param[in] window Window to move.
 * \param[in] p      New screen coordinate of the window's content top-left.
 */
void wuss_window_move(wuss_window_t *window, point_t p);

/**
 * Show or hide a window without destroying it.
 *
 * A hidden window keeps its z-order slot and all its state, but is not drawn
 * and not hit-tested: it neither occludes lower windows nor catches the
 * pointer. wuss_window_move still works while hidden, so a caller can park a
 * window off to one side and re-show it in position later. Toggling
 * visibility invalidates the window's footprint so the next redraw picks up
 * the change.
 *
 * Showing a hidden window fires wuss_EVENT_PRE_SHOW to the task first: a
 * non-OK return vetoes the reveal, the window stays hidden and that result
 * is returned. On a successful reveal wuss_EVENT_SHOW follows. Hiding a
 * visible window, and any call that is a no-op (already in the requested
 * state), fires nothing and returns \ref result_OK.
 *
 * \param[in] window Window to show or hide.
 * \param[in] hidden Non-zero to hide, zero to show.
 * \return \ref result_OK on success or a no-op, else the non-OK result the
 *         task returned from wuss_EVENT_PRE_SHOW.
 */
result_t wuss_window_set_hidden(wuss_window_t *window, int hidden);

/**
 * Resize a window's content area, preserving its top-left position.
 *
 * \param[in] window Window to resize.
 * \param[in] size   New content size.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if size's
 *         width or height is not positive.
 */
result_t wuss_window_resize(wuss_window_t *window, size2d_t size);

/**
 * Move a window to one end of the z-order.
 *
 * \param[in] window Window to restack.
 * \param[in] reason wuss_ZORDER_FRONT or wuss_ZORDER_BACK.
 */
void wuss_window_restack(wuss_window_t *window, wuss_zorder_t reason);

/**
 * Fetch a window's current visible (on-screen) bounds: its full footprint,
 * including any titlebar/outline furniture, not just its content area.
 *
 * \param[in]  window  Window to query.
 * \param[out] visible Filled in with the window's visible bounds.
 */
void wuss_window_get_visible_bounds(const wuss_window_t *window,
                                    box_t               *visible);

/**
 * Fetch a window's current content-area bounds, screen space (as requested
 * at creation, or adjusted by a subsequent move/resize; excludes any
 * titlebar/outline furniture). Useful for computing invalidation regions
 * outside of a redraw callback.
 *
 * \param[in]  window  Window to query.
 * \param[out] content Filled in with the window's content bounds.
 */
void wuss_window_get_content_bounds(const wuss_window_t *window,
                                    box_t               *content);

/**
 * Mark a region of a window's content as dirty, for the next
 * wuss_redraw_dirty call. Content changes are opaque to Wuss, so tasks must
 * call this themselves (e.g. the union of an animated element's old and new
 * positions).
 *
 * \param[in] window    Window whose content changed.
 * \param[in] local_box Region, in window-local content coordinates (as
 *                      passed to the task's mouse callback), or NULL to mark
 *                      the whole content area dirty.
 */
void wuss_window_invalidate(wuss_window_t *window, const box_t *local_box);

/** Mark a window's whole content area as dirty. Shorthand for
 * wuss_window_invalidate(window, NULL). */
#define wuss_window_invalidate_all(window) \
  wuss_window_invalidate((window), NULL)

/**
 * Set a window's scroll offset: the point in virtual content space that
 * appears at the content area's top-left. Larger offsets bring later content
 * into view. Invalidates the content area so the next redraw picks up the
 * new offset; the task's redraw callback is responsible for using the offset
 * (via wuss_window_get_scroll) to draw the right portion of its content.
 *
 * \param[in] window Window to scroll.
 * \param[in] p      New scroll offset.
 */
void wuss_window_set_scroll(wuss_window_t *window, point_t p);

/**
 * Fetch a window's current scroll offset.
 *
 * \param[in]  window Window to query.
 * \param[out] p      Filled in with the scroll offset.
 */
void wuss_window_get_scroll(const wuss_window_t *window, point_t *p);

/**
 * Change a window's background, invalidating its content area so the next
 * redraw picks up the new fill.
 *
 * \param[in] window Window to change.
 * \param[in] bg     New content background: a flat colour or an 8x8 fill
 *                   pattern (see wuss_backdrop_t). Set its colour to
 *                   wuss_NO_BACKGROUND to hand background painting back to
 *                   the task.
 * \return \ref result_OK on success, \ref result_WUSS_BAD_COLOUR if any of
 *         bg's colours are out of range for the palette.
 */
result_t wuss_window_set_background(wuss_window_t  *window,
                                    wuss_backdrop_t bg);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_WINDOW_H */
