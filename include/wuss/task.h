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
  /** A work-area button icon was clicked or hovered (window view, see
   *  data.icon); or an icon bar icon was clicked or dragged (task view,
   *  window == NULL, see data.iconbar_icon). */
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
  wuss_EVENT_POINTER_EXIT,
  /** This window has gained the input focus (see wuss_set_focus). Carries
   *  no data. */
  wuss_EVENT_GAIN_FOCUS,
  /** This window has lost the input focus: to another window, to none, or
   *  because it is being hidden or closed via wuss_window_try_close. Not
   *  fired by wuss_window_close. Carries no data. */
  wuss_EVENT_LOSE_FOCUS,
  /** A key was pressed while this window had the input focus. Return \ref
   *  result_WUSS_KEY_UNCLAIMED to decline it. */
  wuss_EVENT_KEY,
  /** A message addressed to this window (window view) or broadcast (task
   *  view, window == NULL) has arrived; see data.message and wuss/message.h.
   *  To acknowledge a recorded message from this handler, call
   *  wuss_acknowledge (or send with your_ref set to the message's my_ref)
   *  before returning. */
  wuss_EVENT_MESSAGE,
  /** A recorded message this task sent went unacknowledged; delivered to
   *  the sending task (window == NULL), carrying the original message's
   *  fields via data.message. */
  wuss_EVENT_MESSAGE_BOUNCED,
  /** A core drag session (see wuss_drag_start) has ended; delivered to the
   *  window the drag was started against. See data.drag_end. */
  wuss_EVENT_DRAG_END,
  /** A key was released while this window had the input focus. Carries the
   *  same data.key as wuss_EVENT_KEY, but a printable key's code is its
   *  unshifted character, which may differ from the code its press
   *  reported. Unclaimable: \ref result_WUSS_KEY_UNCLAIMED is treated as
   *  \ref result_OK. Not reported by every frontend. */
  wuss_EVENT_KEY_UP,
  /** The room available to this window's layout has changed: the screen was
   *  resized (wuss_resize) or the furniture metrics were (wuss_set_config).
   *  Delivered once to every ordinary (non-pinned) window, after wuss has
   *  already moved and shrunk windows to fit, so the window's bounds are
   *  final. A window that is laid out from its size should re-read
   *  wuss_window_get_max_content, re-measure anything that depends on the
   *  furniture or fonts and update its document extent. It is not raised
   *  for a window wuss left alone, nor by a plain move or resize (see
   *  wuss_EVENT_OPEN). Carries no data. */
  wuss_EVENT_RELAYOUT,
  /** ADJUST was pressed on this window's close icon. wuss does nothing
   *  else with the press -- the window stays open -- so the task decides
   *  what it means: on RISC OS a filer opens the parent directory and
   *  closes this window unless Shift is held. See data.close_adjust. */
  wuss_EVENT_CLOSE_ADJUST
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
 * wuss_EVENT_POINTER_EXIT, wuss_EVENT_GAIN_FOCUS, wuss_EVENT_LOSE_FOCUS,
 * wuss_EVENT_KEY, wuss_EVENT_KEY_UP, wuss_EVENT_MESSAGE (a message addressed
 * to this window), wuss_EVENT_DRAG_END, wuss_EVENT_RELAYOUT,
 * wuss_EVENT_CLOSE_ADJUST.
 */
typedef wuss_event_kind_t wuss_window_event_kind_t;

