/* window.h -- wuss window API */

/**
 * \file window.h
 *
 * A Wuss window: creation, destruction, positioning, sizing and task delegation
 * of content drawing and mouse handling.
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
#include "framebuf/screen.h"

#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Which kind of event a wuss_event_t carries; more will be added over time. */
typedef enum wuss_event_kind
{
  wuss_EVENT_IDLE,   /**< Wuss has finished its pending tasks. */
  wuss_EVENT_REDRAW, /**< Part of the window's content needs repainting. */
  wuss_EVENT_OPEN,   /**< Window moved or resized. */
  wuss_EVENT_CLOSE,  /**< Close icon clicked; Wuss takes no action itself. */
  wuss_EVENT_MOUSE,  /**< Button down/up over the window's content. */
  wuss_EVENT_SCROLL, /**< Mouse wheel used over the window's content. */
  wuss_EVENT_QUIT    /**< Task shutting down, via wuss_task_stop. */
}
wuss_event_kind_t;

/**
 * An event delivered to a task's handle callback. Only the union member
 * matching \c kind is valid.
 */
typedef struct wuss_event
{
  wuss_event_kind_t kind;
  union
  {
    /** wuss_EVENT_REDRAW: called with scr->clip already set to the
     * on-screen, clipped content area. bounds and scroll are exactly what
     * wuss_window_get_content_bounds/wuss_window_get_scroll would return,
     * passed through so tasks don't need to call back into Wuss on every
     * redraw. */
    struct
    {
      screen_t    *scr;

      /**
       * The region that actually needs repainting, screen space; a subset of
       * bounds. Tasks should only touch pixels within this box.
       */
      const box_t *content;

      /**
       * The window's full (unclipped) content-area box, screen space, as per
       * wuss_window_get_content_bounds; for converting screen position to
       * document position.
       */
      const box_t *bounds;

      /** Current scroll offset, as per wuss_window_get_scroll. */
      point_t      scroll;
    }
    redraw;

    /** wuss_EVENT_MOUSE: point is window-local content coordinates (the
     * content area's top-left is (0,0)). button is meaningful for
     * DOWN/UP. */
    struct
    {
      wuss_mouse_action_t action;
      point_t             point;
      wuss_button_t       button;
    }
    mouse;

    /** wuss_EVENT_SCROLL: point is window-local content coordinates, as
     * per mouse. delta's sign and units are as passed to wuss_scroll. */
    struct
    {
      point_t point;
      int     delta;
    }
    scroll;

    /* wuss_EVENT_IDLE, wuss_EVENT_CLOSE, wuss_EVENT_QUIT and
     * wuss_EVENT_OPEN carry no data. */
  }
  data;
}
wuss_event_t;

/**
 * Task event callback.
 *
 * \param[in] window    The window receiving the event.
 * \param[in] event     The event; see wuss_event_t.
 * \param[in] task_data As passed to wuss_window_create.
 * \return \ref result_OK on success, else an appropriate result code.
 */
typedef result_t (wuss_event_fn_t)(wuss_window_t      *window,
                                   const wuss_event_t *event,
                                   void               *task_data);

/** A window's content delegate. Copied by value into the window at creation. */
typedef struct wuss_task
{
  /**
   * NULL => task receives no events; Wuss still fills the content background
   * per wuss_window_create's bg.
   */
  wuss_event_fn_t *handle;

  void            *task_data;
}
wuss_task_t;

/**
 * Build a wuss_task_t from its fields.
 *
 * \param[in] handle    Event callback, or NULL for a task that receives no
 *                      events.
 * \param[in] task_data Opaque pointer passed back to the callback.
 * \return The populated task.
 */
wuss_task_t wuss_task_start(wuss_event_fn_t *handle,
                            void            *task_data);

/**
 * Notify a window's task that it is shutting down, via wuss_EVENT_QUIT. Not
 * called automatically by wuss_window_close; call it first if the task needs
 * notice before its window is torn down.
 *
 * \param[in] window Window whose task should be stopped.
 * \return \ref result_OK on success, else the result returned by the task's
 *         handle callback.
 */
result_t wuss_task_stop(wuss_window_t *window);

/**
 * Broadcast a wuss_EVENT_IDLE event to every window's task, in z-order.
 * Intended to be called once per main-loop iteration, after other pending input
 * has been handled, so tasks can drive their own animation/timers without the
 * caller stepping each one individually.
 *
 * \param[in] wuss Window manager whose windows' tasks should go idle.
 * \return \ref result_OK on success, else the first non-OK result returned by a
 *         task's handle callback.
 */
result_t wuss_idle(wuss_t *wuss);

