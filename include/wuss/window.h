/* window.h -- wuss window API */

/**
 * \file window.h
 *
 * A wuss window: creation, destruction, positioning, sizing, and client
 * delegation of content drawing and mouse handling.
 */

#ifndef WUSS_WINDOW_H
#define WUSS_WINDOW_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"
#include "framebuf/screen.h"

#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** The kind of mouse event delivered to a client's mouse callback. */
typedef enum wuss_mouse_action
{
  wuss_MOUSE_DOWN,
  wuss_MOUSE_UP,
  wuss_MOUSE_MOVE
}
wuss_mouse_action_t;

/**
 * Client redraw callback. Called with scr->clip already set to the
 * on-screen, clipped content area.
 *
 * \param[in] window      The window being redrawn.
 * \param[in] scr         Screen to draw into.
 * \param[in] content     The window's (unclipped) content box, screen space, for context.
 * \param[in] client_data As passed to wuss_window_create.
 * \return \ref result_OK on success, else an appropriate result code.
 */
typedef result_t (wuss_redraw_fn_t)(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data);

/**
 * Client mouse callback. x,y are window-local content coordinates (the
 * content area's top-left is (0,0)).
 *
 * \param[in] window      The window receiving the event.
 * \param[in] action      Which kind of mouse event this is.
 * \param[in] x           Window-local x coordinate.
 * \param[in] y           Window-local y coordinate.
 * \param[in] button      Button involved (meaningful for DOWN/UP).
 * \param[in] client_data As passed to wuss_window_create.
 * \return \ref result_OK on success, else an appropriate result code.
 */
typedef result_t (wuss_mouse_fn_t)(wuss_window_t *window, wuss_mouse_action_t action, int x, int y, wuss_button_t button, void *client_data);

/** A window's content delegate. Copied by value into the window at creation. */
typedef struct wuss_client
{
  wuss_redraw_fn_t *redraw;      /**< NULL => content area left blank. */
  wuss_mouse_fn_t  *mouse;       /**< NULL => content mouse events dropped. */
  void             *client_data;
}
wuss_client_t;

/**
 * Create a window.
 *
 * \param[in]  wuss    Window manager to create the window on.
 * \param[in]  visible Initial visible (on-screen) bounds, screen space. Copied in.
 * \param[in]  title   Titlebar label, or NULL for none. Copied in, truncated if too long.
 * \param[in]  client  Content delegate. Copied in. May be NULL for a window with no content handling.
 * \param[out] window  Newly created window. Becomes the topmost window.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if visible
 *         is too small to hold a titlebar plus any content, or another
 *         appropriate result code.
 */
result_t wuss_window_create(wuss_t *wuss, const box_t *visible, const char *title, const wuss_client_t *client, wuss_window_t **window);

/**
 * Destroy a window.
 *
 * \param[in] doomed Window to destroy.
 */
void wuss_window_destroy(wuss_window_t *doomed);

/**
 * Move a window, preserving its size.
 *
 * \param[in] window Window to move.
 * \param[in] x      New screen x coordinate of the window's top-left.
 * \param[in] y      New screen y coordinate of the window's top-left.
 */
void wuss_window_move(wuss_window_t *window, int x, int y);

/**
 * Resize a window, preserving its top-left position.
 *
 * \param[in] window Window to resize.
 * \param[in] width  New width.
 * \param[in] height New height.
 * \return \ref result_OK on success, \ref result_WUSS_TOO_SMALL if width/height
 *         are too small to hold a titlebar plus any content.
 */
result_t wuss_window_resize(wuss_window_t *window, int width, int height);

/**
 * Bring a window to the front of the z-order.
 *
 * \param[in] window Window to bring to front.
 */
void wuss_window_bring_to_front(wuss_window_t *window);

/**
 * Fetch a window's current visible (on-screen) bounds.
 *
 * \param[in]  window  Window to query.
 * \param[out] visible Filled in with the window's visible bounds.
 */
void wuss_window_get_visible_bounds(const wuss_window_t *window, box_t *visible);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_WINDOW_H */
