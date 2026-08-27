/* wuss.h -- minimal window manager */

/**
 * \file wuss.h
 *
 * Wuss is a minimal window manager. It owns window creation, positioning,
 * sizing, z-ordering, mouse event routing and dragging. Window contents are
 * entirely delegated to tasks (see window.h).
 */

#ifndef WUSS_WUSS_H
#define WUSS_WUSS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "geom/box.h"
#include "geom/point.h"

/* ----------------------------------------------------------------------- */

#define result_WUSS_TOO_SMALL  (result_BASE_WUSS + 0) /* Window/resize dimensions too small */
#define result_WUSS_BAD_COLOUR (result_BASE_WUSS + 1) /* A palette index was out of range */

/* ----------------------------------------------------------------------- */

/** A window manager instance. */
typedef struct wuss wuss_t;

/** A window. Full API is in window.h. */
typedef struct wuss_window wuss_window_t;

/**
 * Mouse buttons, RISC OS-style: Select is the primary action, Adjust the
 * secondary action, Menu pops up a menu.
 */
typedef enum wuss_button
{
  wuss_BUTTON_SELECT,
  wuss_BUTTON_MENU,
  wuss_BUTTON_ADJUST
}
wuss_button_t;

/** The kind of mouse event delivered to a task's mouse callback. */
typedef enum wuss_mouse_action
{
  wuss_MOUSE_DOWN,
  wuss_MOUSE_UP,
  wuss_MOUSE_MOVE
}
wuss_mouse_action_t;

/** An index into a wuss_t's system palette (see wuss_create). Not a colour_t. */
typedef int wuss_colour_t;

/** Sentinel for wuss_task_t::bg meaning "no automatic background fill". */
#define wuss_NO_BACKGROUND ((wuss_colour_t) -1)

/** Furniture chrome colours, one entry per class of furniture. Title is
 * the only two-tone class (fill + text); the rest are drawn as a single
 * flat colour. Each value is an index into the system palette (see
 * wuss_create). */
typedef struct wuss_palette
{
  struct
  {
    wuss_colour_t bg;     /**< Titlebar fill. */
    wuss_colour_t fg;     /**< Titlebar text. */
  }
  title;
  wuss_colour_t back;     /**< Send-to-back icon. */
  wuss_colour_t close;    /**< Close icon. */
  wuss_colour_t toggle;   /**< Toggle-size icon. */
  wuss_colour_t resize;   /**< Resize icon. */
  wuss_colour_t arrows;   /**< Scrollbar arrows. */
  wuss_colour_t wells;    /**< Scrollbar wells. */
  wuss_colour_t sausages; /**< Scrollbar sausages. */
}
wuss_palette_t;

/** Per-window appearance flags, combinable with bitwise OR. */
typedef enum wuss_window_flags
{
  wuss_WINDOW_NONE           = 0,      /**< Default: every furniture region drawn. */
  wuss_WINDOW_NO_TITLEBAR    = 1 << 0, /**< No titlebar; content fills the full visible area, and no drag handle exists. */
  wuss_WINDOW_NO_OUTLINE     = 1 << 1, /**< No 1px border drawn around the visible area. */
  wuss_WINDOW_NO_CLOSE       = 1 << 2, /**< No close icon in the titlebar. Ignored if flags includes wuss_WINDOW_NO_TITLEBAR. */
  wuss_WINDOW_NO_BACK        = 1 << 3, /**< No send-to-back icon in the titlebar. Ignored if flags includes wuss_WINDOW_NO_TITLEBAR. */
  wuss_WINDOW_NO_TOGGLE_SIZE = 1 << 4, /**< No toggle-size icon in the titlebar. Ignored if flags includes wuss_WINDOW_NO_TITLEBAR. */
  wuss_WINDOW_NO_VSCROLL     = 1 << 5, /**< No vertical scrollbar on the right edge. */
  wuss_WINDOW_NO_HSCROLL     = 1 << 6, /**< No horizontal scrollbar on the bottom edge. */
  wuss_WINDOW_NO_RESIZE      = 1 << 7, /**< No resize icon in the bottom-right corner. */
  wuss_WINDOW_NO_TOGGLE_BLIT = 1 << 8  /**< Toggle-size always fully redraws the window's content instead of blitting the preserved region -- for a task whose rendering depends on the window's size in ways redraw can't patch incrementally (e.g. a palette that lays itself out across the whole window). */
}
wuss_window_flags_t;

/** Reason code for wuss_window_restack, selecting which end of the
 * z-order a window moves to. */
typedef enum wuss_zorder
{
  wuss_ZORDER_FRONT, /**< Move the window to the front (topmost). */
  wuss_ZORDER_BACK   /**< Move the window to the back (bottommost). */
}
wuss_zorder_t;

