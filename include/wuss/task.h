/* task.h -- wuss task API */

/**
 * \file task.h
 *
 * A Wuss task: the content delegate a window hands its drawing and input
 * events to, and the events themselves.
 */

#ifndef WUSS_TASK_H
#define WUSS_TASK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"
#include "geom/point.h"
#include "framebuf/screen.h"

#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Which kind of event a wuss_event_t carries; more will be added over time. */
typedef enum wuss_event_kind
{
  wuss_EVENT_IDLE,   /**< Wuss has finished its pending tasks. */
  wuss_EVENT_REDRAW, /**< Part of the window's content needs repainting. */
  wuss_EVENT_OPEN,   /**< Window moved or resized. */
  wuss_EVENT_CLOSE,  /**< Close icon clicked; Wuss takes no action itself. */
  wuss_EVENT_MOUSE,  /**< Button down/up over the window's content. */
  wuss_EVENT_SCROLL, /**< Mouse wheel used over the window's content. */
  wuss_EVENT_QUIT    /**< Task shutting down, via wuss_task_stop. */
}
wuss_event_kind_t;

/**
 * An event delivered to a task's handle callback. Only the union member
 * matching \c kind is valid.
 */
typedef struct wuss_event
{
  wuss_event_kind_t kind;
  union
  {
    /** wuss_EVENT_REDRAW: called with scr->clip already set to the
     * on-screen, clipped content area. bounds and scroll are exactly what
     * wuss_window_get_content_bounds/wuss_window_get_scroll would return,
     * passed through so tasks don't need to call back into Wuss on every
     * redraw. */
    struct
    {
      screen_t    *scr;

      /**
       * The region that actually needs repainting, screen space; a subset of
       * bounds. Tasks should only touch pixels within this box.
       */
      const box_t *content;

      /**
       * The window's full (unclipped) content-area box, screen space, as per
       * wuss_window_get_content_bounds; for converting screen position to
       * document position.
       */
      const box_t *bounds;

      /** Current scroll offset, as per wuss_window_get_scroll. */
      point_t      scroll;
    }
    redraw;

    /** wuss_EVENT_MOUSE: point is in virtual content space -- the window's
     * scroll offset has already been added, so the task must not add it
     * again. With a scroll offset of (0,0) this is the same as window-local
     * content coordinates, where the content area's top-left is (0,0).
     * button is meaningful for DOWN/UP. */
    struct
    {
      wuss_mouse_action_t action;
      point_t             point;
      wuss_button_t       button;
    }
    mouse;

    /** wuss_EVENT_SCROLL: point is window-local content coordinates, as
     * per mouse. delta's sign and units are as passed to wuss_scroll. */
    struct
    {
      point_t point;
      int     delta;
    }
    scroll;

    /* wuss_EVENT_IDLE, wuss_EVENT_CLOSE, wuss_EVENT_QUIT and
     * wuss_EVENT_OPEN carry no data. */
  }
  data;
}
wuss_event_t;

/**
 * Task event callback.
 *
 * \param[in] window    The window receiving the event.
 * \param[in] event     The event; see wuss_event_t.
 * \param[in] task_data As passed to wuss_window_create.
 * \return \ref result_OK on success, else an appropriate result code.
 */
typedef result_t (wuss_event_fn_t)(wuss_window_t      *window,
                                   const wuss_event_t *event,
                                   void               *task_data);

/** A window's content delegate. Copied by value into the window at creation. */
typedef struct wuss_task
{
  /**
   * NULL => task receives no events; Wuss still fills the content background
   * per wuss_window_create's bg.
   */
  wuss_event_fn_t *handle;

  void            *task_data;
}
wuss_task_t;

/**
 * Build a wuss_task_t from its fields.
 *
 * \param[in] handle    Event callback, or NULL for a task that receives no
 *                      events.
 * \param[in] task_data Opaque pointer passed back to the callback.
 * \return The populated task.
 */
wuss_task_t wuss_task_start(wuss_event_fn_t *handle,
                            void            *task_data);

/**
 * Notify a window's task that it is shutting down, via wuss_EVENT_QUIT. Not
 * called automatically by wuss_window_close; call it first if the task needs
 * notice before its window is torn down.
 *
 * \param[in] window Window whose task should be stopped.
 * \return \ref result_OK on success, else the result returned by the task's
 *         handle callback.
 */
result_t wuss_task_stop(wuss_window_t *window);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_TASK_H */
