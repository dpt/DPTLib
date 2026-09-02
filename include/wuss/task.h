/* wuss/task.h -- wuss task API */

/**
 * \file task.h
 *
 * A Wuss task: a registered object that owns windows and is the single
 * delivery target for their events, plus the events themselves.
 *
 * A task is a mini-instance: wuss_task_create / wuss_task_destroy rhyme with
 * wuss_create / wuss_destroy. Every window is created against a task (see
 * window.h), and that task's one handle callback receives all events for all
 * its windows, as well as the app-wide IDLE / PALETTE / MENU_SELECT
 * notifications.
 */

#ifndef WUSS_TASK_H
#define WUSS_TASK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"
#include "geom/point.h"
#include "framebuf/screen.h"

#include "wuss/wuss.h"
#include "wuss/icon.h"

/* ----------------------------------------------------------------------- */

/**
 * The master event kind. Every event delivered to any recipient carries one
 * of these values; the per-recipient views below are strict subsets of it
 * with the same integer values, so no translation happens at dispatch.
 */
typedef enum wuss_event_kind
{
  wuss_EVENT_REDRAW,      /**< Part of a window's content needs repaint. */
  wuss_EVENT_MOUSE,       /**< Button down/up/move over window content. */
  wuss_EVENT_ICON,        /**< A work-area button icon was clicked or hovered
                               (window view); or, in the task view, a future
                               shared/dock element -- reserved, nothing emits
                               it yet. */
  wuss_EVENT_SCROLL,      /**< Mouse wheel used over a window's content. */
  wuss_EVENT_OPEN,        /**< A visible window moved or resized. */
  wuss_EVENT_PRE_SHOW,    /**< A hidden window is about to become visible; a
                               non-OK return vetoes the reveal. */
  wuss_EVENT_SHOW,        /**< A window has become visible (hidden->visible
                               transition only, not at create). */
  wuss_EVENT_PRE_CLOSE,   /**< A window is about to close via
                               wuss_window_try_close; a non-OK return vetoes
                               it. Never fired by wuss_window_close. */
  wuss_EVENT_CLOSE,       /**< A window has closed after a successful
                               wuss_window_try_close. Never fired by
                               wuss_window_close. */
  wuss_EVENT_IDLE,        /**< Wuss has finished its pending work; once per
                               task per wuss_idle. */
  wuss_EVENT_QUIT,        /**< The task is shutting down, via
                               wuss_task_destroy; its windows are still
                               alive. */
  wuss_EVENT_PALETTE,     /**< System palette changed, via wuss_set_palette;
                               once per task. Recache any wuss_nearest_colour
                               selections. */
  wuss_EVENT_MENU_SELECT  /**< A leaf menu item was picked; delivered to the
                               task that opened the menu. */
}
wuss_event_kind_t;

/**
 * The subset of wuss_event_kind_t a window's handle callback can receive.
 * Same integer values as the master enum; listed separately for
 * documentation and the debug-only dispatch assert.
 *
 * Members: wuss_EVENT_REDRAW, wuss_EVENT_MOUSE, wuss_EVENT_ICON,
 * wuss_EVENT_SCROLL, wuss_EVENT_OPEN, wuss_EVENT_PRE_SHOW, wuss_EVENT_SHOW,
 * wuss_EVENT_PRE_CLOSE, wuss_EVENT_CLOSE.
 */
typedef wuss_event_kind_t wuss_window_event_kind_t;

/**
 * The subset of wuss_event_kind_t a task's handle callback can receive with
 * no window (window == NULL): the app-wide notifications.
 *
 * Members: wuss_EVENT_IDLE, wuss_EVENT_QUIT, wuss_EVENT_PALETTE,
 * wuss_EVENT_MENU_SELECT, wuss_EVENT_ICON (reserved for a future shared/dock
 * element; nothing emits it yet).
 */
typedef wuss_event_kind_t wuss_task_event_kind_t;

