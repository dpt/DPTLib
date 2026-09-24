/* wuss/component/dialogue.h -- interactive dialogue base class */

/**
 * \file dialogue.h
 *
 * A shared wuss component: the window-and-button shell behind an interactive
 * dialogue (a Configure box, a form with OK/Cancel), so a task need not
 * hand-roll its own window creation, fillout-on-show wiring and per-button
 * event dispatch.
 *
 * A wuss_dialogue owns one hidden window on a task the caller passes in. The
 * caller builds its own content (icons, layout) on that window after
 * wuss_dialogue_create returns, then registers a small table of actions --
 * an icon and a callback -- with wuss_dialogue_set_actions. From the task's
 * own wuss_EVENT_ICON handler, a click on any registered icon is dispatched
 * to that action's callback via wuss_dialogue_handle_icon; every other icon
 * event (e.g. a slider drag) is left to the caller to handle first, as
 * wuss_dialogue_handle_icon only fires on wuss_MOUSE_UP against a registered
 * icon and returns 0 (not consumed) otherwise. From the task's
 * wuss_EVENT_PRE_SHOW handler, wuss_dialogue_handle_pre_show calls the
 * fillout callback (if any) to resync the dialogue's icons to live state
 * before it is revealed.
 *
 * This component does not know about menus: a dialogue shown as a
 * wuss_menu_item_t::window leaf is dismissed by the task's own action
 * callback calling wuss_menu_close, exactly as it already must decide
 * whether a Select or Adjust click on Cancel/Apply/OK dismisses at all (the
 * RISC OS convention: Select dismisses, Adjust applies/resets and leaves the
 * dialogue open). A standalone dialogue dismisses itself with
 * wuss_dialogue_hide.
 *
 * Built only when WUSS_COMPONENTS is defined (which implies WUSS_MENUS).
 */

#ifndef WUSS_COMPONENT_DIALOGUE_H
#define WUSS_COMPONENT_DIALOGUE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/size.h"

#include "wuss/icon.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Opaque handle: owns the dialogue window and its action table. */
typedef struct wuss_dialogue wuss_dialogue_t;

/**
 * Fillout callback: resync the dialogue's icons to live state. Called by
 * wuss_dialogue_handle_pre_show, and so on every reveal of the dialogue.
 *
 * \param[in] opaque As passed to wuss_dialogue_create.
 * \return \ref result_OK to allow the reveal, else a code that vetoes it
 *         (see wuss_EVENT_PRE_SHOW).
 */
typedef result_t (wuss_dialogue_fillout_fn_t)(void *opaque);

/**
 * Action callback: a registered icon was clicked. Called by
 * wuss_dialogue_handle_icon.
 *
 * \param[in] opaque As passed to wuss_dialogue_create.
 * \param[in] button Button(s) released, as wuss_event_t::data::icon::button.
 *                   Test with '&': RISC OS convention is Select dismisses,
 *                   Adjust applies/resets without dismissing.
 * \return \ref result_OK, or an appropriate result code; propagated back
 *         through wuss_dialogue_handle_icon to the task's own handler.
 */
typedef result_t (wuss_dialogue_action_fn_t)(void          *opaque,
                                             wuss_button_t  button);

/** One entry in a dialogue's action table: an icon and the callback a click
 *  on it dispatches to. */
typedef struct wuss_dialogue_action
{
  wuss_icon_t                *icon; /**< Icon this action fires for. */
  wuss_dialogue_action_fn_t  *fn;   /**< Callback; never NULL in a
                                     *   registered entry. */
}
wuss_dialogue_action_t;

/**
 * Upper bound on the number of entries wuss_dialogue_set_actions accepts.
 */
#define WUSS_DIALOGUE_MAX_ACTIONS 16

/* ----------------------------------------------------------------------- */

/**
 * Create a dialogue: a hidden window on \p task, sized \p size, titled \p
 * title. The caller builds its own icons on the returned window's
 * wuss_dialogue_window afterwards.
 *
 * \param[out] out     Filled with the new handle on success, untouched on
 *                     failure.
 * \param[in]  task    Task the dialogue window is created on. Must not be an
 *                     autoclose task and must outlive the handle.
 * \param[in]  size    Content size; also used as both \c doc and \c min_doc,
 *                     so the window never scrolls or resizes on its own.
 * \param[in]  title   Titlebar caption, or NULL for none; borrowed and
 *                     copied by wuss_window_create.
 * \param[in]  fillout Called by wuss_dialogue_handle_pre_show on every
 *                     reveal, or NULL for none.
 * \param[in]  opaque  Passed back to \p fillout and to every action
 *                     callback.
 * \return \ref result_OK on success, \ref result_OOM, \ref result_NULL_ARG
 *         if \p out or \p task is NULL, or a wuss_window_create code.
 */
result_t wuss_dialogue_create(wuss_dialogue_t           **out,
                              wuss_task_t                *task,
                              size2d_t                    size,
                              const char                 *title,
                              wuss_dialogue_fillout_fn_t *fillout,
                              void                       *opaque);

