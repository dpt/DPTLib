/* wuss/icon.h -- wuss work-area icons */

/**
 * \file icon.h
 *
 * Work-area icons: static labels and clickable bevelled buttons that Wuss draws
 * inside a window's content area and hit-tests before the content task sees a
 * click.
 *
 * An icon's bounding box is given in virtual document space -- the same
 * coordinate space as wuss_EVENT_MOUSE's point and wuss_window_invalidate's
 * local_box -- so an icon scrolls with the content it sits on. Its on-screen
 * position is (content.x0 - scroll.x + bbox), using the window's current
 * content bounds and scroll offset.
 *
 * Wuss fills a window's background, draws its icons, then delivers
 * wuss_EVENT_REDRAW, so a task is free to paint over or around icon pixels. A
 * click on a wuss_ICON_TYPE_BUTTON reaches the task as wuss_EVENT_ICON; clicks
 * on a label, or on a hidden or disabled icon, fall through as
 * wuss_EVENT_MOUSE.
 */

#ifndef WUSS_ICON_H
#define WUSS_ICON_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "wuss/wuss.h"

/* The in-content icon subsystem is a compile-time option (CMake WUSS_ICONS).
 * With it off the library has no wuss_icon_* symbols, so this header body is
 * skipped. */
#ifdef WUSS_ICONS

/* ----------------------------------------------------------------------- */

/* wuss_icon_t (opaque, owned by the window it is created on) is forward-declared
 * in wuss.h so wuss_event_t can name it regardless of this option. */

/**
 * What an icon looks like and how it behaves. The enum is left open so sprite
 * and editable-text icons can be added later without breaking existing specs.
 */
typedef enum wuss_icon_type
{
  wuss_ICON_TYPE_LABEL = 0, /**< Static text drawn with the window manager's
                             *   font. Not interactive: clicks fall through to
                             *   the task as wuss_EVENT_MOUSE. */
  wuss_ICON_TYPE_BUTTON,    /**< Bevelled rectangle with a centred text label
                             *   and pressed-state visual feedback; clicks and
                             *   hovers are delivered to the task as
                             *   wuss_EVENT_ICON. */
  wuss_ICON_TYPE_PATTERN,   /**< Bounding box filled with a repeating two-colour
                             *   8x8 tile (spec.pattern) in fg/bg, phased to
                             *   document space so it scrolls rigidly with
                             *   content. Not interactive: clicks fall through
                             *   to the task as wuss_EVENT_MOUSE. text is
                             *   ignored. */
  wuss_ICON_TYPE_FRAME,     /**< A grouping box: a one-pixel rectangle in fg
                             *   around the bounding box, broken at the top-left
                             *   for an optional caption (text) drawn over the
                             *   window background. Not interactive: clicks fall
                             *   through to the task as wuss_EVENT_MOUSE. */
  wuss_ICON_TYPE_RADIO,     /**< A radio button: a small ring at the left of the
                             *   bounding box, filled when selected, with the
                             *   label (text) to its right. Interactive: a click
                             *   selects it and clears every other selected
                             *   radio sharing its non-zero group, then the task
                             *   is told via wuss_EVENT_ICON. */
  wuss_ICON_TYPE_OPTION,    /**< An option button: a small box at the left of
                             *   the bounding box, ticked when selected, with
                             *   the label (text) to its right. Interactive: a
                             *   click toggles its own selected state (group is
                             *   ignored), then the task is told via
                             *   wuss_EVENT_ICON. */
  wuss_ICON_TYPE_BITMAP,    /**< A caller-owned bitmap (spec.bitmap) drawn at
                             *   the top-left of the bounding box, alpha-blended
                             *   against what is already there, clipped to the
                             *   box; no scaling. The bitmap is borrowed, not
                             *   copied, and must outlive the icon (unlike
                             *   text). fg, bg, text and pattern are ignored.
                             *   Not interactive unless
                             *   wuss_ICON_FLAGS_INTERACTIVE is set, in which
                             *   case clicks raise wuss_EVENT_ICON like a
                             *   button. */
  wuss_ICON_TYPE_MENU_ENTRY /**< A menu row: text left-justified across the
                             *   bounding box, drawn in fg over the window
                             *   background, or inverted (window manager's title
                             *   colours) while the pointer is over it. An
                             *   optional tick at the left edge when the icon is
                             *   selected (see wuss_icon_set_selected), and an
                             *   optional submenu arrow at the right edge with
                             *   wuss_ICON_FLAGS_SUBMENU.
                             *   wuss_ICON_FLAGS_SEPARATOR draws a dashed rule
                             *   along the top edge before the text, marking a
                             *   group boundary; the entry keeps its label and
                             *   stays interactive (an entry with the flag and
                             *   no text is just a bare rule). Interactive: a
                             *   click raises wuss_EVENT_ICON like a button; a
                             *   disabled entry never highlights and its clicks
                             *   fall through. */
}
wuss_icon_type_t;

