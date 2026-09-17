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
  /** Part of a window's content needs repaint. */
  wuss_EVENT_REDRAW,
  /** Button down/up/move over window content. */
  wuss_EVENT_MOUSE,
  /** A work-area button icon was clicked or hovered (window view); or, in
   *  the task view, a future shared/dock element -- reserved, nothing
   *  emits it yet. */
  wuss_EVENT_ICON,
  /** Mouse wheel used over a window's content. */
  wuss_EVENT_SCROLL,
  /** A visible window moved or resized. */
  wuss_EVENT_OPEN,
  /** A hidden window (item->window "borrowed window" menu leaf flagged
   *  wuss_MENU_ITEM_PRE_OPEN) is about to become visible. Call
   *  wuss_menu_open_window_now from the handler to proceed; not calling it
   *  leaves the window hidden and the row inert. A non-OK return is a
   *  genuine failure, not a veto. Only fires for a flagged row -- an
   *  unflagged item->window leaf just opens on hover, no event. */
  wuss_EVENT_PRE_SHOW,
  /** An item->submenu menu leaf flagged wuss_MENU_ITEM_PRE_OPEN is about to
   *  open. Delivered to the task that opened the chain (window == NULL),
   *  like wuss_EVENT_MENU_SELECT. Call wuss_menu_open_submenu_now from the
   *  handler, supplying the menu to open (allowing a shared instance to be
   *  retitled/retargeted first); not calling it leaves the row inert. A
   *  non-OK return is a genuine failure, not a veto. Only fires for a
   *  flagged row -- an unflagged item->submenu leaf just opens on hover,
   *  no event. */
  wuss_EVENT_PRE_SUBMENU_OPEN,
  /** A window has become visible (hidden->visible transition only, not at
   *  create). */
  wuss_EVENT_SHOW,
  /** A window is about to close via wuss_window_try_close; a non-OK return
   *  vetoes it. Never fired by wuss_window_close. */
  wuss_EVENT_PRE_CLOSE,
  /** A window has closed after a successful wuss_window_try_close. Never
   *  fired by wuss_window_close. This is not a veto point: PRE_CLOSE has
   *  already been accepted and wuss frees the window the instant this
   *  handler returns, so the task must drop any wuss_window_t it holds for
   *  this window here -- keeping or using it afterwards is a use-after-free.
   *  Put "keep the window open" logic (e.g. an unsaved-changes prompt) in
   *  PRE_CLOSE and veto from there. */
  wuss_EVENT_CLOSE,
  /** Wuss has finished its pending work; once per task per wuss_idle. */
  wuss_EVENT_IDLE,
  /** The task is shutting down, via wuss_task_destroy; its windows are
   *  still alive. */
  wuss_EVENT_QUIT,
  /** System palette changed, via wuss_set_palette; once per task. Recache
   *  any wuss_nearest_colour selections. */
  wuss_EVENT_PALETTE,
  /** A leaf menu item was picked; delivered to the task that opened the
   *  menu. */
  wuss_EVENT_MENU_SELECT,
  /** The menu chain the task opened has been closed by wuss itself -- a
   *  click outside every menu window, or another wuss_menu_open -- rather
   *  than by a pick or by the task's own wuss_menu_close. Delivered to the
   *  task that opened it (window == NULL); carries no data. A task that
   *  stored the wuss_menu_open handle must drop it here: the chain is
   *  already freed. Not fired for a SELECT pick (a wuss_EVENT_MENU_SELECT
   *  with a chain-closing button) or for a wuss_menu_close the task made. */
  wuss_EVENT_MENU_CLOSED,
  /** The pointer has moved onto this window's on-screen footprint (content
   *  or furniture) from elsewhere. Exactly one is outstanding per window at
   *  a time, always balanced by a later wuss_EVENT_POINTER_EXIT. Carries no
   *  data. Fires only on the window-crossing edge: moving between content
   *  and furniture within the same window does not re-fire it. */
  wuss_EVENT_POINTER_ENTER,
  /** The pointer has left this window's footprint -- onto another window,
   *  onto no window, or because the window is closing under it. Balances an
   *  earlier wuss_EVENT_POINTER_ENTER. Carries no data. A task must not
   *  assume the window is still usable if this arrives during teardown; it
   *  is safe to read but a wuss_EVENT_CLOSE may follow immediately. */
  wuss_EVENT_POINTER_EXIT
}
wuss_event_kind_t;

