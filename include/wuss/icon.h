/* wuss/icon.h -- wuss work-area icons */

/**
 * \file icon.h
 *
 * Work-area icons: static labels and clickable bevelled buttons that Wuss
 * draws inside a window's content area and hit-tests before the content task
 * sees a click.
 *
 * An icon's bounding box is given in virtual document space -- the same
 * coordinate space as wuss_EVENT_MOUSE's point and wuss_window_invalidate's
 * local_box -- so an icon scrolls with the content it sits on. Its on-screen
 * position is (content.x0 - scroll.x + bbox), using the window's current
 * content bounds and scroll offset.
 *
 * Wuss fills a window's background, draws its icons, then delivers
 * wuss_EVENT_REDRAW, so a task is free to paint over or around icon pixels.
 * A click on a wuss_ICON_TYPE_ACTION reaches the task as wuss_EVENT_ICON;
 * clicks on a label, or on a hidden or disabled icon, fall through as
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
#include "geom/inset.h"
#include "geom/point.h"

#include "wuss/wuss.h"

/* The in-content icon subsystem is a compile-time option (CMake WUSS_ICONS).
 * With it off the library has no wuss_icon_* symbols, so this header body is
 * skipped. */
#ifdef WUSS_ICONS

/* ----------------------------------------------------------------------- */

/* wuss_icon_t (opaque, owned by the window it is created on) is forward-declared
 * in wuss.h so wuss_event_t can name it regardless of this option. */

/**
 * What an icon looks like and how it behaves. The enum is left open so
 * further types can be added later without breaking existing specs.
 */