/**
 * Create a window.
 *
 * Furniture (titlebar/outline) is added outside \p content, not carved out of
 * it: before clamping, the window's content area is exactly \p content, and its
 * on-screen footprint (see wuss_window_get_visible_bounds) is \p content
 * expanded outward by whatever furniture flags request. If that footprint would
 * then fall off the top or left edge of the screen, the window (content
 * included) is nudged right/down just enough to bring it flush with the edge,
 * so the titlebar/close icon stay reachable; a window wider or taller than the
 * screen keeps its top-left corner on-screen instead. The bottom/right edges
 * are not clamped.
 *
 * \param[in]  wuss       Window manager to create the window on.
 * \param[in]  content    Requested content-area bounds, screen space. Copied
 *                        in.
 * \param[in]  title      Titlebar label, or NULL for none. Copied in, truncated
 *                        if too long. Ignored if flags includes
 *                        wuss_WINDOW_NO_TITLEBAR.
 * \param[in]  flags      Appearance flags, e.g. wuss_WINDOW_NO_TITLEBAR /
 *                        wuss_WINDOW_NO_OUTLINE, OR'd together, or
 *                        wuss_WINDOW_NONE for the default furniture.
 * \param[in]  bg         Content background, filled by Wuss before each redraw,
 *                        or wuss_NO_BACKGROUND for the task to draw its own
 *                        background (avoids a redundant fill behind an opaque
 *                        task). Changeable later via
 *                        wuss_window_set_background.
 * \param[in]  task       Content delegate. Copied in. May be NULL for a window
 *                        with no content handling.
 * \param[in]  doc_width  Virtual document width, for the horizontal scrollbar's
 *                        sausage proportion; pass content's own width for a
 *                        window with nothing to scroll.
 * \param[in]  doc_height Virtual document height, for the vertical scrollbar's
 *                        sausage proportion; pass content's own height for a
 *                        window with nothing to scroll.
 * \param[out] window     Newly created window. Becomes the topmost window.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if content's
 *         width or height is not positive, \ref result_WUSS_BAD_COLOUR if bg is
 *         out of range for the palette, or another appropriate result code.
 */
result_t wuss_window_create(wuss_t             *wuss,
                            const box_t        *content,
                            const char         *title,
                            wuss_window_flags_t flags,
                            wuss_colour_t       bg,
                            const wuss_task_t  *task,
                            int                 doc_width,
                            int                 doc_height,
                            wuss_window_t     **window);

/**
 * Destroy a window.
 *
 * \param[in] doomed Window to destroy.
 */
void wuss_window_close(wuss_window_t *doomed);

/**
 * Move a window, preserving its size.
 *
 * \param[in] window Window to move.
 * \param[in] p      New screen coordinate of the window's content top-left.
 */
void wuss_window_move(wuss_window_t *window, point_t p);

/**
 * Resize a window's content area, preserving its top-left position.
 *
 * \param[in] window Window to resize.
 * \param[in] width  New content width.
 * \param[in] height New content height.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if width or
 *         height is not positive.
 */
result_t wuss_window_resize(wuss_window_t *window, int width, int height);

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
 * Fetch a window's current content-area bounds, screen space (as requested at
 * creation, or adjusted by a subsequent move/resize; excludes any
 * titlebar/outline furniture). Useful for computing invalidation regions
 * outside of a redraw callback.
 *
 * \param[in]  window  Window to query.
 * \param[out] content Filled in with the window's content bounds.
 */
void wuss_window_get_content_bounds(const wuss_window_t *window,
                                    box_t               *content);

/**
 * Mark a region of a window's content as dirty, for the next wuss_redraw_dirty
 * call. Content changes are opaque to Wuss, so tasks must call this themselves
 * (e.g. the union of an animated element's old and new positions).
 *
 * \param[in] window     Window whose content changed.
 * \param[in] local_box  Region, in window-local content coordinates (as passed
 *                       to the task's mouse callback), or NULL to mark the
 *                       whole content area dirty.
 */
void wuss_window_invalidate(wuss_window_t *window, const box_t *local_box);

/** Mark a window's whole content area as dirty. Shorthand for
 * wuss_window_invalidate(window, NULL). */
#define wuss_window_invalidate_all(window) \
  wuss_window_invalidate((window), NULL)

/**
 * Set a window's scroll offset: the point in virtual content space that appears
 * at the content area's top-left. Larger offsets bring later content into view.
 * Invalidates the content area so the next redraw picks up the new offset; the
 * task's redraw callback is responsible for using the offset (via
 * wuss_window_get_scroll) to draw the right portion of its content.
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
 * Change a window's background colour, invalidating its content area so the
 * next redraw picks up the new fill.
 *
 * \param[in] window Window to change.
 * \param[in] bg     New content background, as an index into the system
 *                   palette, or wuss_NO_BACKGROUND to hand background painting
 *                   back to the task.
 * \return \ref result_OK on success, \ref result_WUSS_BAD_COLOUR if bg is out
 *         of range for the palette.
 */
result_t wuss_window_set_background(wuss_window_t *window, wuss_colour_t bg);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_WINDOW_H */