/**
 * The subset of wuss_event_kind_t a window's handle callback can receive.
 * Same integer values as the master enum; listed separately for
 * documentation and the debug-only dispatch assert.
 *
 * Members: wuss_EVENT_REDRAW, wuss_EVENT_MOUSE, wuss_EVENT_ICON,
 * wuss_EVENT_SCROLL, wuss_EVENT_OPEN, wuss_EVENT_PRE_SHOW, wuss_EVENT_SHOW,
 * wuss_EVENT_PRE_CLOSE, wuss_EVENT_CLOSE, wuss_EVENT_POINTER_ENTER,
 * wuss_EVENT_POINTER_EXIT.
 */
typedef wuss_event_kind_t wuss_window_event_kind_t;

/**
 * The subset of wuss_event_kind_t a task's handle callback can receive with
 * no window (window == NULL): the app-wide notifications.
 *
 * Members: wuss_EVENT_IDLE, wuss_EVENT_QUIT, wuss_EVENT_PALETTE,
 * wuss_EVENT_MENU_SELECT, wuss_EVENT_MENU_CLOSED,
 * wuss_EVENT_PRE_SUBMENU_OPEN, wuss_EVENT_ICON (reserved for a future
 * shared/dock element; nothing emits it yet).
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
     * pointer is inside a wuss_ICON_TYPE_ACTION icon's bounding box.
     * Label, hidden and disabled icons never raise this; those clicks
     * fall through as wuss_EVENT_MOUSE. action is DOWN/UP/MOVE; button
     * is a set of wuss_button_t flags, so test it with '&' rather than
     * comparing for equality. value is the icon's current value for a
     * wuss_ICON_TYPE_SLIDER (updated before this event is delivered, so it
     * always reflects the click/drag that raised it); meaningless for every
     * other icon type. In the task view (window == NULL) this is
     * reserved for a future shared/dock element and is never currently
     * emitted. */
    struct
    {
      wuss_icon_t        *icon;
      wuss_mouse_action_t action;
      wuss_button_t       button;
      int                 value;
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

    /** wuss_EVENT_PRE_SHOW (menu window leaf only): index is the row's
     * position in the hovering menu level's items. Call
     * wuss_menu_open_window_now(handle, index) to proceed. */
    struct
    {
      struct wuss__menu *handle;
      int                index;
    }
    pre_show;

    /** wuss_EVENT_PRE_SUBMENU_OPEN: index is the row's position in the
     * hovering menu level's items. Call
     * wuss_menu_open_submenu_now(handle, index, menu) to proceed,
     * supplying the (possibly retitled/retargeted) menu to open. */
    struct
    {
      struct wuss__menu *handle;
      int                index;
    }
    pre_submenu_open;

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

    /* wuss_EVENT_OPEN, wuss_EVENT_SHOW, wuss_EVENT_PRE_CLOSE,
     * wuss_EVENT_CLOSE, wuss_EVENT_IDLE, wuss_EVENT_QUIT,
     * wuss_EVENT_PALETTE and wuss_EVENT_MENU_CLOSED carry no data. */
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
 *         wuss_EVENT_PRE_CLOSE a non-OK return vetoes the transition. For
 *         wuss_EVENT_PRE_SHOW a non-OK return is a genuine failure, not a
 *         veto -- proceeding is opt-in via wuss_window_reveal_now /
 *         wuss_menu_open_window_now.
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
 * \return \ref result_OK on success, else an appropriate result code. For
 *         wuss_EVENT_PRE_SUBMENU_OPEN a non-OK return is a genuine failure,
 *         not a veto -- proceeding is opt-in via wuss_menu_open_submenu_now.
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

/**
 * Opt a task into self-destruct once its last window closes.
 *
 * With autoclose set, the moment a task's window list becomes empty (whether
 * via wuss_window_close or the wuss_window_try_close close path) the task
 * behaves as if wuss_task_destroy had been called: one wuss_EVENT_QUIT is
 * delivered, then the task is unlinked and freed. Free task_data from the
 * wuss_EVENT_QUIT case, not wuss_EVENT_CLOSE -- a closing window would
 * otherwise leave the task registered and still receiving wuss_idle /
 * wuss_set_palette broadcasts with a stale task_data.
 *
 * Suited to fire-and-forget tasks that own exactly the windows they spawn.
 * Leave it off for tasks that outlive their windows (e.g. one owning a pool
 * of transient menu windows).
 *
 * \param[in] task Task to configure. NULL is a no-op.
 * \param[in] on   Non-zero to enable autoclose, zero to disable.
 */
void wuss_task_set_autoclose(wuss_task_t *task, int on);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_TASK_H */
