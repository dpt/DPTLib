/* wuss/component/proginfo.h -- a "program information" standard dialogue */

/**
 * \file proginfo.h
 *
 * A shared wuss component: the RISC OS "Info" / Toolbox ProgInfo dialogue in
 * miniature -- a small fixed window of Name / Purpose / Author / Version
 * rows a task fills in once, then hangs off its Menu-button pop-up.
 *
 * A wuss_proginfo owns one window (titlebar caption only -- no close, back,
 * toggle, scrollbars or resize), created hidden on a task the caller passes
 * in and laid out from a wuss_proginfo_desc_t: one row per non-NULL field,
 * the field name right-justified in a left column and its value centred in a
 * sunken display field beside it.
 *
 * It is meant to be used as a wuss_menu_item_t::window -- point an "Info"
 * menu row at wuss_proginfo_window() and wuss shows it where a submenu would
 * open. A task can equally wuss_window_set_hidden() it directly.
 *
 * The window is created wuss_WINDOW_NO_CLOSE and stays on the caller's task
 * until wuss_proginfo_destroy closes it. That task must therefore not be an
 * autoclose task -- its window list would never empty -- and must outlive
 * the handle. As for any borrowed wuss_menu_item_t::window, the dialogue
 * must also outlive every menu chain that references it: close the chain
 * (wuss_menu_close) before wuss_proginfo_destroy.
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

/* ----------------------------------------------------------------------- */

/**
 * The rows of a program-information dialogue. Every field is borrowed and
 * copied; a NULL field is left out entirely, so a zero-initialised desc with
 * only \c name set yields a one-row dialogue.
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

/** Opaque handle: owns the dialogue window and the label strings in it. */
typedef struct wuss_proginfo wuss_proginfo_t;

/* ----------------------------------------------------------------------- */

/**
 * Build a program-information dialogue: a hidden window on \p task, laid out
 * from \p desc with one label row per non-NULL field.
 *
 * The window's on-screen size is derived from \p task's font metrics and the
 * text in \p desc; it is created with wuss_WINDOW_HIDDEN, so nothing shows
 * until the caller reveals it (directly, or via a wuss_menu_item_t::window
 * that names wuss_proginfo_window()).
 *
 * \param[out] out   Filled with the new handle on success, untouched on
 *                   failure.
 * \param[in]  task  Task the dialogue window is created on. Must not be an
 *                   autoclose task and must outlive the handle.
 * \param[in]  desc  Field values; each borrowed and copied. At least \c name
 *                   must be non-NULL.
 * \return \ref result_OK, \ref result_OOM, \ref result_NULL_ARG if \p out,
 *         \p task, \p desc or \c desc->name is NULL, or a wuss_window_create
 *         / wuss_icon_create code.
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
 * The dialogue's window, for use as a wuss_menu_item_t::window or to show
 * and hide directly. Borrowed; valid until wuss_proginfo_destroy. NULL only
 * if \p pi is NULL.
 *
 * \param[in] pi Handle.
 * \return The window, or NULL.
 */
wuss_window_t *wuss_proginfo_window(const wuss_proginfo_t *pi);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_PROGINFO_H */