typedef enum wuss_icon_type
{
  /** Static text drawn with the window manager's font. Not interactive: clicks
   *  fall through to the task as wuss_EVENT_MOUSE. */
  wuss_ICON_TYPE_LABEL = 0,
  /** Bevelled rectangle with a centred text label and pressed-state visual
   *  feedback; clicks and hovers are delivered to the task as
   *  wuss_EVENT_ICON. */
  wuss_ICON_TYPE_ACTION,
  /** Bounding box filled with a repeating two-colour 8x8 tile
   *  (spec.u.pattern.tile) in fg/bg, phased to document space so it scrolls
   *  rigidly with content. Not interactive: clicks fall through to the task as
   *  wuss_EVENT_MOUSE. text is ignored. */
  wuss_ICON_TYPE_PATTERN,
  /** A grouping box: a bevelled rectangle around the bounding box,
   *  broken at the top-left for an optional caption (text) drawn over the
   *  window background. Not interactive: clicks fall through to the task as
   *  wuss_EVENT_MOUSE. */
  wuss_ICON_TYPE_FRAME,
  /** A radio button: a small ring at the left of the bounding box, filled when
   *  selected, with the label (text) to its right. Interactive: a click selects
   *  it and clears every other selected radio sharing its non-zero group, then
   *  the task is told via wuss_EVENT_ICON. */
  wuss_ICON_TYPE_RADIO,
  /** An option button: a small box at the left of the bounding box, ticked when
   *  selected, with the label (text) to its right. Interactive: a click toggles
   *  its own selected state (group is ignored), then the task is told via
   *  wuss_EVENT_ICON. */
  wuss_ICON_TYPE_OPTION,
  /** A caller-owned bitmap (spec.u.bitmap.image) drawn at the top-left of the
   *  bounding box, alpha-blended against what is already there, clipped to the
   *  box; no scaling. The bitmap is borrowed, not copied, and must outlive the
   *  icon (unlike text). fg, bg, text and pattern are ignored. Not interactive
   *  unless wuss_ICON_FLAGS_INTERACTIVE is set, in which case clicks raise
   *  wuss_EVENT_ICON like a button. */
  wuss_ICON_TYPE_BITMAP,
  /** A menu row: text left-justified across the bounding box, drawn in fg over
   *  the window background, or inverted (window manager's title colours) while
   *  the pointer is over it. An optional tick at the left edge when the icon is
   *  selected (see wuss_icon_set_selected), and an optional submenu arrow at
   *  the right edge with wuss_ICON_FLAGS_SUBMENU. wuss_ICON_FLAGS_SEPARATOR
   *  marks the entry as following a group boundary: the dashed rule itself is a
   *  separate wuss_ICON_TYPE_RULE icon laid out above the entry, not drawn by
   *  the entry. The entry keeps its label and stays fully interactive.
   *  Interactive: a click raises wuss_EVENT_ICON like a button; a disabled
   *  entry never highlights and its clicks fall through. */
  wuss_ICON_TYPE_MENU_ENTRY,
  /** A horizontal dashed rule centred in the bounding box, drawn in fg over the
   *  window background. Purely decorative: never hit-tested, never highlights.
   *  text, bg and pattern are ignored. Used between menu rows to render the
   *  line a wuss_ICON_FLAGS_SEPARATOR entry sits below. */
  wuss_ICON_TYPE_RULE,
  /** A slider: the bounding box is filled with slider.surround, with a fixed
   *  4px gap (also slider.surround) around an inner sunken groove
   *  (spec.u.slider) that fills the remaining space on both axes; its fill
   *  marks a value between min and max. A click or drag anywhere in the
   *  inner rect -- fill or bare groove, but not the surround or gap -- jumps
   *  the value straight to the pointer position; the task is told via
   *  wuss_EVENT_ICON, continuously while dragging. */
  wuss_ICON_TYPE_SLIDER,
  /** An editable single-line text field: the bounding box filled with bg
   *  inside a 1px fg outline, the text left-aligned in fg. Its buffer is
   *  owned by the icon and holds up to spec.u.writable.size - 1 bytes,
   *  seeded from text; read it back with wuss_icon_get_text. A Select or
   *  Adjust click on a focusable window's writable places the caret (see
   *  wuss_icon_set_caret) at the nearest character. While it holds the
   *  caret, wuss_key edits the buffer -- printable Latin-1, Backspace,
   *  Delete, Left, Right, Home and End, Shift+Left / Shift+Right by words,
   *  Ctrl+Left / Ctrl+Right to the start / end and Ctrl+U to clear -- and
   *  Tab / Shift-Tab move the caret to the next / previous writable on the
   *  window; other keys reach the task as wuss_EVENT_KEY. Every edit raises
   *  wuss_EVENT_ICON with button 0. Text wider than the box scrolls to keep
   *  the caret in view. */
  wuss_ICON_TYPE_WRITABLE,

  /* The following types are reserved: the enum values and validation exist
   * but no rendering, hit-testing or event routing is wired up yet. A spec
   * using one is accepted and currently draws as a plain
   * wuss_ICON_TYPE_LABEL. */

  /** A read-only value field: a bevelled well showing text the task updates but
   *  the user cannot edit. Not yet implemented. */
  wuss_ICON_TYPE_DISPLAY,
  /** An editable numeric field, optionally with up/down adjusters. Not yet
   *  implemented. */
  wuss_ICON_TYPE_NUMBER,
  /** A field cycling through a fixed set of string values. Not yet
   *  implemented. */
  wuss_ICON_TYPE_STRING_SET,
  /** A free-drag handle: reports pointer motion to the task while dragged. Not
   *  yet implemented. */
  wuss_ICON_TYPE_DRAGGABLE
}
wuss_icon_type_t;

/**
 * The border drawn around a wuss_ICON_TYPE_LABEL, inside its bounding box.
 * Ignored by every other icon type. Uses the window manager's bevel shades
 * (wuss_config_t::bevel), the same as a button.
 */
typedef enum wuss_icon_border
{
  /** No border; the label fills its box as before. The default. */
  wuss_ICON_BORDER_NONE = 0,
  /** A 1px raised bevel: light top/left, dark bottom/right, like a button at
   *  rest. */
  wuss_ICON_BORDER_RIDGE,
  /** A 1px sunken bevel: dark top/left, light bottom/right -- a RISC OS-style
   *  read-only display field. */
  wuss_ICON_BORDER_GROOVE,
  /** A 6px-per-edge "action" surround: a 2px raised outset, a 2px moat filled
   *  with wuss_COLOUR_ACCENT, then a 2px raised inset -- like a RISC OS
   *  default-action button. */
  wuss_ICON_BORDER_ACTION,
  /** A 4px-per-edge "divider": two 2px bevels, an outer sunken ring wrapping an
   *  inner raised one. Drawn in a lighter pair of shades than RIDGE/GROOVE --
   *  bevel.divider against bevel.light rather than the full light/dark
   *  contrast -- so it reads as a soft inset panel rather than a hard button
   *  edge. */
  wuss_ICON_BORDER_DIVIDER,
  /** A 1px outline in the icon's fg colour, as drawn around a
   *  wuss_ICON_TYPE_WRITABLE. */
  wuss_ICON_BORDER_PLAIN
}
wuss_icon_border_t;

