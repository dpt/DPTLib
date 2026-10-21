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
 * \param[in]  flags   Appearance flags, e.g. wuss_WINDOW_CLOSE /
 *                     wuss_WINDOW_VSCROLL, OR'd together, or
 *                     wuss_WINDOW_DEFAULT for the default furniture.
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
 * \param[out] window  Newly created window. Becomes the frontmost window of
 *                     its stack: middle unless flags includes
 *                     wuss_WINDOW_STACK_TOP or wuss_WINDOW_STACK_BACK.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if content's
 *         width or height is not positive, \ref result_WUSS_BAD_COLOUR if
 *         any of bg's colours are out of range for the palette, \ref
 *         result_BAD_ARG if flags names both wuss_WINDOW_STACK_TOP and
 *         wuss_WINDOW_STACK_BACK, or another appropriate result code.
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
 * free screen region, packed towards the top-left, avoiding every shown
 * window at its current position so successive auto-placed windows tile
 * rather than stack. When no region is large enough the window is centred on
 * the screen (or pinned at the top-left if larger than the screen). See
 * wuss_window_create_at for other placements.
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
 * Where an anchored window goes on the screen: one horizontal value OR'd
 * with one vertical value, e.g. wuss_ALIGN_TOP | wuss_ALIGN_RIGHT. Both axes
 * default to the centre, so 0 is centred on the screen.
 */
typedef enum wuss_align
{
  wuss_ALIGN_CENTRE = 0,      /**< Centred on both axes. */
  wuss_ALIGN_LEFT   = 1 << 0, /**< Against the left edge. */
  wuss_ALIGN_RIGHT  = 1 << 1, /**< Against the right edge. */
  wuss_ALIGN_TOP    = 1 << 2, /**< Against the top edge. */
  wuss_ALIGN_BOTTOM = 1 << 3  /**< Against the bottom edge. */
}
wuss_align_t;

/** Placement options for wuss_place_t, OR'd together. */
typedef enum wuss_place_flags
{
  /**
   * Try the first free screen region first, as wuss_window_create_placed;
   * the anchor or relative position is then only the fallback for when no
   * region is large enough. The search starts from the screen corner that
   * align names (wuss_ALIGN_RIGHT and/or wuss_ALIGN_BOTTOM), else the
   * top-left, so the window opens nearest that corner.
   */
  wuss_PLACE_SPARE         = 1 << 0,

  /**
   * Keep the window clear of the icon bar: the bar's strip is removed from
   * the screen area used for placement. No effect without an icon bar.
   */
  wuss_PLACE_AVOID_ICONBAR = 1 << 1
}
wuss_place_flags_t;

/**
 * Where to open a window. The footprint (furniture included) is positioned
 * at its base position plus offset, then clamped to fit the placement area
 * (the screen, less the icon bar with wuss_PLACE_AVOID_ICONBAR), favouring
 * the top-left when it is larger than the area.
 *
 * The base position is the top-left of relative_to's footprint when
 * relative_to is non-NULL; otherwise it is the area aligned to by align,
 * inset from any edge it is against by the auto-placement gutter. A
 * zero-initialised wuss_place_t centres the window on the screen.
 */
typedef struct wuss_place
{
  wuss_place_flags_t   flags;       /**< Placement options. */
  wuss_align_t         align;       /**< Anchor if relative_to is NULL. */
  const wuss_window_t *relative_to; /**< Place relative to, or NULL. */
  point_t              offset;      /**< Added to the base position. */
}
wuss_place_t;

/**
 * Create a window at a position described by \p place.
 *
 * As wuss_window_create_placed, but with the position chosen by \p place: in
 * a free region, at one of nine screen anchors, or relative to another
 * window. wuss_window_create_placed is this with place's flags set to
 * wuss_PLACE_SPARE and everything else zero.
 *
 * \param[in]  task    Owning task, as wuss_window_create.
 * \param[in]  place   Where to put the window; copied.
 * \param[in]  size    Requested content-area size. Width and height must
 *                     both be positive.
 * \param[in]  title   Titlebar label, as wuss_window_create.
 * \param[in]  flags   Appearance flags, as wuss_window_create.
 * \param[in]  bg      Content background, as wuss_window_create.
 * \param[in]  doc     Virtual document extent, as wuss_window_create.
 * \param[in]  min_doc Minimum content size, as wuss_window_create.
 * \param[out] window  Newly created window, as wuss_window_create.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if size's
 *         width or height is not positive, \ref result_OOM if the layout
 *         tracker could not be created, or another result code from
 *         wuss_window_create.
 */
