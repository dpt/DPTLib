/* wuss/component/proginfo.h -- a shared "program information" dialogue */

/**
 * \file proginfo.h
 *
 * A shared wuss component: the RISC OS "Info" / Toolbox ProgInfo dialogue in
 * miniature -- a small window of Name / Purpose / Author / Version rows,
 * built once and reusable by every task that wants an "Info" row on its
 * Menu-button pop-up, rather than each task carrying its own copy.
 *
 * wuss_proginfo_t composes a wuss_info_t (the label:value grid and its
 * window) with a wuss_dialogue_t (fillout-on-show dispatch, via
 * wuss_dialogue_create_on_window). A caller that wants to show it points its
 * "Info" wuss_menu_item_t::window at wuss_proginfo_window(), then calls
 * wuss_proginfo_set_desc() with its own field values before opening the menu
 * chain. The stored desc is only applied -- resizing the window and
 * rewriting its rows via wuss_info_set_rows -- when the dialogue is actually
 * revealed, from the home task's wuss_EVENT_PRE_SHOW handler via
 * wuss_dialogue_handle_pre_show(); see wuss_proginfo_handle_pre_show(), a
 * thin cap over that call. This way the same window is correctly sized for
 * whichever caller most recently asked to show it, even though its text
 * differs per caller.
 *
 * The dialogue lives on one "home" task -- normally whichever task's caller
 * created it first, or a dedicated task if several unrelated tasks share it
 * -- and every wuss_EVENT_PRE_SHOW for wuss_proginfo_window() must reach
 * that task's handler and be forwarded to wuss_proginfo_handle_pre_show().
 * As for wuss_info_t, that task must not be an autoclose task and must
 * outlive the handle, and the dialogue must outlive every menu chain that
 * borrows it as a wuss_menu_item_t::window.
 *
 * Built only when WUSS_COMPONENTS is defined (which implies WUSS_MENUS).
 */

#ifndef WUSS_COMPONENT_PROGINFO_H
#define WUSS_COMPONENT_PROGINFO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "wuss/task.h"
#include "wuss/window.h"

#include "wuss/component/info.h"

/* ----------------------------------------------------------------------- */

/**
 * The rows of a program-information dialogue. Every field is borrowed and
 * copied; a NULL field is left out entirely, so a desc with only \c name set
 * yields a one-row dialogue.
 */
typedef struct wuss_proginfo_desc
{
  const char *name;    /**< Program name; row "Name". */
  const char *purpose; /**< One-line description; row "Purpose". */
  const char *author;  /**< Author / copyright line; row "Author". */
  const char *version; /**< Version string, e.g. "1.23 (14 Feb 2026)"; row
                            "Version". */
}
wuss_proginfo_desc_t;

/**
 * Opaque handle: owns the dialogue's window, wuss_info_t and
 * wuss_dialogue_t.
 */
typedef struct wuss_proginfo wuss_proginfo_t;

/* ----------------------------------------------------------------------- */

/**
 * Build a program-information dialogue: a hidden window on \p task, laid out
 * with one label row per non-NULL field of \p desc, in the order Name,
 * Purpose, Author, Version. \p desc need only be some starting size -- e.g.
 * the first caller's own values -- since wuss_proginfo_set_desc relays out
 * and resizes the window for whoever is about to show it next.
 *
 * \param[out] out   Filled with the new handle on success, untouched on
 *                   failure.
 * \param[in]  task  Task the dialogue window is created on ("home task"; see
 *                   \ref proginfo.h). Must not be an autoclose task and must
 *                   outlive the handle.
 * \param[in]  desc  Initial field values; each borrowed and copied. At least
 *                   \c name must be non-NULL.
 * \return \ref result_OK, \ref result_OOM, \ref result_NULL_ARG if \p out,
 *         \p task, \p desc or \c desc->name is NULL, or a wuss_info_create /
 *         wuss_dialogue_create_on_window code.
 */
result_t wuss_proginfo_create(wuss_proginfo_t           **out,
                              wuss_task_t                *task,
                              const wuss_proginfo_desc_t *desc);

/**
 * Free a program-information dialogue: closes its window and frees every
 * string in it. Safe to pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_proginfo_destroy(wuss_proginfo_t *doomed);

/**
 * Point the dialogue at \p desc's field values, to be applied -- resizing
 * the window and rewriting its rows -- the next time it is revealed. Call
 * this before opening the menu chain that shows wuss_proginfo_window(), so
 * whichever caller last asked gets its own values.
 *
 * \param[in] pi   Handle.
 * \param[in] desc Field values; each borrowed and copied when applied. At
 *                 least \c name must be non-NULL.
 */
void wuss_proginfo_set_desc(wuss_proginfo_t            *pi,
                            const wuss_proginfo_desc_t *desc);

/**
 * Handle a wuss_EVENT_PRE_SHOW for wuss_proginfo_window() from the home
 * task's own handler: applies the desc last passed to
 * wuss_proginfo_set_desc, as wuss_dialogue_handle_pre_show.
 *
 * \param[in] pi Handle.
 * \return \ref result_OK, or a wuss_info_set_rows code.
 */
result_t wuss_proginfo_handle_pre_show(wuss_proginfo_t *pi);

/**
 * The dialogue's window, for use as a wuss_menu_item_t::window. Borrowed;
 * valid until wuss_proginfo_destroy. NULL only if \p pi is NULL.
 *
 * \param[in] pi Handle.
 * \return The window, or NULL.
 */
wuss_window_t *wuss_proginfo_window(const wuss_proginfo_t *pi);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_PROGINFO_H */