/** Optional creation-time configuration. */
typedef struct wuss_config
{
  int            titlebar_height; /**< Titlebar height in pixels, or 0 to derive from font metrics (or a built-in fallback if no font). */
  wuss_palette_t palette;         /**< Furniture chrome colours. */
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
 * \return \ref result_OK on success, \ref result_WUSS_BAD_COLOUR if any of
 *         config's palette entries are out of range for the palette, or
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
 * Fetch the system font (see wuss_create), for tasks to draw their own
 * content in the same face as window titlebars.
 *
 * \param[in] wuss Window manager.
 * \return System font, or NULL if none was given to wuss_create.
 */
bmfont_t *wuss_get_font(const wuss_t *wuss);

/**
 * Redraw every window, back-to-front.
 *
 * \param[in] wuss Window manager.
 * \return \ref result_OK on success, or the last non-OK result returned by
 *         a task's redraw callback (drawing continues past a failing
 *         window rather than stopping).
 */
result_t wuss_redraw(wuss_t *wuss);

/**
 * Mark a screen-space region dirty. Window creation, destruction, move,
 * resize and bring-to-front invalidate their own affected regions
 * automatically; tasks must call this themselves when their content
 * changes (e.g. an animation), passing the union of the old and new
 * screen-space areas that need repainting.
 *
 * \param[in] wuss Window manager.
 * \param[in] box  Screen-space region to mark dirty.
 * \return \ref result_OK.
 */
result_t wuss_invalidate(wuss_t *wuss, const box_t *box);

/**
 * Redraw only the region accumulated by wuss_invalidate calls (and any
 * automatic invalidation from window management) since the last redraw,
 * back-to-front, then clear the dirty region. Does nothing if nothing is
 * dirty.
 *
 * \param[in] wuss Window manager.
 * \return \ref result_OK on success, or the last non-OK result returned by
 *         a task's redraw callback.
 */
result_t wuss_redraw_dirty(wuss_t *wuss);

/**
 * Fetch the number of dirty regions currently accumulated (see
 * wuss_invalidate). Regions are self-coalescing: an invalidation already
 * covered by an existing region is discarded, and one sharing a complete
 * edge with an existing region extends it in place, so this stays small
 * under most usage.
 *
 * \param[in] wuss Window manager.
 * \return Number of dirty regions, 0 if nothing is dirty.
 */
int wuss_get_dirty_count(const wuss_t *wuss);

/**
 * Fetch one of the current accumulated dirty regions (see
 * wuss_invalidate). Wuss only repaints windows, not background
 * between/behind them, so a caller whose invalidations can expose
 * background (e.g. after a window move) should clear these regions itself
 * before calling wuss_redraw_dirty.
 *
 * \param[in]  wuss  Window manager.
 * \param[in]  index Index of the region to fetch, 0 to wuss_get_dirty_count() - 1.
 * \param[out] out   Filled in with the dirty region.
 */
void wuss_get_dirty(const wuss_t *wuss, int index, box_t *out);

/**
 * Deliver a mouse-down or mouse-up event (action must be wuss_MOUSE_DOWN or
 * wuss_MOUSE_UP). Hit-tests the topmost window at (x,y). On a down, a
 * titlebar click brings the window to front if button is Select (Adjust
 * and Menu leave the z-order unchanged) and starts a drag; on an up, an
 * in-progress drag is ended instead of hit-testing (an Adjust click with
 * no move in between sends the window to the back rather than dragging
 * it). A click on the window's content never changes the z-order and is
 * delivered to the task in window-local content coordinates.
 *
 * \param[in]  wuss   Window manager.
 * \param[in]  p      Screen coordinate.
 * \param[in]  button Button pressed or released.
 * \param[in]  action wuss_MOUSE_DOWN or wuss_MOUSE_UP.
 * \param[out] hit    Window under the pointer (or being dragged), or NULL if none. May be NULL if not needed.
 * \return \ref result_OK, or a result code returned by the task's mouse callback.
 */
result_t wuss_mouse_click(wuss_t              *wuss,
                          point_t              p,
                          wuss_button_t        button,
                          wuss_mouse_action_t  action,
                          wuss_window_t      **hit);

/**
 * Deliver a mouse-move event. Updates the dragged window's position if a
 * drag is active (invalidating the affected region; call wuss_redraw_dirty
 * to actually repaint it), otherwise hit-tests and delivers to the
 * window's task as per wuss_mouse_click.
 *
 * \param[in]  wuss Window manager.
 * \param[in]  p    Screen coordinate.
 * \param[out] hit  Window under the pointer (or being dragged), or NULL if none. May be NULL if not needed.
 * \return \ref result_OK, or a result code returned by the task's mouse callback.
 */
result_t wuss_mouse_move(wuss_t *wuss, point_t p, wuss_window_t **hit);

/**
 * Deliver a scroll event. Hit-tests the topmost window at p as per
 * wuss_mouse_click, and delivers to the window's task in window-local
 * content coordinates; dropped if the hit window has no scroll callback,
 * or the pointer is over its titlebar.
 *
 * \param[in]  wuss  Window manager.
 * \param[in]  p     Screen coordinate.
 * \param[in]  delta Scroll amount; sign and units are caller-defined.
 * \param[out] hit   Window under the pointer, or NULL if none. May be NULL if not needed.
 * \return \ref result_OK, or a result code returned by the task's scroll callback.
 */
result_t wuss_scroll(wuss_t         *wuss,
                     point_t         p,
                     int             delta,
                     wuss_window_t **hit);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_WUSS_H */
