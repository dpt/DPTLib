/* wuss/message.h -- inter-task messaging */

/**
 * \file message.h
 *
 * Fixed-size messages passed between tasks, RISC OS Wimp-message style.
 * Every send is queued, never delivered inline: sending never re-enters a
 * handler, and the whole queue is drained to empty whenever a public wuss
 * entry point (wuss_mouse_click, wuss_mouse_move, wuss_key, wuss_idle)
 * returns to top level. There is no public pump -- the queue is always empty
 * again by the time control returns to the frontend.
 *
 * A message may be sent plain (fire and forget), recorded (bounces back to
 * the sender as \ref wuss_EVENT_MESSAGE_BOUNCED if nobody acknowledges it
 * during its handler) or as an acknowledgement of a recorded message. Action
 * codes 0x0000-0xFFFF are reserved for core (see \ref wuss_MESSAGE_DATA_SAVE
 * and friends); tasks define their own from \ref wuss_MESSAGE_APP_BASE
 * upwards.
 */

#ifndef WUSS_MESSAGE_H
#define WUSS_MESSAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include "base/result.h"

#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** A message payload that would not fit in \ref wuss_MESSAGE_SIZE; the
 *  caller must shrink it rather than have it silently truncated. */
#define result_WUSS_MESSAGE_TOO_BIG (result_BASE_WUSS + 6)

/* ----------------------------------------------------------------------- */

/** Total size of a wuss_message_t block, header included. */
#define wuss_MESSAGE_SIZE 1024

/** First action code available to tasks; 0x0000-0xFFFF are reserved for
 *  core (see wuss_MESSAGE_DATA_SAVE and its neighbours). */
#define wuss_MESSAGE_APP_BASE 0x10000

/** Core transfer protocol action codes: DataSave -> DataSaveAck -> (saver
 *  writes the file) -> DataLoad -> DataLoadAck, every step a recorded
 *  send. See docs/windowing/drag-save-plan.md. */
enum
{
  wuss_MESSAGE_DATA_SAVE,     /**< Saver -> target: offering a save. */
  wuss_MESSAGE_DATA_SAVE_ACK, /**< Target -> saver: full path to write to. */
  wuss_MESSAGE_DATA_LOAD,     /**< Saver -> target: file has been written. */
  wuss_MESSAGE_DATA_LOAD_ACK  /**< Target -> saver: transfer complete. */
};

/**
 * A message in flight: fixed size, copied by value into the queue -- no
 * per-message allocation.
 */
struct wuss_message
{
  /** Bytes actually used by \ref data, header excluded. */
  size_t         size;

  /** Action code: a wuss_MESSAGE_* core code, or a task-defined code from
   *  \ref wuss_MESSAGE_APP_BASE upwards. */
  int            action;

  /** Sending task. Never NULL: every send is attributed. */
  wuss_task_t   *sender;

  /** Target window, or NULL for a broadcast. Delivery is to this window's
   *  owning task; the window itself is passed through to the recipient. */
  wuss_window_t *window;

  /** Non-zero reference assigned by the sender (see wuss_send /
   *  wuss_send_recorded), from a global counter that skips 0 on wrap. */
  unsigned int   my_ref;

  /** 0 for a fresh send. Set to the message being acknowledged/replied to
   *  by wuss_acknowledge, or by a plain/recorded send made from within a
   *  handler that is itself replying to one. */
  unsigned int   your_ref;

  /** Non-zero if this send bounces back to the sender as \ref
   *  wuss_EVENT_MESSAGE_BOUNCED when nobody acknowledges it, or if the
   *  window has gone by delivery time. */
  int            recorded;

  /** Payload, up to \ref wuss_MESSAGE_DATA_SIZE bytes; only the first \c
   *  size bytes are meaningful. */
  unsigned char  data[wuss_MESSAGE_SIZE - sizeof(size_t) - sizeof(int) -
                       sizeof(wuss_task_t *) - sizeof(wuss_window_t *) -
                       2 * sizeof(unsigned int) - sizeof(int)];
};

