/* wuss/component/info.h -- a label:value grid dialogue */

/**
 * \file info.h
 *
 * A shared wuss component: a small fixed window laid out as a grid of
 * label:value rows -- the base of the RISC OS "Info" / Toolbox ProgInfo
 * dialogue, and of any dialogue that is just a column of named readouts.
 *
 * A wuss_info owns one window (titlebar caption only -- no close, back,
 * toggle, scrollbars or resize), created hidden on a task the caller passes
 * in and laid out from an array of wuss_info_row_t: each row is a label
 * right-justified in a left column and a value centred in a sunken display
 * field beside it. The window is sized from the task's font metrics and the
 * widest label and value.
 *
 * It is meant to be used as a wuss_menu_item_t::window -- point an "Info"
 * menu row at wuss_info_window() and wuss shows it where a submenu would
 * open. A task can equally wuss_window_set_hidden() it directly.
 *
 * The window is created wuss_WINDOW_NO_CLOSE and stays on the caller's task
 * until wuss_info_destroy closes it. That task must therefore not be an
 * autoclose task -- its window list would never empty -- and must outlive
 * the handle. As for any borrowed wuss_menu_item_t::window, the dialogue
 * must also outlive every menu chain that references it: close the chain
 * (wuss_menu_close) before wuss_info_destroy.
 *
 * Built only when WUSS_COMPONENTS is defined (which implies WUSS_MENUS).
 */

#ifndef WUSS_COMPONENT_INFO_H
#define WUSS_COMPONENT_INFO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "wuss/task.h"
#include "wuss/window.h"

/* ----------------------------------------------------------------------- */

/**
 * One row of an info dialogue: a label and its value. Both are borrowed and
 * copied by wuss_info_create.
 */
typedef struct wuss_info_row
{
  const char *label; /**< Left-column caption, right-justified. */
  const char *value; /**< Right-column readout, shown in a sunken field. */
}
wuss_info_row_t;

/** Opaque handle: owns the dialogue window and the strings in it. */
typedef struct wuss_info wuss_info_t;

/* ----------------------------------------------------------------------- */

/**
 * Build a label:value grid dialogue: a hidden window on \p task, one row per
 * entry in \p rows.
 *
 * The window's on-screen size is derived from \p task's font metrics and the
 * text in \p rows; it is created with wuss_WINDOW_HIDDEN, so nothing shows
 * until the caller reveals it (directly, or via a wuss_menu_item_t::window
 * that names wuss_info_window()).
 *
 * \param[out] out    Filled with the new handle on success, untouched on
 *                    failure.
 * \param[in]  task   Task the dialogue window is created on. Must not be an
 *                    autoclose task and must outlive the handle.
 * \param[in]  title  Titlebar caption; borrowed and copied by
 *                    wuss_window_create.
 * \param[in]  rows   Row array; each label and value is borrowed and copied.
 * \param[in]  nrows  Number of rows; must be >= 1.
 * \return \ref result_OK, \ref result_OOM, \ref result_NULL_ARG if \p out,
 *         \p task, \p title or \p rows is NULL, \ref result_BAD_ARG if \p
 *         nrows is < 1, or a wuss_window_create / wuss_icon_create code.
 */
result_t wuss_info_create(wuss_info_t          **out,
                          wuss_task_t           *task,
                          const char            *title,
                          const wuss_info_row_t *rows,
                          int                    nrows);

/**
 * Free an info dialogue: closes its window and frees every string in it.
 * Safe to pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_info_destroy(wuss_info_t *doomed);

/**
 * The dialogue's window, for use as a wuss_menu_item_t::window or to show
 * and hide directly. Borrowed; valid until wuss_info_destroy. NULL only if
 * \p info is NULL.
 *
 * \param[in] info Handle.
 * \return The window, or NULL.
 */
wuss_window_t *wuss_info_window(const wuss_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_INFO_H */