/** Icon appearance and behaviour flags, combinable with bitwise OR. */
typedef enum wuss_icon_flags
{
  wuss_ICON_FLAGS_NONE          = 0,
  /** Not drawn, not hit-tested. */
  wuss_ICON_FLAGS_HIDDEN        = 1 << 0,
  /** Drawn greyed; clicks fall through to the task as wuss_EVENT_MOUSE rather
   *  than raising wuss_EVENT_ICON. */
  wuss_ICON_FLAGS_DISABLED      = 1 << 1,
  /** wuss_ICON_TYPE_LABEL: left-align the text in the bounding box. This is
   *  the default (value 0); named for call sites that want to state the
   *  justification explicitly. */
  wuss_ICON_FLAGS_JUSTIFY_LEFT  = 0,
  /** wuss_ICON_TYPE_LABEL: right-align the text in the bounding box instead of
   *  the default left. */
  wuss_ICON_FLAGS_JUSTIFY_RIGHT = 1 << 2,
  /** wuss_ICON_TYPE_LABEL: centre the text in the bounding box. Takes
   *  precedence over wuss_ICON_FLAGS_JUSTIFY_RIGHT. */
  wuss_ICON_FLAGS_JUSTIFY_CENTRE = 1 << 3,
  /** wuss_ICON_TYPE_ACTION: draw as a default action button -- an accent fill
   *  (see wuss_config_t::accent) inside the same 6px "action" surround as
   *  wuss_ICON_BORDER_ACTION, instead of the ordinary 1px bevel. Ignored by
   *  other icon types. */
  wuss_ICON_FLAGS_DEFAULT       = 1 << 4,
  /** wuss_ICON_TYPE_BITMAP: hit-test the icon and raise wuss_EVENT_ICON on a
   *  click, like a button. Without it a bitmap icon is pure decoration and
   *  clicks fall through as wuss_EVENT_MOUSE. Ignored by other icon types
   *  (interactive or not by their nature). */
  wuss_ICON_FLAGS_INTERACTIVE  = 1 << 5,
  /** wuss_ICON_TYPE_MENU_ENTRY: draw a right-pointing arrow at the right edge,
   *  marking an entry that opens a submenu. Ignored by other types. */
  wuss_ICON_FLAGS_SUBMENU      = 1 << 6,
  /** wuss_ICON_TYPE_MENU_ENTRY: the entry follows a group boundary. It stays a
   *  normal interactive row with its own label; the dashed rule above it is
   *  laid out and drawn as a separate wuss_ICON_TYPE_RULE icon. Ignored by
   *  other types. */
  wuss_ICON_FLAGS_SEPARATOR    = 1 << 7,
  /** wuss_ICON_TYPE_MENU_ENTRY: draw a small colour chip (spec.u.menu_entry.swatch) in the
   *  row's left gutter, where the tick would sit. Mutually exclusive with a
   *  selected tick -- the chip wins. Ignored by other types. */
  wuss_ICON_FLAGS_SWATCH      = 1 << 8,
  /** Two-bit field (bits 9-10) selecting which of wuss_create's fonts draws
   *  this icon's text: 0 is the system font, 1-3 the further slots. Build it
   *  with wuss_ICON_FONT(n); read it with wuss_ICON_FONT_OF(flags). A slot
   *  that was not filled falls back to the system font. */
  wuss_ICON_FLAGS_FONT_MASK   = 3 << 9
}
wuss_icon_flags_t;

/**
 * Encode font slot \p n (0..3) as icon flags; OR into wuss_icon_spec::flags.
 */
#define wuss_ICON_FONT(n)       (((n) & 3) << 9)

/** Decode the font slot (0..3) from an icon's flags. */
#define wuss_ICON_FONT_OF(f)    (((f) >> 9) & 3)

/**
 * Encode a 0-based icon-set index (from \ref wuss_icons_lookup) for
 * wuss_icon_spec_data::bitmap::set. The stored value is offset by one so a
 * zero-initialised spec reads as "no icon-set entry".
 */
#define wuss_ICON_SET(i)        ((i) + 1)