/** Size of a message's payload area (wuss_message_t::data). Defined after
 *  the struct so it can be measured directly rather than duplicating the
 *  header's field sizes (which \ref data's own bound above must still do,
 *  being inside the struct that defines it). */
#define wuss_MESSAGE_DATA_SIZE \
  (sizeof(((wuss_message_t *) 0)->data))

/**
 * Send a plain, fire-and-forget message. Queued; delivered no earlier than
 * the current entry point's queue drain (see the file overview).
 *
 * \param[in] wuss     Window manager.
 * \param[in] sender   Sending task.
 * \param[in] window   Target window, or NULL to broadcast to every other
 *                     task, in registration order.
 * \param[in] action   Action code.
 * \param[in] data     Payload, copied in, or NULL if \p size is 0.
 * \param[in] size     Payload size in bytes.
 * \param[in] your_ref 0 for a fresh send, or the my_ref of a message this
 *                     one replies to (counts as acknowledging it if sent
 *                     from within that message's own handler).
 * \return \ref result_OK on success, \ref result_WUSS_MESSAGE_TOO_BIG if \p
 *         size exceeds \ref wuss_MESSAGE_DATA_SIZE, \ref result_OOM if the
 *         queue could not grow to hold it.
 */
result_t wuss_send(wuss_t        *wuss,
                   wuss_task_t   *sender,
                   wuss_window_t *window,
                   int            action,
                   const void    *data,
                   size_t         size,
                   unsigned int   your_ref);

/**
 * Send a recorded message: bounces back to the sender as \ref
 * wuss_EVENT_MESSAGE_BOUNCED, with the original message's fields, if the
 * recipient's handler does not acknowledge it (see wuss_acknowledge, or a
 * wuss_send/wuss_send_recorded made from within the handler with your_ref
 * set to *my_ref).
 *
 * \param[in]  wuss     Window manager.
 * \param[in]  sender   Sending task.
 * \param[in]  window   Target window, or NULL to broadcast; for a recorded
 *                      broadcast the first acknowledgement stops further
 *                      delivery.
 * \param[in]  action   Action code.
 * \param[in]  data     Payload, copied in, or NULL if \p size is 0.
 * \param[in]  size     Payload size in bytes.
 * \param[in]  your_ref 0 for a fresh send, or the my_ref of a message this
 *                      one replies to.
 * \param[out] my_ref   Filled with the reference assigned to this send, for
 *                      matching a later wuss_EVENT_MESSAGE_BOUNCED or reply
 *                      against it. May be NULL if not needed.
 * \return \ref result_OK on success, \ref result_WUSS_MESSAGE_TOO_BIG if \p
 *         size exceeds \ref wuss_MESSAGE_DATA_SIZE, \ref result_OOM if the
 *         queue could not grow to hold it.
 */
result_t wuss_send_recorded(wuss_t        *wuss,
                            wuss_task_t   *sender,
                            wuss_window_t *window,
                            int            action,
                            const void    *data,
                            size_t         size,
                            unsigned int   your_ref,
                            unsigned int  *my_ref);

/**
 * Acknowledge a recorded message from within its own \ref wuss_EVENT_MESSAGE
 * handler, without sending any payload or reply of your own. Marks \p msg as
 * acknowledged so it is not bounced once the handler returns; sends nothing
 * and is never itself observed by any task. Calling this outside the handler
 * for \p msg has no defined effect -- the bounce decision is made when the
 * handler returns.
 *
 * \param[in] wuss      Window manager.
 * \param[in] recipient Task acknowledging (must be the message's recipient).
 * \param[in] msg       The message being acknowledged.
 * \return \ref result_OK always.
 */
result_t wuss_acknowledge(wuss_t               *wuss,
                          wuss_task_t          *recipient,
                          const wuss_message_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_MESSAGE_H */