/**
 * An event delivered to a handle callback. Only the union member matching \c
 * kind is valid; several kinds carry no data.
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

    /** wuss_EVENT_MOUSE: point is in virtual content space -- the window's
     * scroll offset has already been added, so the task must not add it
     * again. With a scroll offset of (0,0) this is the same as window-local
     * content coordinates, where the content area's top-left is (0,0).
     * button is meaningful for DOWN/UP, and is a set of wuss_button_t
     * flags, so test it with '&' rather than comparing for equality. */
    struct
    {
      wuss_mouse_action_t action;
      point_t             point;
      wuss_button_t       button;
    }
    mouse;

    /** wuss_EVENT_ICON: delivered instead of wuss_EVENT_MOUSE while the
     * pointer is inside a wuss_ICON_TYPE_BUTTON icon's bounding box.
     * Label, hidden and disabled icons never raise this; those clicks
     * fall through as wuss_EVENT_MOUSE. action is DOWN/UP/MOVE; button
     * is a set of wuss_button_t flags, so test it with '&' rather than
     * comparing for equality. In the task view (window == NULL) this is
     * reserved for a future shared/dock element and is never currently
     * emitted. */
    struct
    {
      wuss_icon_t        *icon;
      wuss_mouse_action_t action;
      wuss_button_t       button;
    }
    icon;

    /** wuss_EVENT_SCROLL: point is window-local content coordinates, as
     * per mouse. delta's sign and units are as passed to wuss_scroll. */
    struct
    {
      point_t point;
      int     delta;
    }
    scroll;

    /** wuss_EVENT_MENU_SELECT: delivered to the task that called
     * wuss_menu_open. menu is the (sub)menu the item belongs to; index is
     * its position in menu->items; button is the wuss_button_t flags for the
     * release -- ADJUST keeps the chain open, SELECT closes it; test with
     * '&'. */
    struct
    {
      const struct wuss_menu *menu;
      int                     index;
      wuss_button_t           button;
    }
    menu_select;

    /* wuss_EVENT_OPEN, wuss_EVENT_PRE_SHOW, wuss_EVENT_SHOW,
     * wuss_EVENT_PRE_CLOSE, wuss_EVENT_CLOSE, wuss_EVENT_IDLE,
     * wuss_EVENT_QUIT and wuss_EVENT_PALETTE carry no data. */
  }
  data;
}
wuss_event_t;

/**
 * A window's event callback: receives the wuss_window_event_kind_t subset.
 *
 * \param[in] window    The window receiving the event.
 * \param[in] event     The event; see wuss_event_t.
 * \param[in] task_data As passed to wuss_task_create.
 * \return \ref result_OK on success, else an appropriate result code. For
 *         wuss_EVENT_PRE_SHOW / wuss_EVENT_PRE_CLOSE a non-OK return vetoes
 *         the transition.
 */
typedef result_t (wuss_window_fn_t)(wuss_window_t      *window,
                                    const wuss_event_t *event,
                                    void               *task_data);

/**
 * A task's event callback: receives the wuss_task_event_kind_t subset,
 * always with window == NULL.
 *
 * \param[in] window    Always NULL for task-view events; the parameter is
 *                      kept so a single handle can serve both views.
 * \param[in] event     The event; see wuss_event_t.
 * \param[in] task_data As passed to wuss_task_create.
 * \return \ref result_OK on success, else an appropriate result code.
 */
typedef result_t (wuss_task_fn_t)(wuss_window_t      *window,
                                  const wuss_event_t *event,
                                  void               *task_data);

/**
 * Task creation descriptor. handle serves both the window-view and task-view
 * events for every window the task owns.
 */
typedef struct wuss_task_desc
{
  /**
   * Event callback, or NULL for a task that receives no events (its windows
   * still get their background filled per wuss_window_create's bg). The one
   * callback is invoked for both wuss_window_fn_t-shaped and
   * wuss_task_fn_t-shaped events; the two typedefs have the same signature.
   */
  wuss_window_fn_t *handle;

  /** Opaque pointer passed back to handle. */
  void             *task_data;

  /** Optional name, for debugging/tracing; borrowed, not copied. */
  const char       *name;
}
wuss_task_desc_t;

/**
 * Register a task on a window manager.
 *
 * The task is appended to the manager's task list; app-wide notifications
 * (wuss_idle, wuss_set_palette) are delivered to tasks in registration
 * order.
 *
 * \param[in]  wuss Window manager.
 * \param[in]  desc Task descriptor; copied in (name is borrowed).
 * \param[out] task Newly registered task.
 * \return \ref result_OK on success, \ref result_OOM if the task node could
 *         not be allocated.
 */
result_t wuss_task_create(wuss_t                 *wuss,
                          const wuss_task_desc_t *desc,
                          wuss_task_t           **task);

/**
 * Unregister and free a task.
 *
 * Fires one wuss_EVENT_QUIT to the task's handle (its windows are still
 * alive), then force-closes every window the task owns via wuss_window_close
 * (no wuss_EVENT_PRE_CLOSE / wuss_EVENT_CLOSE), unlinks the task from the
 * manager and frees it.
 *
 * \param[in] doomed Task to destroy. NULL is a no-op.
 */
void wuss_task_destroy(wuss_task_t *doomed);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_TASK_H */