/** Icon appearance and behaviour flags, combinable with bitwise OR. */
typedef enum wuss_icon_flags
{
  wuss_ICON_FLAGS_NONE          = 0,
  wuss_ICON_FLAGS_HIDDEN        = 1 << 0, /**< Not drawn, not hit-tested. */
  wuss_ICON_FLAGS_DISABLED      = 1 << 1, /**< Drawn greyed; clicks fall through
                                          *   to the task as wuss_EVENT_MOUSE
                                          *   rather than raising
                                          *   wuss_EVENT_ICON. */
  wuss_ICON_FLAGS_JUSTIFY_RIGHT = 1 << 2, /**< wuss_ICON_TYPE_LABEL: right-align
                                          *   the text in the bounding box
                                          *   instead of the default left. */
  wuss_ICON_FLAGS_JUSTIFY_CENTRE = 1 << 3, /**< wuss_ICON_TYPE_LABEL: centre
                                          *   the text in the bounding box.
                                          *   Takes precedence over
                                          *   wuss_ICON_FLAGS_JUSTIFY_RIGHT. */
  wuss_ICON_FLAGS_DEFAULT       = 1 << 4, /**< wuss_ICON_TYPE_BUTTON: draw as a
                                          *   default action button, in the
                                          *   window manager's accent colours
                                          *   (see wuss_config_t::accent)
                                          *   instead of the ordinary bevel.
                                          *   Ignored by other icon types. */
  wuss_ICON_FLAGS_INTERACTIVE  = 1 << 5, /**< wuss_ICON_TYPE_BITMAP: hit-test
                                          *   the icon and raise wuss_EVENT_ICON
                                          *   on a click, like a button.
                                          *   Without it a bitmap icon is pure
                                          *   decoration and clicks fall through
                                          *   as wuss_EVENT_MOUSE. Ignored by
                                          *   other icon types (interactive or
                                          *   not by their nature). */
  wuss_ICON_FLAGS_SUBMENU      = 1 << 6, /**< wuss_ICON_TYPE_MENU_ENTRY: draw a
                                          *   right-pointing arrow at the right
                                          *   edge, marking an entry that opens
                                          *   a submenu. Ignored by other
                                          *   types. */
  wuss_ICON_FLAGS_SEPARATOR    = 1 << 7  /**< wuss_ICON_TYPE_MENU_ENTRY: draw a
                                          *   dashed rule along the entry's top
                                          *   edge, then the text as normal; the
                                          *   entry stays interactive. An entry
                                          *   carrying the flag with no text is
                                          *   a bare rule and is not hit-tested.
                                          *   Ignored by other types. */
}
wuss_icon_flags_t;

/**
 * Description of an icon at creation. Copied by value into the icon; the caller
 * keeps ownership of \c text, which is copied.
 *
 * A RISC OS-style validation string is deliberately omitted for now; a later \c
 * validation field would stay source-compatible for callers that
 * zero-initialise the spec.
 */
typedef struct wuss_icon_spec
{
  box_t             bbox;  /**< Bounding box, virtual document space,
                            *   inclusive-exclusive. */
  wuss_icon_type_t  type;  /**< Icon type. */
  const char       *text;  /**< NUL-terminated label; copied. NULL means "". */
  wuss_colour_t     fg;    /**< Text colour, as an index into the system
                            *   palette. */
  wuss_colour_t     bg;    /**< Fill/bevel base colour, as an index into the
                            *   system palette. A label, frame, radio or option
                            *   icon may pass wuss_NO_BACKGROUND for no fill
                            *   behind its text/glyph; a button or pattern icon
                            *   must pass a real index. */
  screen_pattern_t  pattern; /**< Tile for wuss_ICON_TYPE_PATTERN; ignored by
                              *   other types. Zero (screen_PATTERN_SOLID) is a
                              *   safe default for zero-initialised specs. */
  const bitmap_t   *bitmap; /**< wuss_ICON_TYPE_BITMAP: the image to draw.
                             *   Borrowed, not copied; must outlive the icon.
                             *   Ignored by other types; NULL (the default) is
                             *   only valid when type is not
                             *   wuss_ICON_TYPE_BITMAP. */
  int               group; /**< wuss_ICON_TYPE_RADIO: exclusive-selection group.
                            *   Selecting a radio clears every other selected
                            *   radio on the same window with the same group.
                            *   Zero (the default) means "no group": such a
                            *   radio still toggles but never clears another.
                            *   Ignored by all other icon types. */
  wuss_icon_flags_t flags; /**< Appearance/behaviour flags. */
}
wuss_icon_spec_t;

/* ----------------------------------------------------------------------- */