/**
 * The subset of wuss_event_kind_t a task's handle callback can receive with
 * no window (window == NULL): the app-wide notifications.
 *
 * Members: wuss_EVENT_IDLE, wuss_EVENT_QUIT, wuss_EVENT_PALETTE,
 * wuss_EVENT_MENU_SELECT, wuss_EVENT_MENU_CLOSED,
 * wuss_EVENT_PRE_SUBMENU_OPEN, wuss_EVENT_ICON (reserved for a future
 * shared/dock element; nothing emits it yet), wuss_EVENT_MESSAGE (a
 * broadcast message), wuss_EVENT_MESSAGE_BOUNCED.
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
     * always reflects the click/drag that raised it; a wheel turn over a
     * slider raises MOVE with button 0); meaningless for every
     * other icon type. A wuss_ICON_TYPE_WRITABLE also raises this, with
     * button 0 and action wuss_MOUSE_UP, after every edit to its text. In
     * the task view (window == NULL) this is instead a click or drag on
     * one of the caller's icon bar icons; read iconbar_icon, not icon. */
    struct
    {
      wuss_icon_t        *icon;
      wuss_mouse_action_t action;
      wuss_button_t       button;
      int                 value;
    }
    icon;

    /** wuss_EVENT_ICON in the task view (window == NULL): a click or drag
     * on one of this task's icon bar icons. action is DOWN/UP/MOVE; button
     * is a set of wuss_button_t flags, so test it with '&' rather than
     * comparing for equality. */
    struct
    {
      wuss_iconbar_icon_t *icon;
      wuss_mouse_action_t  action;
      wuss_button_t        button;
    }
    iconbar_icon;

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

    /** wuss_EVENT_MENU_CLOSED: picked is non-zero when the chain closed for
     * a SELECT pick, so a wuss_EVENT_MENU_SELECT follows at once; zero when
     * wuss abandoned it (a click outside, another wuss_menu_open). */
    struct
    {
      int picked;
    }
    menu_closed;

    /** wuss_EVENT_CLOSE_ADJUST: modifiers is the set of
     * wuss_key_modifiers_t flags held at the press (see
     * wuss_set_modifiers). */
    struct
    {
      wuss_key_modifiers_t modifiers;
    }
    close_adjust;

    /** wuss_EVENT_KEY / wuss_EVENT_KEY_UP: code is a Unicode codepoint or
     * a wuss_KEY_* constant; modifiers is a set of wuss_key_modifiers_t
     * flags, so test it with '&'. */
    struct
    {
      int                  code;
      wuss_key_modifiers_t modifiers;
    }
    key;

    /** wuss_EVENT_MESSAGE / wuss_EVENT_MESSAGE_BOUNCED: the message itself;
     * see wuss/message.h. For a BOUNCED event this is the original message
     * as sent, unmodified. */
    const wuss_message_t *message;

    /** wuss_EVENT_DRAG_END: drop is the window under the pointer when the
     * drag ended (NULL over bare backdrop, and always NULL when cancelled),
     * drop_icon is the icon bar icon under the pointer (NULL if none, if
     * cancelled, or without the icon bar; drop is then the bar's window),
     * point is the release/cancel position in screen space, cancelled is
     * non-zero if Escape ended the drag rather than a MOUSE_UP, button is
     * the button that started the drag (SELECT or ADJUST). */
    struct
    {
      wuss_window_t       *drop;
      wuss_iconbar_icon_t *drop_icon;
      point_t              point;
      int                  cancelled;
      wuss_button_t        button;
    }
    drag_end;

    /* wuss_EVENT_OPEN, wuss_EVENT_SHOW, wuss_EVENT_PRE_CLOSE,
     * wuss_EVENT_CLOSE, wuss_EVENT_IDLE, wuss_EVENT_QUIT,
     * wuss_EVENT_PALETTE, wuss_EVENT_GAIN_FOCUS and wuss_EVENT_LOSE_FOCUS
     * carry no data. */
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

  /** Optional name, for debugging/tracing and to locate the task's private
   *  icon pool (see \ref wuss_task_iconpool_load_resource); borrowed, not
   *  copied. */
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

/**
 * Bring the windows of every child of \p parent (see wuss_set_task_parent)
 * to the front of their stacks.
 *
 * \param[in] wuss   Window manager.
 * \param[in] parent Task whose children to raise.
 * \return Non-zero if \p parent has any child task, else zero.
 */
int wuss_task_raise_children(wuss_t *wuss, wuss_task_t *parent);

/**
 * Close every window of \p task via wuss_window_try_close, so each task gets
 * its wuss_EVENT_PRE_CLOSE veto. Stops at the first veto, leaving the
 * remaining windows open. A task with autoclose set is destroyed when its
 * last window closes.
 *
 * \param[in] wuss Window manager.
 * \param[in] task Task whose windows to close.
 * \return \ref result_OK if every window closed, else the result that
 *         vetoed.
 */
result_t wuss_task_close_windows(wuss_t *wuss, wuss_task_t *task);

/**
 * Hide or show every window of \p task and of its child tasks (see
 * wuss_set_task_parent), so an app is out of the way until revealed.
 *
 * Hiding hides each window that is visible, as wuss_window_set_hidden does,
 * and remembers which it hid. Showing reveals only those: a window that was
 * already hidden for its own reasons (a borrowed dialogue, say) stays so.
 * Revealing fires wuss_EVENT_PRE_SHOW per window, which a handler may leave
 * unrevealed. Windows created while the task is hidden are not affected.
 *
 * \param[in] wuss   Window manager.
 * \param[in] task   Task whose windows, and its children's, to hide or show.
 * \param[in] hidden Non-zero to hide, zero to show.
 * \return \ref result_OK, or the first non-OK result from a
 *         wuss_EVENT_PRE_SHOW handler (the remaining windows are still
 *         processed).
 */
result_t wuss_task_set_hidden(wuss_t *wuss, wuss_task_t *task, int hidden);

/**
 * Whether \p task or any of its child tasks has a window that
 * wuss_task_set_hidden would act on: one that is visible, or that it hid. A
 * window hidden in its own right (a borrowed dialogue, say) does not count.
 *
 * \param[in] wuss Window manager.
 * \param[in] task Task to test, with its children.
 * \return Non-zero if there is such a window, else zero.
 */
int wuss_task_has_windows(wuss_t *wuss, wuss_task_t *task);

/**
 * Bring every window of the app that \p task belongs to (its root task: the
 * parent, else itself) to the front of the regular windows, keeping their
 * order, and focus the frontmost.
 *
 * \param[in] wuss Window manager.
 * \param[in] task Any task of the app.
 * \return Non-zero if a window took the focus, else zero.
 */
int wuss_task_front(wuss_t *wuss, wuss_task_t *task);