/**
 * A wuss_ICON_TYPE_SLIDER's axis: which way its groove runs and, so, which
 * end of the groove (inset from the bounding box by the fixed 4px gap on all
 * four sides, filling the rest) is pixel 0 of the value fill.
 */
typedef enum wuss_slider_orientation
{
  /** Groove runs left-to-right; value grows rightward, pixel 0 at the
   *  groove's left. */
  wuss_SLIDER_HORIZONTAL = 0,
  /** Groove runs top-to-bottom; value grows upward (low at the bottom, high
   *  at the top), matching a volume-slider convention -- pixel 0 at the
   *  groove's bottom. */
  wuss_SLIDER_VERTICAL
}
wuss_slider_orientation_t;

/**
 * Per-type payload in a wuss_icon_spec. Exactly one arm applies, selected by
 * wuss_icon_spec::type; the arms mirror the icon's internal storage. Types
 * with no type-specific data (ACTION, FRAME, OPTION, RULE, and the reserved
 * types) touch no arm, so a zero-initialised spec is valid for them. Each
 * arm is its own struct so a type can gain fields without disturbing the
 * others.
 */
typedef union wuss_icon_spec_data
{
  /** wuss_ICON_TYPE_LABEL */
  struct
  {
    /** Border drawn inside the bounding box. Zero (wuss_ICON_BORDER_NONE) is
     *  the default for a zero-initialised spec. */
    wuss_icon_border_t border;
  }
  label;

  /** wuss_ICON_TYPE_PATTERN */
  struct
  {
    /** Repeating two-colour 8x8 tile. Zero (screen_PATTERN_SOLID) is a safe
     *  default for a zero-initialised spec. */
    screen_pattern_t tile;
  }
  pattern;

  /** wuss_ICON_TYPE_BITMAP */
  struct
  {
    /** The image to draw. Borrowed, not copied; must outlive the icon. NULL
     *  (the default) falls back to \c set. */
    const bitmap_t *image;
    /** When \c image is NULL, draw an entry from the window manager's loaded
     *  icon set (see \ref wuss_icons_load). Zero (the default for a
     *  zero-initialised spec) means "no icon-set entry"; encode a 0-based
     *  index from \ref wuss_icons_lookup with \ref wuss_ICON_SET. Ignored
     *  when \c image is set. */
    int             set;
  }
  bitmap;

  /** wuss_ICON_TYPE_RADIO */
  struct
  {
    /** Exclusive-selection group. Selecting a radio clears every other
     *  selected radio on the same window with the same group. Zero (the
     *  default) means "no group": such a radio still toggles but never clears
     *  another. */
    int group;
  }
  radio;

  /** wuss_ICON_TYPE_MENU_ENTRY */
  struct
  {
    /** With wuss_ICON_FLAGS_SWATCH: the colour chip to draw in the left
     *  gutter, as an index into the system palette. Ignored unless that flag
     *  is set. */
    wuss_colour_t swatch;
  }
  menu_entry;

  /** wuss_ICON_TYPE_SLIDER */
  struct
  {
    /** Which way the groove runs. wuss_SLIDER_HORIZONTAL (0) is the default
     *  for a zero-initialised spec. */
    wuss_slider_orientation_t orientation;
    /** Value at the groove's start (left for wuss_SLIDER_HORIZONTAL, bottom
     *  for VERTICAL). May be greater than max to run the fill backwards. */
    int                       min;
    /**
     * Value at the groove's end (right for HORIZONTAL, top for VERTICAL).
     */
    int                       max;
    /** Initial value, clamped to [min,max] (or [max,min] if min > max). */
    int                       default_value;
  }
  slider;

  /** wuss_ICON_TYPE_WRITABLE */
  struct
  {
    /** Buffer size in bytes, including the terminator: the field holds at
     *  most size - 1 characters. Must be at least 1. */
    int size;
  }
  writable;
}
wuss_icon_spec_data_t;

/**
 * Description of an icon at creation. Copied by value into the icon; the
 * caller keeps ownership of \c text, which is copied.
 *
 * A RISC OS-style validation string is deliberately omitted for now; a later
 * \c validation field would stay source-compatible for callers that
 * zero-initialise the spec.
 */