/**
 * Create an icon on a window. The icon is owned by the window and freed when
 * the window is closed (or the window manager destroyed). Its bounding box is
 * invalidated so the next redraw paints it.
 *
 * \param[in]  window Window to attach the icon to.
 * \param[in]  spec   Icon description; copied.
 * \param[out] icon   Filled in with the new icon handle, or NULL if the caller
 *                    does not need it.
 * \return \ref result_OK on success, \ref result_OOM on allocation failure,
 *         \ref result_WUSS_BAD_COLOUR if fg or bg is out of range for the
 *         palette, or \ref result_WUSS_BAD_ICON if type is unknown, a button or
 *         pattern spec has no fill colour, or a bitmap spec has no bitmap.
 */
result_t wuss_icon_create(wuss_window_t          *window,
                          const wuss_icon_spec_t *spec,
                          wuss_icon_t           **icon);

/**
 * Create several icons on a window in one call, as if by \ref wuss_icon_create
 * for each. Either all \c nspecs icons are created, or none are: on the first
 * failure any icons already created by this call are destroyed and no handles
 * are written.
 *
 * \param[in]  window Window to attach the icons to.
 * \param[in]  specs  Array of \c nspecs icon descriptions; each copied.
 * \param[in]  nspecs Number of entries in \c specs. Zero is a no-op.
 * \param[out] icons  Array of \c nspecs handles, filled in on success, or NULL
 *                    if the caller does not need them. Untouched on failure.
 * \return \ref result_OK on success, or the first failing \ref wuss_icon_create
 *         code (\ref result_OOM, \ref result_WUSS_BAD_COLOUR, \ref
 *         result_WUSS_BAD_ICON).
 */
result_t wuss_icon_create_array(wuss_window_t          *window,
                                const wuss_icon_spec_t *specs,
                                int                     nspecs,
                                wuss_icon_t           **icons);

/**
 * Destroy an icon, unlinking it from its window and invalidating its bounding
 * box so the next redraw clears it. Safe to pass NULL.
 *
 * \param[in] icon Icon to destroy, or NULL.
 */
void wuss_icon_delete(wuss_icon_t *icon);

/**
 * Replace an icon's label text. The new text is copied. Invalidates the icon's
 * bounding box.
 *
 * \param[in] icon Icon to change.
 * \param[in] text New NUL-terminated label; copied. NULL means "".
 * \return \ref result_OK on success, \ref result_OOM on allocation failure (the
 *         icon keeps its old text).
 */
result_t wuss_icon_set_text(wuss_icon_t *icon, const char *text);

/**
 * Show or hide an icon, toggling wuss_ICON_FLAGS_HIDDEN. Invalidates the icon's
 * bounding box.
 *
 * \param[in] icon   Icon to change.
 * \param[in] hidden Non-zero to hide the icon, zero to show it.
 */
void wuss_icon_set_hidden(wuss_icon_t *icon, int hidden);

/**
 * Fetch an icon's bounding box, in virtual document space.
 *
 * \param[in]  icon Icon to query.
 * \param[out] bbox Filled in with the bounding box.
 */
void wuss_icon_get_bbox(const wuss_icon_t *icon, box_t *bbox);

/**
 * Fetch an icon's type.
 *
 * \param[in] icon Icon to query.
 * \return The icon's type.
 */
wuss_icon_type_t wuss_icon_get_type(const wuss_icon_t *icon);

/**
 * Fetch an icon's current label text.
 *
 * \param[in] icon Icon to query.
 * \return The label, never NULL (may be ""). Owned by the icon; valid until the
 *         next wuss_icon_set_text or wuss_icon_delete on it.
 */
const char *wuss_icon_get_text(const wuss_icon_t *icon);

/**
 * Fetch the window an icon belongs to.
 *
 * \param[in] icon Icon to query.
 * \return The owning window.
 */
wuss_window_t *wuss_icon_get_window(const wuss_icon_t *icon);

/**
 * Fetch a radio or option icon's selected (latched) state.
 *
 * \param[in] icon Icon to query.
 * \return Non-zero if selected, zero otherwise. Always zero for icon types that
 *         have no latched state.
 */
int wuss_icon_get_selected(const wuss_icon_t *icon);

/**
 * Set a radio or option icon's selected state, invalidating it so the next
 * redraw repaints it. For a radio with a non-zero group, selecting it (passing
 * non-zero) also clears every other selected radio on the same window with that
 * group. No task event is delivered -- this is the programmatic path, distinct
 * from a user click. A no-op for icon types with no latched state.
 *
 * \param[in] icon     Icon to change.
 * \param[in] selected Non-zero to select, zero to deselect.
 */
void wuss_icon_set_selected(wuss_icon_t *icon, int selected);

#endif /* WUSS_ICONS */

#ifdef __cplusplus
}
#endif

#endif /* WUSS_ICON_H */