/**
 * Create a dialogue wrapping an already-created window, e.g. one owned and
 * laid out by another component such as wuss_info_t, instead of creating its
 * own. wuss_dialogue_destroy then leaves \p window alone -- the caller (or
 * the component that owns it) is responsible for closing it.
 *
 * Use this to give an existing window fillout/action-table dispatch without
 * duplicating its layout logic; see wuss/component/proginfo.h for the
 * motivating case of one dialogue shared and refilled by several callers.
 *
 * \param[out] out     Filled with the new handle on success, untouched on
 *                     failure.
 * \param[in]  wuss    The window's owning wuss instance.
 * \param[in]  window  Existing window; borrowed, not closed by
 *                     wuss_dialogue_destroy.
 * \param[in]  fillout Called by wuss_dialogue_handle_pre_show on every
 *                     reveal, or NULL for none.
 * \param[in]  opaque  Passed back to \p fillout and to every action
 *                     callback; see also wuss_dialogue_set_opaque.
 * \return \ref result_OK on success, \ref result_OOM, \ref result_NULL_ARG
 *         if \p out, \p wuss or \p window is NULL.
 */
result_t wuss_dialogue_create_on_window(wuss_dialogue_t           **out,
                                        wuss_t                     *wuss,
                                        wuss_window_t              *window,
                                        wuss_dialogue_fillout_fn_t *fillout,
                                        void                       *opaque);

/**
 * Free a dialogue: closes its window (and so the icons on it), unless it was
 * built with wuss_dialogue_create_on_window, in which case the window is
 * left for its owning component to close. Safe to pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_dialogue_destroy(wuss_dialogue_t *doomed);

/**
 * Change the opaque pointer passed to the fillout and action callbacks,
 * replacing the one given at creation. For a dialogue shared by several
 * callers (e.g. the wuss_proginfo singleton reused across tasks), call this
 * to retarget it at the caller about to show it, before revealing it.
 *
 * \param[in] dialogue Handle.
 * \param[in] opaque   New opaque pointer.
 */
void wuss_dialogue_set_opaque(wuss_dialogue_t *dialogue, void *opaque);

/**
 * Register the dialogue's action table, replacing any previous one. Does not
 * take ownership of the icons named in it -- they belong to the dialogue's
 * window, as created by the caller.
 *
 * \param[in] dialogue Handle.
 * \param[in] actions  Action table; copied.
 * \param[in] nactions Number of entries, 0..\ref WUSS_DIALOGUE_MAX_ACTIONS.
 * \return \ref result_OK on success, \ref result_BAD_ARG if \p nactions is
 *         out of range.
 */
result_t wuss_dialogue_set_actions(wuss_dialogue_t              *dialogue,
                                   const wuss_dialogue_action_t *actions,
                                   int                           nactions);

/**
 * Dispatch a wuss_EVENT_ICON from the task's own handler: if \p event is a
 * wuss_MOUSE_UP on an icon registered via wuss_dialogue_set_actions, calls
 * that action's callback and returns 1 (with *out_result set to its return
 * value). Otherwise returns 0 and leaves *out_result untouched, for the
 * caller to handle the event itself (e.g. a slider drag) or ignore it.
 *
 * \param[in]  dialogue   Handle.
 * \param[in]  event      The wuss_EVENT_ICON event.
 * \param[out] out_result Filled with the action callback's return value if
 *                        (and only if) this call returns 1.
 * \return 1 if the event was dispatched to a registered action, else 0.
 */
int wuss_dialogue_handle_icon(wuss_dialogue_t    *dialogue,
                              const wuss_event_t *event,
                              result_t           *out_result);

/**
 * Handle a wuss_EVENT_PRE_SHOW from the task's own handler: calls the
 * fillout callback passed to wuss_dialogue_create, if any.
 *
 * \param[in] dialogue Handle.
 * \return \ref result_OK if there is no fillout callback, else its return
 *         value.
 */
result_t wuss_dialogue_handle_pre_show(wuss_dialogue_t *dialogue);

/**
 * Hide the dialogue without destroying it, as wuss_window_set_hidden(window,
 * 1). For a standalone dialogue (not a wuss_menu_item_t::window leaf, which
 * dismisses via wuss_menu_close instead).
 *
 * \param[in] dialogue Handle.
 * \return \ref result_OK.
 */
result_t wuss_dialogue_hide(wuss_dialogue_t *dialogue);

/**
 * The dialogue's window, for building content on it, using it as a
 * wuss_menu_item_t::window, or showing/hiding directly. Borrowed; valid
 * until wuss_dialogue_destroy.
 *
 * \param[in] dialogue Handle.
 * \return The window, or NULL if \p dialogue is NULL.
 */
wuss_window_t *wuss_dialogue_window(const wuss_dialogue_t *dialogue);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_DIALOGUE_H */