typedef struct wuss_icon_spec
{
  /** Bounding box, virtual document space, inclusive-exclusive. */
  box_t                 bbox;
  /** Icon type. */
  wuss_icon_type_t      type;
  /** NUL-terminated label; copied. NULL means "". */
  const char           *text;
  /** Text colour, as an index into the system palette. */
  wuss_colour_t         fg;
  /** Fill/bevel base colour, as an index into the system palette. A label,
   *  frame, radio or option icon may pass wuss_NO_BACKGROUND for no fill behind
   *  its text/glyph; a button or pattern icon must pass a real index. */
  wuss_colour_t         bg;
  /** Appearance/behaviour flags. */
  wuss_icon_flags_t     flags;
  /** Per-type payload, selected by \c type. */
  wuss_icon_spec_data_t u;
}
wuss_icon_spec_t;

/* ----------------------------------------------------------------------- */

/** Standard main-axis size (px) for a wuss_ICON_TYPE_SLIDER. */
#define wuss_STD_SLIDER_HEIGHT           18

/** Standard main-axis size (px) for a non-default wuss_ICON_TYPE_ACTION
 *  button, e.g. Cancel. */
#define wuss_STD_SECONDARY_BUTTON_HEIGHT 26

/** Standard main-axis size (px) for a wuss_ICON_FLAGS_DEFAULT
 *  wuss_ICON_TYPE_ACTION button, e.g. OK/Apply. */
#define wuss_STD_PRIMARY_BUTTON_HEIGHT   34

/** Standard gap (px) between two sibling icons/components in a layout. */
#define wuss_STD_GAP                     4

/** Standard gap (px) between an icon/component and the window edge. */
#define wuss_STD_INSET                   4

/** wuss_STD_INSET on every edge, e.g. a stack_item_t.pad value for a
 *  window's root item. */
#define wuss_STD_INSETS \
  INSET(wuss_STD_INSET, wuss_STD_INSET, wuss_STD_INSET, wuss_STD_INSET)

/** Standard gap (px) between a wuss_ICON_TYPE_FRAME's contents and the
 *  frame's own border/caption, on every edge except for the top. */
#define wuss_STD_FRAME_INSET             4

/** Standard gap (px) between a wuss_ICON_TYPE_FRAME's contents and the
 *  frame's own border/caption, on the top edge. */
#define wuss_STD_FRAME_TOP_INSET         8

/** wuss_STD_FRAME_INSET plus wuss_STD_INSET on every edge except the top,
 *  which gets a further 6px, e.g. a stack_item_t.pad value for a frame's
 *  contents below a caption. */
#define wuss_STD_FRAME_INSETS \
  INSET(wuss_STD_FRAME_INSET + wuss_STD_FRAME_TOP_INSET, \
        wuss_STD_FRAME_INSET + wuss_STD_INSET, \
        wuss_STD_FRAME_INSET + wuss_STD_INSET, \
        wuss_STD_FRAME_INSET + wuss_STD_INSET)

/* ----------------------------------------------------------------------- */

/**
 * Create an icon on a window. The icon is owned by the window and freed when
 * the window is closed (or the window manager destroyed). Its bounding box
 * is invalidated so the next redraw paints it.
 *
 * \param[in]  window Window to attach the icon to.
 * \param[in]  spec   Icon description; copied.
 * \param[out] icon   Filled in with the new icon handle, or NULL if the
 *                    caller does not need it.
 * \return \ref result_OK on success, \ref result_OOM on allocation failure,
 *         \ref result_WUSS_BAD_COLOUR if fg or bg is out of range for the
 *         palette, or \ref result_WUSS_BAD_ICON if type is unknown, a button
 *         or pattern spec has no fill colour, or a bitmap spec has no
 *         bitmap.
 */
result_t wuss_icon_create(wuss_window_t          *window,
                          const wuss_icon_spec_t *spec,
                          wuss_icon_t           **icon);

/**
 * Create several icons on a window in one call, as if by \ref
 * wuss_icon_create for each. Either all \c nspecs icons are created, or none
 * are: on the first failure any icons already created by this call are
 * destroyed and no handles are written.
 *
 * \param[in]  window Window to attach the icons to.
 * \param[in]  specs  Array of \c nspecs icon descriptions; each copied.
 * \param[in]  nspecs Number of entries in \c specs. Zero is a no-op.
 * \param[out] icons  Array of \c nspecs handles, filled in on success, or
 *                    NULL if the caller does not need them. Untouched on
 *                    failure.
 * \return \ref result_OK on success, or the first failing \ref
 *         wuss_icon_create code (\ref result_OOM, \ref
 *         result_WUSS_BAD_COLOUR, \ref result_WUSS_BAD_ICON).
 */
