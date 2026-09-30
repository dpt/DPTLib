/* wuss/component/saveas.h -- RISC OS-style drag-and-drop Save As dialogue */

/**
 * \file saveas.h
 *
 * A wuss component: a hidden dialogue with a DRAGGABLE file icon, an
 * editable leafname, Cancel/Save buttons and a status line, driving the core
 * DataSave transfer protocol (wuss_MESSAGE_DATA_SAVE and friends, see
 * wuss/message.h) against whatever window the icon is dragged onto -- a
 * Filer directory display in practice. Built on top of wuss_dialogue, as
 * with wuss_info/wuss_proginfo.
 *
 * Attach the dialogue to a menu as a submenu: set a row's
 * wuss_menu_item_t::window to wuss_saveas_window, and the dialogue opens on
 * hover like any other submenu. A save or Cancel then closes the whole menu
 * chain, not just the dialogue. It can still be shown standalone (e.g. from
 * a keyboard shortcut), where it hides itself instead.
 *
 * The task forwards every event for the dialogue's window (see
 * wuss_saveas_window) through wuss_saveas_handle_event, including
 * wuss_EVENT_DRAG_END (the DRAGGABLE icon starts its own core drag) and
 * wuss_EVENT_MESSAGE (the DataSaveAck / DataLoadAck replies).
 *
 * v1 is save-only: there is no load side, and the icon starts a fresh
 * transfer only when none is already in progress.
 *
 * Built only when WUSS_COMPONENTS is defined.
 */

#ifndef WUSS_COMPONENT_SAVEAS_H
#define WUSS_COMPONENT_SAVEAS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "io/filetype.h"

#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Opaque handle: owns the dialogue, its icons and any transfer in
 *  progress. */
typedef struct wuss_saveas wuss_saveas_t;

/**
 * Save callback: write the caller's data to \p path. Called by
 * wuss_saveas_handle_event once the target has offered a full path, either
 * straight away (the writable already held one) or after a DataSaveAck.
 *
 * \param[in] path   Full path to write to.
 * \param[in] opaque As passed to wuss_saveas_create.
 * \return \ref result_OK on success. On failure the transfer is abandoned
 *         silently, as for a bounce; the status line is left showing the
 *         path that failed.
 */
typedef result_t (wuss_saveas_save_fn_t)(const char *path, void *opaque);

/* ----------------------------------------------------------------------- */

/**
 * Create a Save As dialogue: a hidden window on \p task, laid out with a
 * DRAGGABLE file icon, a leafname writable seeded from \p leafname, Cancel
 * and Save buttons, and a status line.
 *
 * \param[out] out      Filled with the new handle on success, untouched on
 *                      failure.
 * \param[in]  task     Task the dialogue window is created on. Must not be
 *                      an autoclose task and must outlive the handle.
 * \param[in]  filetype File type offered in the DataSave protocol; copied.
 *                      Also picks the file icon: the icon-set entry
 *                      "file_<type>" (three lowercase hex digits), else
 *                      "file_xxx", else none.
 * \param[in]  leafname Initial leafname; copied. NULL means "" (a caller
 *                      normally passes a name already carrying the
 *                      platform's extension, e.g. from filetype_from_ext).
 * \param[in]  save_fn  Called to write the file once a path is known; must
 *                      not be NULL.
 * \param[in]  opaque   Passed back to \p save_fn.
 * \return \ref result_OK on success, \ref result_OOM, \ref result_NULL_ARG
 *         if \p out, \p task or \p save_fn is NULL, or a
 *         wuss_dialogue_create/wuss_icon_create code.
 */
result_t wuss_saveas_create(wuss_saveas_t        **out,
                            wuss_task_t           *task,
                            const filetype_t      *filetype,
                            const char            *leafname,
                            wuss_saveas_save_fn_t *save_fn,
                            void                  *opaque);

/**
 * Free a Save As dialogue: closes its window (and so its icons). Safe to
 * pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_saveas_destroy(wuss_saveas_t *doomed);

/**
 * Replace the leafname shown in the writable, as if the user had retyped it.
 * Ignored while a transfer is in progress.
 *
 * \param[in] saveas   Handle.
 * \param[in] leafname New leafname; copied. NULL means "".
 */
void wuss_saveas_set_leafname(wuss_saveas_t *saveas, const char *leafname);

/**
 * Replace the file type offered in the DataSave protocol, and redraw the
 * file icon to match (see wuss_saveas_create).
 *
 * \param[in] saveas   Handle.
 * \param[in] filetype New file type; copied.
 */
void wuss_saveas_set_filetype(wuss_saveas_t    *saveas,
                              const filetype_t *filetype);

/**
 * The writable's current text, for the caller to seed a fresh dialogue or
 * check the last-saved path.
 *
 * \param[in] saveas Handle.
 * \return The writable's text, never NULL (may be ""). Owned by the
 *         dialogue; valid until the next call into it.
 */
const char *wuss_saveas_get_path(const wuss_saveas_t *saveas);

/**
 * Dispatch an event from the task's own handler: a click on Cancel/Save, a
 * DataSaveAck/DataLoadAck/bounce addressed to this dialogue's window, or the
 * wuss_EVENT_DRAG_END from dragging the file icon. Everything else is left
 * unconsumed.
 *
 * Save with the writable already holding a full path (see
 * io/path.h:path_is_full) calls \c save_fn directly. Otherwise dragging the
 * icon onto another window starts the DataSave protocol against it; on
 * success the writable is updated to the full path it was saved to and the
 * dialogue is dismissed -- its menu chain closed when shown as a menu leaf,
 * else hidden (as wuss_dialogue_hide) -- matching a real Save As's "closes
 * on completion". A bounce at any stage abandons the transfer silently.
 *
 * \param[in] saveas Handle.
 * \param[in] window The event's window, exactly as delivered to the task's
 *                   handler (NULL for a broadcast message).
 * \param[in] event  The event.
 * \return 1 if the event was this dialogue's and was consumed, else 0.
 */
int wuss_saveas_handle_event(wuss_saveas_t      *saveas,
                             wuss_window_t      *window,
                             const wuss_event_t *event);

/**
 * The dialogue's window, for showing/hiding it directly or using it as a
 * wuss_menu_item_t::window leaf. Borrowed; valid until wuss_saveas_destroy.
 *
 * \param[in] saveas Handle.
 * \return The window, or NULL if \p saveas is NULL.
 */
wuss_window_t *wuss_saveas_window(const wuss_saveas_t *saveas);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_SAVEAS_H */
