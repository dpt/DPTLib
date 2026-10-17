/* wuss/component/dcs.h -- Discard/Cancel/Save dialogue */

/**
 * \file dcs.h
 *
 * A wuss component: the RISC OS "Discard/Cancel/Save" dialogue, put up when
 * a document with unsaved changes is about to be closed. It shows a one-line
 * message above three buttons -- Discard on the left, Cancel and Save on the
 * right, Save being the default -- and reports the user's choice through a
 * callback. Built on top of wuss_dialogue, as with wuss_saveas.
 *
 * The dialogue runs on a task of its own (created by wuss_dcs_create and
 * freed by wuss_dcs_destroy), so callers forward nothing. wuss_dcs_open
 * shows it as a menu of its own, centred on the pointer, and returns at
 * once: unlike a RISC OS Wimp dialogue there is no blocking loop, so the
 * caller carries on from its wuss_dcs_choice_fn_t.
 *
 * Exactly one choice is reported for each wuss_dcs_open. A click on any
 * button, ADJUST included, reports it and dismisses the dialogue. ESCAPE,
 * and the dialogue being closed some other way (a click outside it, another
 * menu opening), report wuss_DCS_CANCEL.
 *
 * Built only when WUSS_COMPONENTS is defined.
 */

#ifndef WUSS_COMPONENT_DCS_H
#define WUSS_COMPONENT_DCS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Opaque handle: owns the dialogue, its icons and its task. */
typedef struct wuss_dcs wuss_dcs_t;

/** Which buttons the dialogue has. */
typedef enum
{
  wuss_DCS_KIND_DCS,  /**< Discard, Cancel and Save; Save is the default. */
  wuss_DCS_KIND_QUIT  /**< Discard and Cancel; Cancel is the default. Never
                       *   reports wuss_DCS_SAVE. */
}
wuss_dcs_kind_t;

/** The user's choice. */
typedef enum
{
  wuss_DCS_DISCARD, /**< Throw the changes away. */
  wuss_DCS_CANCEL,  /**< Abandon whatever prompted the dialogue. */
  wuss_DCS_SAVE     /**< Save the changes. */
}
wuss_dcs_choice_t;

/**
 * Choice callback: the user has chosen. Called once per wuss_dcs_open, after
 * the dialogue has been dismissed. The dialogue may be destroyed from here,
 * or reopened.
 *
 * \param[in] choice What the user chose.
 * \param[in] opaque As passed to wuss_dcs_create.
 */
typedef void (wuss_dcs_choice_fn_t)(wuss_dcs_choice_t choice, void *opaque);

/* ----------------------------------------------------------------------- */

/**
 * Create a Discard/Cancel/Save dialogue: a hidden window on a task of its
 * own, sized to fit \p message and the buttons.
 *
 * \param[out] out     Filled with the new handle on success, untouched on
 *                     failure.
 * \param[in]  wuss    Window manager.
 * \param[in]  kind    Which buttons to have. The Quit variant is for "N
 *                     documents have unsaved changes" on quitting, where
 *                     there is no single document to save; pass the count in
 *                     \p message, see wuss_dcs_set_message.
 * \param[in]  title   Titlebar caption, or NULL for none; copied.
 * \param[in]  message The question, on one line; copied.
 * \param[in]  fn      Called with the user's choice.
 * \param[in]  opaque  Passed back to \p fn.
 * \return \ref result_OK, \ref result_OOM, \ref result_NULL_ARG if \p out,
 *         \p wuss, \p message or \p fn is NULL, or a wuss_task_create /
 *         wuss_window_create / wuss_icon_create code.
 */
result_t wuss_dcs_create(wuss_dcs_t           **out,
                         wuss_t                *wuss,
                         wuss_dcs_kind_t        kind,
                         const char            *title,
                         const char            *message,
                         wuss_dcs_choice_fn_t  *fn,
                         void                  *opaque);

/**
 * Free a dialogue: dismisses it without reporting a choice, then closes its
 * window and task. Safe to pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_dcs_destroy(wuss_dcs_t *doomed);

/**
 * Change the message, resizing the dialogue to fit. Call it while the
 * dialogue is hidden, before wuss_dcs_open: a dialogue already shown is
 * resized about its top-left, not recentred. The usual use is to format a
 * count or a document name into the question just before showing it.
 *
 * \param[in] dcs     Handle.
 * \param[in] message The question, on one line; copied.
 * \return \ref result_OK, \ref result_OOM, \ref result_NULL_ARG if \p dcs or
 *         \p message is NULL, or a wuss_window_resize / wuss_icon_create
 *         code. On failure the old message may be gone; set it again.
 */
result_t wuss_dcs_set_message(wuss_dcs_t *dcs, const char *message);

/**
 * Show the dialogue, centred on the pointer, as a menu of its own. Any menu
 * chain already open is closed first.
 *
 * \param[in] dcs Handle.
 * \return \ref result_OK, or a wuss_menu_open_window code, in which case the
 *         dialogue is not shown and no choice will be reported.
 */
result_t wuss_dcs_open(wuss_dcs_t *dcs);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_DCS_H */
