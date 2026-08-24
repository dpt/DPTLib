/* wuss.h -- minimal window manager */

/**
 * \file wuss.h
 *
 * Wuss is a minimal window manager. It owns window creation, positioning,
 * sizing, z-ordering, mouse event routing and dragging. Window contents are
 * entirely delegated to clients (see window.h).
 */

#ifndef WUSS_WUSS_H
#define WUSS_WUSS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bmfont.h"

/* ----------------------------------------------------------------------- */

#define result_WUSS_TOO_SMALL  (result_BASE_WUSS + 0) /* Window/resize dimensions too small */
#define result_WUSS_BAD_COLOUR (result_BASE_WUSS + 1) /* A palette index was out of range */

/* ----------------------------------------------------------------------- */

/** A window manager instance. */
typedef struct wuss wuss_t;

/** A window. Full API is in window.h. */
typedef struct wuss_window wuss_window_t;

/** Mouse buttons. */
typedef enum wuss_button
{
  wuss_BUTTON_LEFT,
  wuss_BUTTON_MIDDLE,
  wuss_BUTTON_RIGHT
}
wuss_button_t;

/** An index into a wuss_t's system palette (see wuss_create). Not a colour_t. */
typedef int wuss_colour_t;

/** Optional creation-time configuration. */
typedef struct wuss_config
{
  int           titlebar_height; /**< Titlebar height in pixels, or 0 to derive from font metrics (or a built-in fallback if no font). */
  wuss_colour_t titlebar_bg;     /**< Titlebar background, as an index into the system palette. */
  wuss_colour_t titlebar_fg;     /**< Titlebar text colour, as an index into the system palette. */
}
wuss_config_t;

/**
 * Create a window manager.
 *
 * \param[in]  scr      Screen to draw windows onto. Not owned; must outlive the wuss_t.
 * \param[in]  font     Font used to draw titlebar labels, or NULL to draw titlebars unlabelled. Not owned.
 * \param[in]  palette  System palette, copied in, or NULL to use a built-in default palette.
 * \param[in]  npalette Number of entries in palette. Ignored if palette is NULL.
 * \param[in]  config   Creation-time configuration, or NULL for defaults.
 * \param[out] wuss     Newly created window manager.
 * \return \ref result_OK on success, \ref result_WUSS_BAD_COLOUR if config's
 *         titlebar_bg/titlebar_fg are out of range for the palette, or
 *         another appropriate result code.
 */
result_t wuss_create(screen_t             *scr,
                     bmfont_t             *font,
                     const colour_t       *palette,
                     int                   npalette,
                     const wuss_config_t  *config,
                     wuss_t              **wuss);

/**
 * Destroy a window manager, and any windows still open on it.
 *
 * \param[in] doomed Window manager to destroy.
 */
void wuss_destroy(wuss_t *doomed);

/**
 * Redraw every window, back-to-front.
 *
 * \param[in] wuss Window manager.
 * \return \ref result_OK on success, or the last non-OK result returned by
 *         a client's redraw callback (drawing continues past a failing
 *         window rather than stopping).
 */
result_t wuss_redraw(wuss_t *wuss);

/**
 * Deliver a mouse-down event. Hit-tests the topmost window at (x,y),
 * brings it to front, and either starts a titlebar drag or delivers the
 * event to the window's client in window-local content coordinates.
 *
 * \param[in]  wuss   Window manager.
 * \param[in]  x      Screen x coordinate.
 * \param[in]  y      Screen y coordinate.
 * \param[in]  button Button pressed.
 * \param[out] hit    Window under the pointer, or NULL if none. May be NULL if not needed.
 * \return \ref result_OK, or a result code returned by the client's mouse callback.
 */
result_t wuss_mouse_down(wuss_t *wuss, int x, int y, wuss_button_t button, wuss_window_t **hit);

/**
 * Deliver a mouse-up event. Ends an in-progress drag if one is active,
 * otherwise hit-tests and delivers to the window's client as per
 * wuss_mouse_down.
 *
 * \param[in]  wuss   Window manager.
 * \param[in]  x      Screen x coordinate.
 * \param[in]  y      Screen y coordinate.
 * \param[in]  button Button released.
 * \param[out] hit    Window under the pointer (or being dragged), or NULL if none. May be NULL if not needed.
 * \return \ref result_OK, or a result code returned by the client's mouse callback.
 */
result_t wuss_mouse_up(wuss_t *wuss, int x, int y, wuss_button_t button, wuss_window_t **hit);

/**
 * Deliver a mouse-move event. Updates the dragged window's position if a
 * drag is active (triggering a redraw), otherwise hit-tests and delivers
 * to the window's client as per wuss_mouse_down.
 *
 * \param[in]  wuss Window manager.
 * \param[in]  x    Screen x coordinate.
 * \param[in]  y    Screen y coordinate.
 * \param[out] hit  Window under the pointer (or being dragged), or NULL if none. May be NULL if not needed.
 * \return \ref result_OK, or a result code returned by the client's mouse callback.
 */
result_t wuss_mouse_move(wuss_t *wuss, int x, int y, wuss_window_t **hit);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_WUSS_H */