/**
 * Fetch the root task of the app that \p task belongs to: its parent, else
 * itself. Destroying it ends the whole app.
 *
 * \param[in] task Any task of the app.
 * \return The root task. Never NULL.
 */
wuss_task_t *wuss_task_get_root(const wuss_task_t *task);

/**
 * Cycle the focus through the windows of \p task's app. A positive \p step
 * brings the rearmost window to the front; otherwise the front window goes
 * to the back and the next one comes forward. Only visible focusable regular
 * windows take part.
 *
 * \param[in] wuss Window manager.
 * \param[in] task Any task of the app.
 * \param[in] step Positive for next, else previous.
 * \return Non-zero if the app has two or more such windows, else zero.
 */
int wuss_task_cycle_windows(wuss_t *wuss, wuss_task_t *task, int step);

/**
 * Cycle through the apps: a positive \p step raises the rearmost app (see
 * wuss_task_front); otherwise the front app goes behind the rest.
 *
 * \param[in] wuss Window manager.
 * \param[in] step Positive for next, else previous.
 * \return Non-zero if there is more than one app to cycle, else zero.
 */
int wuss_cycle_tasks(wuss_t *wuss, int step);

/**
 * Hide the app that \p task belongs to, as wuss_task_set_hidden does for its
 * root task, then focus the frontmost visible window.
 *
 * \param[in] wuss Window manager.
 * \param[in] task Any task of the app.
 * \return As wuss_task_set_hidden.
 */
result_t wuss_task_hide_app(wuss_t *wuss, wuss_task_t *task);

/**
 * Hide every app that has a window on screen except the one \p task belongs
 * to. Apps with only back-stack windows (the icon bar) are left alone.
 *
 * \param[in] wuss Window manager.
 * \param[in] task Any task of the app to keep.
 * \return The first non-OK result from wuss_task_set_hidden, else \ref
 *         result_OK.
 */
result_t wuss_task_hide_others(wuss_t *wuss, wuss_task_t *task);

/**
 * Reveal every window wuss_task_set_hidden hid, in every app, and focus the
 * frontmost window if nothing has the focus.
 *
 * \param[in] wuss Window manager.
 * \return The first non-OK result from wuss_task_set_hidden, else \ref
 *         result_OK.
 */
result_t wuss_task_show_all(wuss_t *wuss);

/**
 * Make every task created from now on a child of \p parent: destroying \p
 * parent (wuss_task_destroy) first destroys its children, each firing its
 * own wuss_EVENT_QUIT. Lets a launcher task start tasks it did not create
 * itself and still shut them all down. Do not destroy a parent from inside a
 * handler while wuss is walking the task list (wuss_idle, wuss_set_palette):
 * its children vanish from under the walk. A child that self-destructs
 * earlier (autoclose) just leaves. Pass NULL to stop; the setting is not
 * scoped, so pair every set with a clear.
 *
 * \param[in] wuss   Window manager.
 * \param[in] parent Task to parent new tasks to, or NULL for none.
 */
void wuss_set_task_parent(wuss_t *wuss, wuss_task_t *parent);

#ifdef WUSS_ICONS

/* ----------------------------------------------------------------------- */

/**
 * Load a private icon pool for \p task from a directory of PNGs, as \ref
 * wuss_iconpool_load does for the shared pool (same decoding, RLE
 * compression and name-sorted entries). A task's icons -- those in its own
 * windows -- and \ref wuss_task_iconpool_find look a name up here first and
 * fall back to the shared pool, so a task can ship its own artwork and
 * shadow shared names. Calling this again replaces the previous private
 * pool; \ref wuss_task_destroy frees it, after the task's windows.
 *
 * \param[in] task Task to give the pool to.
 * \param[in] dir  Directory to scan for "*.png".
 * \return As \ref wuss_iconpool_load.
 */
result_t wuss_task_iconpool_load(wuss_task_t *task, const char *dir);

/**
 * Convenience wrapper for \ref wuss_task_iconpool_load: loads
 * "<resources>/resources/wuss/tasks/<name>/iconpool", where <name> is the
 * task's name (\ref wuss_task_desc::name). A missing directory is not an
 * error: the pool is just empty.
 *
 * \param[in] task      Task to give the pool to; must have a name.
 * \param[in] resources Resource root, as returned by \ref
 *                      wuss_get_resources.
 * \return As \ref wuss_iconpool_load, or \ref result_BAD_ARG if \p task has
 *         no name.
 */
result_t wuss_task_iconpool_load_resource(wuss_task_t *task,
                                          const char  *resources);

/**
 * Find an icon by name: \p task's private pool first, then the shared pool.
 *
 * \param[in] task Task whose private pool to search first.
 * \param[in] name Entry name: a loaded file's leafname without ".png".
 * \return The (RLE-compressed) bitmap, owned by the window manager and valid
 *         until the pool holding it is reloaded or freed, or NULL if neither
 *         pool has \p name.
 */
const bitmap_t *wuss_task_iconpool_find(const wuss_task_t *task,
                                        const char        *name);

#endif /* WUSS_ICONS */

#ifdef __cplusplus
}
#endif

#endif /* WUSS_TASK_H */
