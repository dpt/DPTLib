/* wuss/iconbar.h -- wuss RISC OS-style icon bar */

/**
 * \file iconbar.h
 *
 * A RISC OS-style icon bar: a strip pinned to the bottom edge of the screen,
 * full width, always drawn on top of every window and never hit-tested as
 * part of the ordinary window stack. Any task can add an icon to it; a click
 * or drag is delivered to that icon's owning task as a task-view
 * wuss_EVENT_ICON (window == NULL, data.iconbar_icon), the same event kind a
 * work-area wuss_ICON_TYPE_ACTION icon raises in the window view.
 *
 * The bar itself always exists once the library is built with the
 * WUSS_ICONBAR option on -- there is no create/destroy call for the bar,
 * only for the icons on it. Icons are laid out left to right in fixed-size
 * slots; an icon bar wider than the screen simply clips, it does not wrap or
 * scroll.
 */

#ifndef WUSS_ICONBAR_H
#define WUSS_ICONBAR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/screen.h"

#include "wuss/wuss.h"

/* The icon bar is a compile-time option (CMake WUSS_ICONBAR). With it off
 * the library has no wuss_iconbar_* symbols, so this header body is
 * skipped. */
#ifdef WUSS_ICONBAR

/* ----------------------------------------------------------------------- */

/* wuss_iconbar_icon_t (opaque) is forward-declared in wuss.h so it can be
 * named regardless of this option. */

/**
 * Description of an icon bar icon at creation. Copied by value into the
 * icon; the caller keeps ownership of \c text, which is copied.
 */
typedef struct wuss_iconbar_icon_spec
{
  /** NUL-terminated label; copied. NULL means "". */
  const char     *text;

  /** The image to draw. Borrowed, not copied; must outlive the icon. NULL
   *  (the default) draws the slot with just its bevel and label text. */
  const bitmap_t *image;
}
wuss_iconbar_icon_spec_t;

/**
 * Add an icon to the icon bar, in the next free slot to the right of every
 * existing icon. The icon's bounding box is invalidated so the next redraw
 * paints it.
 *
 * \param[in]  wuss  Window manager.
 * \param[in]  owner Task to receive this icon's wuss_EVENT_ICON events (as a
 *                   task-view event, window == NULL). Must not be NULL.
 * \param[in]  spec  Icon description; copied.
 * \param[out] icon  Filled in with the new icon handle, or NULL if the
 *                   caller does not need it.
 * \return \ref result_OK on success, \ref result_OOM on allocation failure.
 */
result_t wuss_iconbar_icon_create(wuss_t                         *wuss,
                                  wuss_task_t                    *owner,
                                  const wuss_iconbar_icon_spec_t *spec,
                                  wuss_iconbar_icon_t           **icon);

/**
 * Remove an icon from the icon bar, freeing its slot and invalidating its
 * bounding box so the next redraw clears it. Safe to pass NULL for \p icon.
 *
 * \param[in] wuss Window manager.
 * \param[in] icon Icon to remove, or NULL.
 */
void wuss_iconbar_icon_destroy(wuss_t *wuss, wuss_iconbar_icon_t *icon);

#endif /* WUSS_ICONBAR */

#ifdef __cplusplus
}
#endif

#endif /* WUSS_ICONBAR_H */
