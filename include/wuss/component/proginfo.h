/* wuss/component/proginfo.h -- a "program information" standard dialogue */

/**
 * \file proginfo.h
 *
 * A shared wuss component: the RISC OS "Info" / Toolbox ProgInfo dialogue in
 * miniature -- a small fixed window of Name / Purpose / Author / Version
 * rows a task fills in once, then hangs off its Menu-button pop-up.
 *
 * This is a thin cap over \ref info.h: wuss_proginfo_create maps the
 * non-NULL fields of a wuss_proginfo_desc_t to wuss_info_row_t rows and
 * calls wuss_info_create. wuss_proginfo_t is wuss_info_t, and the destroy /
 * window calls forward straight through, so all of info.h's lifetime rules
 * apply verbatim: the window is created without wuss_WINDOW_CLOSE, stays on
 * the caller's task until wuss_proginfo_destroy, that task must not be an
 * autoclose task and must outlive the handle, and the dialogue must outlive
 * every menu chain that borrows it as a wuss_menu_item_t::window.
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

/**
 * Opaque handle: a wuss_info_t. Owns the dialogue window and its strings.
 */
typedef wuss_info_t wuss_proginfo_t;

/* ----------------------------------------------------------------------- */

/**
 * Build a program-information dialogue: a hidden window on \p task, laid out
 * with one label row per non-NULL field of \p desc, in the order Name,
 * Purpose, Author, Version.
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
 *         \p task, \p desc or \c desc->name is NULL, or a wuss_info_create
 *         code.
 */
result_t wuss_proginfo_create(wuss_proginfo_t           **out,
                              wuss_task_t                *task,
                              const wuss_proginfo_desc_t *desc);

/**
 * Free a program-information dialogue: closes its window and frees every
 * string in it. Safe to pass NULL. An alias for wuss_info_destroy.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
#define wuss_proginfo_destroy wuss_info_destroy

/**
 * The dialogue's window, for use as a wuss_menu_item_t::window or to show
 * and hide directly. Borrowed; valid until wuss_proginfo_destroy. NULL only
 * if \p pi is NULL. An alias for wuss_info_window.
 *
 * \param[in] pi Handle.
 * \return The window, or NULL.
 */
#define wuss_proginfo_window wuss_info_window

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_PROGINFO_H */