result_t wuss_icon_create_array(wuss_window_t          *window,
                                const wuss_icon_spec_t *specs,
                                int                     nspecs,
                                wuss_icon_t           **icons);

/**
 * Draw an icon from a spec once, retaining nothing: no allocation, no icon
 * added to the window. For static, non-interactive content a task can redraw
 * cheaply from its own model -- a large grid of swatches, say -- without a
 * live icon per cell.
 *
 * Call only from a task's wuss_EVENT_REDRAW handler, passing that event's \c
 * bounds and \c scroll straight through; the icon is painted through the
 * window manager's screen with the redraw clip already in force. The spec is
 * validated exactly as \ref wuss_icon_create validates it. The icon's \c
 * bbox is in virtual document space, as for a created icon.
 *
 * \param[in] window  Window being redrawn.
 * \param[in] spec    Icon description; not retained.
 * \param[in] content The redraw event's \c bounds (full content box, screen
 *                    space).
 * \param[in] scroll  The redraw event's \c scroll offset.
 * \return \ref result_OK, or the same \ref result_WUSS_BAD_ICON / \ref
 *         result_WUSS_BAD_COLOUR \ref wuss_icon_create would return.
 */
result_t wuss_icon_plot(wuss_window_t          *window,
                        const wuss_icon_spec_t *spec,
                        const box_t            *content,
                        point_t                 scroll);

/**
 * Destroy an icon, unlinking it from its window and invalidating its
 * bounding box so the next redraw clears it. Safe to pass NULL for \p icon.
 *
 * \param[in] window Window the icon belongs to.
 * \param[in] icon   Icon to destroy, or NULL.
 */
void wuss_icon_delete(wuss_window_t *window, wuss_icon_t *icon);

/**
 * Replace an icon's label text. The new text is copied. Invalidates the
 * icon's bounding box. A wuss_ICON_TYPE_WRITABLE truncates the text to its
 * buffer and, if it holds the caret, moves the caret to the end.
 *
 * \param[in] window Window the icon belongs to.
 * \param[in] icon   Icon to change.
 * \param[in] text   New NUL-terminated label; copied. NULL means "".
 * \return \ref result_OK on success, \ref result_OOM on allocation failure
 *         (the icon keeps its old text).
 */
result_t wuss_icon_set_text(wuss_window_t *window,
                            wuss_icon_t   *icon,
                            const char    *text);

/**
 * Show or hide an icon, toggling wuss_ICON_FLAGS_HIDDEN. Invalidates the
 * icon's bounding box.
 *
 * \param[in] window Window the icon belongs to.
 * \param[in] icon   Icon to change.
 * \param[in] hidden Non-zero to hide the icon, zero to show it.
 */
void wuss_icon_set_hidden(wuss_window_t *window,
                          wuss_icon_t   *icon,
                          int            hidden);

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
 * \return The label, never NULL (may be ""). Owned by the icon; valid until
 *         the next wuss_icon_set_text or wuss_icon_delete on it.
 */
const char *wuss_icon_get_text(const wuss_icon_t *icon);

/**
 * Fetch a radio or option icon's selected (latched) state.
 *
 * \param[in] icon Icon to query.
 * \return Non-zero if selected, zero otherwise. Always zero for icon types
 *         that have no latched state.
 */
int wuss_icon_get_selected(const wuss_icon_t *icon);

/**
 * Set a radio or option icon's selected state, invalidating it so the next
 * redraw repaints it. For a radio with a non-zero group, selecting it
 * (passing non-zero) also clears every other selected radio on the same
 * window with that group. No task event is delivered -- this is the
 * programmatic path, distinct from a user click. A no-op for icon types with
 * no latched state.
 *
 * \param[in] window   Window the icon belongs to.
 * \param[in] icon     Icon to change.
 * \param[in] selected Non-zero to select, zero to deselect.
 */
void wuss_icon_set_selected(wuss_window_t *window,
                            wuss_icon_t   *icon,
                            int            selected);

/**
 * Fetch a slider icon's current value.
 *
 * \param[in] icon Icon to query.
 * \return The value, in [min,max] (or [max,min]) as given at creation.
 *         Always 0 for icon types with no value.
 */