result_t wuss_window_create_at(wuss_task_t        *task,
                               const wuss_place_t *place,
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
 * Toggle a window between its normal and its full size, as a click on its
 * toggle-size icon does.
 *
 * \param[in] window Window to toggle. NULL, or a window created without
 *                   wuss_WINDOW_TOGGLE_SIZE, is a no-op.
 */
void wuss_window_toggle_size(wuss_window_t *window);

/**
 * Fetch the task that owns a window.
 *
 * \param[in] window Window to query.
 * \return The owning task. Never NULL.
 */
wuss_task_t *wuss_window_get_task(const wuss_window_t *window);

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
 * Showing a hidden window always fires wuss_EVENT_PRE_SHOW to the task
 * first, whether the window is plain or a menu leaf (flagged or not). For a
 * plain window, or a menu leaf not flagged wuss_MENU_ITEM_PRE_OPEN, this
 * defaults to proceeding; the handler only needs to call
 * wuss_window_reveal_now to react before it shows. For a menu leaf flagged
 * wuss_MENU_ITEM_PRE_OPEN, the handler must call wuss_menu_open_window_now
 * synchronously to opt in -- not calling it leaves the window hidden. A
 * non-OK return from the handler is a genuine failure, not a veto. On a
 * successful reveal wuss_EVENT_SHOW follows. Hiding a visible window, and
 * any call that is a no-op (already in the requested state), fires nothing
 * and returns \ref result_OK.
 *
 * \param[in] window Window to show or hide.
 * \param[in] hidden Non-zero to hide, zero to show.
 * \return \ref result_OK on success or a no-op, else the non-OK result the
 *         task returned from wuss_EVENT_PRE_SHOW.
 */
result_t wuss_window_set_hidden(wuss_window_t *window, int hidden);

/**
 * Proceed with a pending wuss_EVENT_PRE_SHOW for a plain (non-menu) window:
 * opts in to revealing \p window. Must be called synchronously from within
 * the handler that received the event. For a menu leaf, call
 * wuss_menu_open_window_now instead.
 *
 * \param[in] window Window named by the pending wuss_EVENT_PRE_SHOW.
 * \return \ref result_OK on success, else an appropriate result code.
 */
result_t wuss_window_reveal_now(wuss_window_t *window);

/**
 * Resize a window's content area, preserving its top-left position. As
 * wuss_window_create does, the size is capped so the window fits the screen
 * from its top-left, except that it never drops below the minimum grabbable
 * size.
 *
 * \param[in] window Window to resize.
 * \param[in] size   Requested content size; capped to the screen.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if size's
 *         width or height is not positive.
 */
result_t wuss_window_resize(wuss_window_t *window, size2d_t size);

/**
 * Set a window's virtual document extent, as wuss_window_create's doc
 * parameter. Changes the scrollbar sausage size and re-clamps the current
 * scroll offset to the new extent; the whole content area is invalidated so
 * the next redraw repaints against the new extent.
 *
 * \param[in] window Window whose document extent changed.
 * \param[in] doc    New virtual document extent.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if doc's
 *         width or height is not positive.
 */
result_t wuss_window_set_doc(wuss_window_t *window, size2d_t doc);

/** Pass as a component of wuss_window_set_min_size to leave that axis as it
 * is. */
#define wuss_KEEP (-1)

/**
 * Set the smallest content size a resize-drag or toggle-size will shrink a
 * window to, as wuss_window_create's min_doc parameter. Either axis may be
 * left unchanged by passing \ref wuss_KEEP for it. The furniture's own
 * minimum still applies.
 *
 * \param[in] window Window to configure.
 * \param[in] min    Minimum content size; each component at least 0, or \ref
 *                   wuss_KEEP.
 * \return \ref result_OK, or \ref result_BAD_ARG if a component is below
 *         \ref wuss_KEEP.
 */
result_t wuss_window_set_min_size(wuss_window_t *window, size2d_t min);

/**
 * Fetch the largest content size this window could have on the current
 * screen if it were moved to fit: the whole screen less the window's own
 * outline, titlebar and scrollbar furniture. Floored at the built-in minimum
 * content size. Depends on the screen size and the furniture, so ask again
 * after wuss_EVENT_RELAYOUT rather than caching it.
 *
 * \param[in]  window Window to query.
 * \param[out] max    Filled with the largest content size.
 */
void wuss_window_get_max_content(const wuss_window_t *window, size2d_t *max);

/**
 * Move a window to one end of its own stack (see wuss_stack_t). It never
 * crosses into another stack: a middle window brought to front stays behind
 * every top window.
 *
 * \param[in] window Window to restack.
 * \param[in] reason wuss_ZORDER_FRONT or wuss_ZORDER_BACK.
 */
void wuss_window_restack(wuss_window_t *window, wuss_zorder_t reason);

/**
 * Move a window to the front of another stack (see wuss_stack_t). A no-op if
 * it is already in \p stack. A visible window repaints whatever its new
 * position uncovers or covers; a hidden one is only relinked.
 *
 * \param[in] window Window to move.
 * \param[in] stack  Destination stack.
 */
void wuss_window_set_stack(wuss_window_t *window, wuss_stack_t stack);

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
 * \param[in] window    Window whose content changed, or NULL to do nothing.
 * \param[in] local_box Region, in window-local content coordinates (as
 *                      passed to the task's mouse callback), or NULL to mark
 *                      the whole content area dirty.
 */
void wuss_window_invalidate(wuss_window_t *window, const box_t *local_box);

/** Mark a window's whole visible content rectangle as dirty -- not the
 * parts of the document scrolled out of view; use
 * wuss_window_invalidate_extent for that. Shorthand for
 * wuss_window_invalidate(window, NULL). */
#define wuss_window_invalidate_visible(window) \
  wuss_window_invalidate((window), NULL)

/**
 * Mark a window's whole virtual document extent as dirty, not just the part
 * currently on screen. wuss_window_invalidate_visible only covers the
 * visible content rectangle, which is enough when the document is no bigger
 * than the window; use this when a change touches the whole document (a
 * colour or theme swap, say) so every scroll position repaints correctly.
 * The extent is clipped to what is actually visible before anything is
 * queued.
 *
 * \param[in] window Window whose whole document changed, or NULL to do
 *                   nothing.
 */
void wuss_window_invalidate_extent(wuss_window_t *window);

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

/**
 * Change a window's title, repainting its furniture if it differs. The title
 * is copied and truncated as for wuss_window_create. No-op when built
 * without furniture.
 *
 * \param[in] window Window to change.
 * \param[in] title  New title. NULL is treated as "".
 */
void wuss_window_set_title(wuss_window_t *window, const char *title);

/**
 * A window's current title, as last set by wuss_window_create or
 * wuss_window_set_title, truncated the same way. "" (never NULL) when built
 * without furniture, or when no title was set.
 *
 * \param[in] window Window to query.
 * \return The title, owned by \p window; valid until the next
 *         wuss_window_set_title call or the window is closed.
 */
const char *wuss_window_get_title(const wuss_window_t *window);

/* ----------------------------------------------------------------------- */

/** The edge of a parent window that a toolbar attaches to. */
typedef enum wuss_edge
{
  wuss_EDGE_LEFT,   /**< Against the parent's left edge. */
  wuss_EDGE_RIGHT,  /**< Against the parent's right edge. */
  wuss_EDGE_TOP,    /**< Above the parent's top edge (its titlebar). */
  wuss_EDGE_BOTTOM  /**< Below the parent's bottom edge. */
}
wuss_edge_t;

/**
 * Attach a window to an edge of another window as a toolbar.
 *
 * The toolbar sits flush against the outside of the parent's footprint and
 * moves with it. It keeps its own size whatever the parent's size. Several
 * toolbars on one edge stack outwards in attach order; a hidden toolbar
 * takes no room. Where the parent is near the screen edge, the toolbars on
 * that side stay on the screen, overlapping the parent if necessary, until
 * the parent's edge itself leaves the screen.
 *
 * The toolbar is moved into the parent's stack, directly above the parent,
 * and from then on the group restacks together: restacking either window
 * restacks both. Hiding or showing the parent hides or shows its toolbars.
 * Closing the parent closes its toolbars too, as wuss_window_close does,
 * without any events of their own. wuss_window_move does nothing to a
 * toolbar; wuss_window_resize works and re-lays out the group.
 *
 * A Select or Adjust press on a toolbar that is not wuss_WINDOW_FOCUSABLE
 * gives the input focus to its parent instead. The parent's titlebar is
 * drawn in the focus tint while the parent or any of its toolbars has the
 * focus.
 *
 * \param[in] toolbar Window to attach. Must have been created with
 *                    wuss_WINDOW_NO_TITLEBAR and with no furniture other
 *                    than an outline and scrollbars.
 * \param[in] parent  Window to attach it to. Must belong to the same task.
 * \param[in] edge    Parent edge to attach to.
 * \param[in] align   Position along the edge, against the parent's content
 *                    area: wuss_ALIGN_TOP, wuss_ALIGN_BOTTOM or centred for
 *                    a left or right edge, wuss_ALIGN_LEFT, wuss_ALIGN_RIGHT
 *                    or centred for a top or bottom edge. The other axis'
 *                    bits are ignored.
 * \param[in] offset  Pixels added along the edge after aligning (rightwards
 *                    or downwards).
 * \return \ref result_OK on success, or \ref result_BAD_ARG if the windows
 *         belong to different tasks, either is already a toolbar, toolbar
 *         has toolbars of its own, or toolbar has furniture other than an
 *         outline and scrollbars.
 */
result_t wuss_window_attach(wuss_window_t *toolbar,
                            wuss_window_t *parent,
                            wuss_edge_t    edge,
                            wuss_align_t   align,
                            int            offset);

/**
 * Detach a toolbar from its parent. It stays where it is as an ordinary
 * window; the parent's other toolbars close up.
 *
 * \param[in] toolbar Toolbar to detach. A window that is not a toolbar is
 *                    left alone.
 */
void wuss_window_detach(wuss_window_t *toolbar);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_WINDOW_H */
