/* wuss/component/proginfo.h -- a shared "program information" dialogue */

/**
 * \file proginfo.h
 *
 * A shared wuss component: the RISC OS "Info" / Toolbox ProgInfo dialogue in
 * miniature -- a small window of Name / Purpose / Author / Version rows,
 * built once and reusable by every task that wants an "Info" row on its
 * Menu-button pop-up, rather than each task carrying its own copy.
 *
 * One process-wide singleton -- no per-task instances, no create/destroy --
 * composing a wuss_info_t (the label:value grid and its window) with a
 * wuss_dialogue_t (fillout-on-show dispatch, via
 * wuss_dialogue_create_on_window). A caller that wants to show it points its
 * "Info" wuss_menu_item_t::window at wuss_proginfo_window(task), then calls
 * wuss_proginfo_set_desc() with its own field values before opening the menu
 * chain. wuss_proginfo_window() rebuilds the dialogue -- closing and
 * recreating its window -- whenever \p task differs from whichever task it
 * was last built on, since the window belongs to one "home" task at a time.
 * The stored desc is only applied -- resizing the window and rewriting its
 * rows via wuss_info_set_rows -- when the dialogue is actually revealed,
 * from the home task's wuss_EVENT_PRE_SHOW handler via
 * wuss_dialogue_handle_pre_show(); see wuss_proginfo_handle_pre_show(), a
 * thin cap over that call. This way the same window is correctly sized for
 * whichever caller most recently asked to show it, even though its text and
 * home task differ per caller.
 *
 * Every wuss_EVENT_PRE_SHOW for wuss_proginfo_window() must reach the
 * current home task's handler and be forwarded to
 * wuss_proginfo_handle_pre_show(). As for wuss_info_t, the home task must
 * not be an autoclose task and must outlive the handle, and the dialogue
 * must outlive every menu chain that borrows it as a
 * wuss_menu_item_t::window.
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

/* ----------------------------------------------------------------------- */

/**
 * The singleton dialogue's window, for use as a wuss_menu_item_t::window.
 * Rebuilds the dialogue -- closing and recreating its window -- if \p task
 * differs from whichever task it was last built on. Borrowed; valid until
 * the next call with a different \p task.
 *
 * \param[in] task Task the dialogue window is (re)created on if needed
 *                 ("home task"; see \ref proginfo.h). Must not be an
 *                 autoclose task and must outlive the handle.
 * \return The window, or NULL if \p task is NULL or on allocation failure.
 */
wuss_window_t *wuss_proginfo_window(wuss_task_t *task);

/**
 * Point the singleton dialogue at \p desc's field values, to be applied --
 * resizing the window and rewriting its rows -- the next time it is
 * revealed. Call this before opening the menu chain that shows
 * wuss_proginfo_window(), so whichever caller last asked gets its own
 * values.
 *
 * \param[in] desc Field values; each borrowed and copied when applied. At
 *                 least \c name must be non-NULL.
 */
void wuss_proginfo_set_desc(const wuss_proginfo_desc_t *desc);

/**
 * Handle a wuss_EVENT_PRE_SHOW for wuss_proginfo_window() from the current
 * home task's own handler: applies the desc last passed to
 * wuss_proginfo_set_desc, as wuss_dialogue_handle_pre_show.
 *
 * \return \ref result_OK, or a wuss_info_set_rows code.
 */
result_t wuss_proginfo_handle_pre_show(void);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_PROGINFO_H */
