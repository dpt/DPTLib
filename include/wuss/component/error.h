/* wuss/component/error.h -- error report window */

/**
 * \file error.h
 *
 * A wuss component: the equivalent of RISC OS's Wimp_ReportError. It puts up
 * a message with an OK button and, optionally, a Cancel button, and reports
 * which was chosen through a callback.
 *
 * Unlike the Wimp's, the report is not modal. It is an ordinary window, and
 * there is only ever one of them: a singleton, created on the first
 * wuss_error_report and kept for the life of the wuss instance. The caller
 * carries on at once and the rest of the desktop stays usable.
 *
 * A report made while another is showing is queued, not dropped, and shown
 * once the earlier ones have been answered. Each report is answered exactly
 * once. The window has no close icon: RETURN activates OK and, with a Cancel
 * button, ESCAPE activates Cancel; with no Cancel button ESCAPE also
 * activates OK, as dismissing the only choice there is.
 *
 * The message is shown on one line and is not wrapped.
 *
 * Built only when WUSS_COMPONENTS is defined.
 */

#ifndef WUSS_COMPONENT_ERROR_H
#define WUSS_COMPONENT_ERROR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Which buttons a report has. */
typedef enum
{
  wuss_ERROR_BUTTONS_OK,       /**< OK only; it is the default. */
  wuss_ERROR_BUTTONS_OK_CANCEL /**< OK and Cancel; OK is the default. */
}
wuss_error_buttons_t;

/** The user's choice. */
typedef enum
{
  wuss_ERROR_OK,    /**< OK was pressed. */
  wuss_ERROR_CANCEL /**< Cancel was pressed or ESCAPE was typed. */
}
wuss_error_choice_t;

/**
 * Choice callback: the report has been answered. Called once per report,
 * after its window has moved on to the next queued report or been hidden. It
 * may call wuss_error_report.
 *
 * \param[in] choice What the user chose.
 * \param[in] opaque As passed to wuss_error_report.
 */
typedef void (wuss_error_choice_fn_t)(wuss_error_choice_t choice,
                                      void               *opaque);

/* ----------------------------------------------------------------------- */

/**
 * Report an error: show it now, or queue it behind those already showing.
 *
 * \param[in] wuss    Window manager.
 * \param[in] app_icon Name of the icon pool entry for the application the
 *                     report is from, drawn at the right of the message, or
 *                     NULL for none. A name the pool lacks is the same as
 *                     NULL; copied.
 * \param[in] title   Titlebar caption, or NULL for "Error"; copied.
 * \param[in] message The error, on one line; copied.
 * \param[in] buttons Which buttons to have.
 * \param[in] fn      Called with the user's choice, or NULL if the caller
 *                    does not care.
 * \param[in] opaque  Passed back to \p fn.
 * \return \ref result_OK, \ref result_OOM, \ref result_NULL_ARG if \p wuss
 *         or \p message is NULL, or a wuss_task_create / wuss_window_create
 *         / wuss_icon_create code. On failure nothing is queued and \p fn is
 *         not called.
 */
result_t wuss_error_report(wuss_t                *wuss,
                           const char            *app_icon,
                           const char            *title,
                           const char            *message,
                           wuss_error_buttons_t   buttons,
                           wuss_error_choice_fn_t *fn,
                           void                  *opaque);

/**
 * The number of reports waiting, the one showing included.
 *
 * \param[in] wuss Window manager.
 * \return The count, 0 if none, or if \p wuss has no error window.
 */
int wuss_error_pending(const wuss_t *wuss);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_ERROR_H */