int wuss_icon_get_value(const wuss_icon_t *icon);

/**
 * Set a slider icon's value programmatically, invalidating it so the next
 * redraw repaints it. No task event is delivered -- this is the programmatic
 * path, distinct from a user click or drag. A no-op for icon types with no
 * value.
 *
 * \param[in] window Window the icon belongs to.
 * \param[in] icon   Icon to change.
 * \param[in] value  New value; clamped to the icon's [min,max] range.
 */
void wuss_icon_set_value(wuss_window_t *window,
                         wuss_icon_t   *icon,
                         int            value);

/**
 * Place the text caret in a wuss_ICON_TYPE_WRITABLE icon, giving its window
 * the input focus, or remove the caret. There is one caret per window
 * manager; it is removed whenever its window loses the focus. No task event
 * is delivered for the caret move itself.
 *
 * \param[in] window Window the icon belongs to.
 * \param[in] icon   Writable icon to take the caret, or NULL to remove the
 *                   caret if it is on \p window.
 * \param[in] index  Caret position in bytes; negative or past the end of the
 *                   text means the end.
 * \return \ref result_OK, or \ref result_BAD_ARG if \p icon is not a
 *         visible, enabled writable or \p window cannot take the focus.
 */
result_t wuss_icon_set_caret(wuss_window_t *window,
                             wuss_icon_t   *icon,
                             int            index);

/* ----------------------------------------------------------------------- */

/**
 * Scan a directory for PNG images and load them as the window manager's icon
 * set. Each ".png" file becomes one indexed entry: its pixels are loaded and
 * then RLE-compressed in place (see \ref bitmap_compress), and its leafname
 * sans ".png" is recorded so \ref wuss_icons_lookup can resolve it back to
 * the index. Non-".png" entries are ignored. Entry indices are assigned in
 * the order the platform's directory scan yields, which is unspecified --
 * address entries by name, not by a hardcoded index.
 *
 * Calling this again replaces the previous set (the old bitmaps and names
 * are freed). \ref wuss_destroy frees the set.
 *
 * \param[in] wuss Window manager.
 * \param[in] dir  Directory to scan for "*.png".
 * \return \ref result_OK on success (including an empty or missing
 *         directory, which yields a zero-length set), \ref result_OOM on
 *         allocation failure, or a code propagated from the PNG loader or
 *         the compressor (the set is left empty on failure).
 */
result_t wuss_icons_load(wuss_t *wuss, const char *dir);

/**
 * Convenience wrapper for \ref wuss_icons_load: loads the wuss-wide icon set
 * from "<resources>/resources/wuss/icons", the fixed location every demo
 * task uses. \p resources is joined via \c pathf and copied before the load,
 * so the caller need not worry about \c pathf's single shared return buffer.
 *
 * \param[in] wuss      Window manager.
 * \param[in] resources Resource root, as returned by \ref
 *                      wuss_get_resources.
 * \return As \ref wuss_icons_load.
 */
result_t wuss_icons_load_resource(wuss_t *wuss, const char *resources);

/**
 * Number of entries in the loaded icon set (see \ref wuss_icons_load). Zero
 * if none has been loaded.
 *
 * \param[in] wuss Window manager.
 * \return The entry count.
 */
int wuss_icons_count(const wuss_t *wuss);

/**
 * Resolve an icon-set entry's name to its index.
 *
 * \param[in] wuss Window manager.
 * \param[in] name Entry name: a loaded file's leafname without ".png".
 * \return The 0-based index, or -1 if no entry has that name. Pass the index
 *         through \ref wuss_ICON_SET to put it in a spec's \c icon_set.
 */
int wuss_icons_lookup(const wuss_t *wuss, const char *name);

/**
 * Fetch an icon-set entry's (compressed) bitmap by index.
 *
 * \param[in] wuss  Window manager.
 * \param[in] index 0-based index, 0..\ref wuss_icons_count -1.
 * \return The bitmap, owned by the window manager and valid until the next
 *         \ref wuss_icons_load or \ref wuss_destroy, or NULL if \p index is
 *         out of range.
 */
const bitmap_t *wuss_icons_bitmap(const wuss_t *wuss, int index);

#endif /* WUSS_ICONS */

#ifdef __cplusplus
}
#endif

#endif /* WUSS_ICON_H */
