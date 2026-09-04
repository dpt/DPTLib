/* wuss/wuss.h -- minimal window manager */

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

#include <stddef.h>

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

/* ----------------------------------------------------------------------- */

/** Window/resize dimensions too small. */
#define result_WUSS_TOO_SMALL  (result_BASE_WUSS + 0)
/** A palette index was out of range. */
#define result_WUSS_BAD_COLOUR (result_BASE_WUSS + 1)
/**
 * An icon spec was malformed (unknown type, or BUTTON without a fill
 * colour).
 */
#define result_WUSS_BAD_ICON   (result_BASE_WUSS + 2)

/* ----------------------------------------------------------------------- */

/** A window manager instance. */
typedef struct wuss wuss_t;

/** A window. Full API is in window.h. */
typedef struct wuss_window wuss_window_t;

/** A registered task: owns windows, receives their events. Full API is in
 * task.h. */
typedef struct wuss_task wuss_task_t;

/** A work-area icon. Full API is in icon.h, and is compiled only when the
 * library is built with the WUSS_ICONS option on. */
typedef struct wuss_icon wuss_icon_t;

/**
 * Allocator hooks used by a wuss_t for every heap block it owns (the
 * instance itself, windows, icons, menu nodes) and by the wuss shared
 * components. \c malloc, \c realloc and \c free must behave like their C
 * library namesakes -- same argument and return conventions, realloc(NULL,
 * n) == malloc(n), free(NULL) a no-op. Passed to wuss_create and copied in;
 * NULL there selects wuss_alloc (plain stdlib).
 */
typedef struct wuss_alloc
{
  void *(*malloc)(size_t size);
  void *(*realloc)(void *ptr, size_t size);
  void  (*free)(void *ptr);
}
wuss_alloc_t;

/** The default allocator: the C library malloc / realloc / free. */
extern const wuss_alloc_t wuss_alloc;

/**
 * Mouse buttons, RISC OS-style: Select is the primary action, Adjust the
 * secondary action, Menu pops up a menu.
 *
 * These are flags, OR'd together, so that chords (e.g. Select and Adjust
 * pressed together) can be reported. The bit values match the RISC OS button
 * order. Test them with '&' rather than comparing for equality, or a chord
 * will match no button at all.
 */
typedef enum wuss_button
{
  wuss_BUTTON_NONE   = 0,
  wuss_BUTTON_ADJUST = 1 << 0,
  wuss_BUTTON_MENU   = 1 << 1,
  wuss_BUTTON_SELECT = 1 << 2
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

/**
 * An index into a wuss_t's system palette (see wuss_create). Not a colour_t.
 */
typedef unsigned char wuss_colour_t;

/**
 * Sentinel for wuss_window_create's bg meaning "no automatic background
 * fill".
 */
#define wuss_NO_BACKGROUND ((wuss_colour_t) -1)

/**
 * Symbolic wuss_colour_t values. A raw wuss_colour_t is a system-palette
 * index, 0..wuss_COLOUR_SYMBOLIC-1 (so a palette may hold up to 128 real
 * entries). Values from wuss_COLOUR_SYMBOLIC up are not palette indices but
 * roles the window manager resolves to a concrete index against the live
 * palette (and, for the wuss_COLOUR_*_ chrome roles, the live wuss_config):
 * the named colours pick the nearest system-palette entry to the RGB the
 * name implies, the chrome roles echo the matching wuss_config_t field.
 * Accepted anywhere a wuss_colour_t is: config furniture/bevel/accent/
 * backdrop, window backgrounds (see wuss_window_create), icon specs.
 * wuss_NO_BACKGROUND is not symbolic and always passes through unchanged.
 */
#define wuss_COLOUR_SYMBOLIC ((wuss_colour_t) 128)

/* Named colours: nearest system-palette entry to the named RGB. */
#define wuss_COLOUR_BLACK   (wuss_COLOUR_SYMBOLIC + 0)
#define wuss_COLOUR_WHITE   (wuss_COLOUR_SYMBOLIC + 1)
#define wuss_COLOUR_RED     (wuss_COLOUR_SYMBOLIC + 2)
#define wuss_COLOUR_GREEN   (wuss_COLOUR_SYMBOLIC + 3)
#define wuss_COLOUR_BLUE    (wuss_COLOUR_SYMBOLIC + 4)
#define wuss_COLOUR_YELLOW  (wuss_COLOUR_SYMBOLIC + 5)
#define wuss_COLOUR_CYAN    (wuss_COLOUR_SYMBOLIC + 6)
#define wuss_COLOUR_MAGENTA (wuss_COLOUR_SYMBOLIC + 7)
#define wuss_COLOUR_GREY    (wuss_COLOUR_SYMBOLIC + 8)

/* Chrome roles: echo the matching wuss_config_t field, resolved to a
 * concrete index -- e.g. wuss_COLOUR_TITLE_BG is furniture.title.bg,
 * wuss_COLOUR_BUTTON_HILIGHT is bevel.light, wuss_COLOUR_BUTTON_SHADOW is
 * bevel.dark, wuss_COLOUR_BACKDROP is backdrop.colour. */
#define wuss_COLOUR_TITLE_BG       (wuss_COLOUR_SYMBOLIC + 16)
#define wuss_COLOUR_TITLE_FG       (wuss_COLOUR_SYMBOLIC + 17)
#define wuss_COLOUR_BUTTON_HILIGHT (wuss_COLOUR_SYMBOLIC + 18)
#define wuss_COLOUR_BUTTON_SHADOW  (wuss_COLOUR_SYMBOLIC + 19)
#define wuss_COLOUR_ACCENT_BG      (wuss_COLOUR_SYMBOLIC + 20)
#define wuss_COLOUR_ACCENT_FG      (wuss_COLOUR_SYMBOLIC + 21)
#define wuss_COLOUR_BACKDROP       (wuss_COLOUR_SYMBOLIC + 22)

/** Furniture chrome colours, one entry per class of furniture. Title is
 * the only two-tone class (fill + text); the rest are drawn as a single
 * flat colour. Ignored when the library is built with WUSS_FURNITURE off.
 * Each value is an index into the system palette (see
 * wuss_create). */
typedef struct wuss_furniture_palette
{
  struct
  {
    wuss_colour_t bg;       /**< Titlebar fill. */
    wuss_colour_t fg;       /**< Titlebar text. */
  }
  title;
  wuss_colour_t back;       /**< Send-to-back icon. */
  wuss_colour_t close;      /**< Close icon. */
  wuss_colour_t toggle;     /**< Toggle-size icon. */
  wuss_colour_t resize;     /**< Resize icon. */
  struct
  {
    wuss_colour_t arrows;   /**< Scrollbar arrows. */
    wuss_colour_t wells;    /**< Scrollbar wells. */
    wuss_colour_t sausages; /**< Scrollbar sausages. */
  }
  scroll;
}
wuss_furniture_palette_t;

/**
 * Per-window appearance flags, combinable with bitwise OR.
 *
 * \note When the library is built with the WUSS_FURNITURE CMake option off,
 *       every window is chromeless regardless of these flags and the
 *       wuss_WINDOW_NO_* bits are ignored.
 */
typedef enum wuss_window_flags
{
  /** Default: every furniture region drawn. */
  wuss_WINDOW_NONE           = 0,

  /**
   * No titlebar; content fills the full visible area, and no drag handle
   * exists.
   */
  wuss_WINDOW_NO_TITLEBAR    = 1 << 0,

  /** No 1px border drawn around the visible area. */
  wuss_WINDOW_NO_OUTLINE     = 1 << 1,

  /**
   * No close icon in the titlebar. Ignored if flags includes
   * wuss_WINDOW_NO_TITLEBAR.
   */
  wuss_WINDOW_NO_CLOSE       = 1 << 2,

  /**
   * No send-to-back icon in the titlebar. Ignored if flags includes
   * wuss_WINDOW_NO_TITLEBAR.
   */
  wuss_WINDOW_NO_BACK        = 1 << 3,

  /**
   * No toggle-size icon in the titlebar. Ignored if flags includes
   * wuss_WINDOW_NO_TITLEBAR.
   */
  wuss_WINDOW_NO_TOGGLE_SIZE = 1 << 4,

  /** No vertical scrollbar on the right edge. */
  wuss_WINDOW_NO_VSCROLL     = 1 << 5,

  /** No horizontal scrollbar on the bottom edge. */
  wuss_WINDOW_NO_HSCROLL     = 1 << 6,

  /** No resize icon in the bottom-right corner. */
  wuss_WINDOW_NO_RESIZE      = 1 << 7,

  /**
   * A resize (drag or toggle-size) always fully redraws the window's content
   * instead of blitting the preserved region -- for a task whose rendering
   * depends on the window's size in ways redraw can't patch incrementally
   * (e.g. a palette that lays itself out across the whole window).
   */
  wuss_WINDOW_NO_RESIZE_BLIT = 1 << 8,

  /**
   * Created hidden, or hidden later via wuss_window_set_hidden: the window
   * keeps its place in the z-order but is not drawn and not hit-tested, so
   * it neither occludes other windows nor catches the pointer. Its position
   * can still be changed with wuss_window_move while hidden, ready for when
   * it is shown again. Unlike the wuss_WINDOW_NO_* bits this one is toggled
   * at runtime, and it is honoured regardless of the WUSS_FURNITURE build
   * option.
   */
  wuss_WINDOW_HIDDEN         = 1 << 9
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

/**
 * Desktop background specification: a flat colour, or an 8x8 fill pattern.
 * Used by wuss_config_t. Always honoured regardless of the WUSS_FURNITURE /
 * WUSS_ICONS options.
 */
typedef struct wuss_backdrop
{
  /**
   * Fill colour, or wuss_NO_BACKGROUND to leave the background untouched
   * (the caller must then repaint it itself before wuss_redraw /
   * wuss_redraw_dirty). When pattern is not screen_PATTERN_SOLID this is the
   * pattern's foreground (set-bit) colour.
   */
  wuss_colour_t    colour;

  /**
   * Fill pattern. screen_PATTERN_SOLID (the default) fills flat in colour;
   * any other value tiles that pattern in colour over pattern_bg, phased to
   * a fixed screen origin so it stays put across dirty-region redraws.
   * Ignored when colour is wuss_NO_BACKGROUND.
   */
  screen_pattern_t pattern;

  /** Pattern background (clear-bit) colour; used only when pattern is not
   *  screen_PATTERN_SOLID. */
  wuss_colour_t    pattern_bg;
}
wuss_backdrop_t;

/** A flat-colour wuss_backdrop_t (or wuss_NO_BACKGROUND for none), as a
 *  compound literal -- the common case where no fill pattern is wanted. */
#define wuss_BACKDROP_COLOUR(c) \
  ((wuss_backdrop_t) { (c), screen_PATTERN_SOLID, wuss_NO_BACKGROUND })

/** A patterned wuss_backdrop_t: 8x8 pattern p tiled in colour c over
 *  background colour b. */
#define wuss_BACKDROP_PATTERN(c, p, b) ((wuss_backdrop_t) { (c), (p), (b) })

/**
 * Optional creation-time configuration.
 *
 * \note titlebar_height and palette are ignored when the library is built
 *       with WUSS_FURNITURE off; bevel and accent are ignored when built
 *       with both WUSS_FURNITURE and WUSS_ICONS off. backdrop is always
 *       honoured. See the backdrop sub-struct for its own notes.
 */
typedef struct wuss_config
{
  /**
   * Titlebar height in pixels, or 0 to derive from font metrics (or a
   * built-in fallback if no font). Ignored when WUSS_FURNITURE is off.
   */
  int            titlebar_height;

  /** Furniture chrome colours. Ignored when WUSS_FURNITURE is off. */
  wuss_furniture_palette_t furniture;

  /**
   * Bevelled work-area button edge shades, as indices into the system
   * palette: light on the top/left edges, dark on the bottom/right (swapped
   * when the button is pressed). Both default to the titlebar fill colour
   * when config is NULL. Ignored when both WUSS_FURNITURE and WUSS_ICONS are
   * off.
   */
  struct
  {
    wuss_colour_t light; /**< Top/left bevel edge. */
    wuss_colour_t dark;  /**< Bottom/right bevel edge. */
  }
  bevel;

  /**
   * Fill and text colours for a default action button -- a work-area button
   * icon created with wuss_ICON_FLAGS_DEFAULT, drawn to stand out from the
   * ordinary bevelled buttons around it (RISC OS's "default action button").
   * Both default to the titlebar colours (bg / fg) when config is NULL.
   * Ignored when both WUSS_FURNITURE and WUSS_ICONS are off.
   */
  struct
  {
    wuss_colour_t bg; /**< Default-button fill. */
    wuss_colour_t fg; /**< Default-button text. */
  }
  accent;

  /** Desktop background, painted behind windows on every redraw. */
  wuss_backdrop_t backdrop;
}
wuss_config_t;

/** Most fonts wuss_create will take, and the range an icon's font-select
 *  flags can name (see wuss_ICON_FONT). */
#define wuss_MAX_FONTS 4

/**
 * What a font passed to wuss_create is for. Lets a shared component such as
 * a font-picker menu tell a decorative/chrome font apart from one meant to
 * be offered as a user-selectable text face.
 */
typedef enum wuss_font_class
{
  wuss_FONT_CLASS_NONE, /**< An ordinary text font. */

  /**
   * Chrome/decoration only, e.g. the symbol font menu ticks and submenu
   * arrows are drawn from (see \ref WUSS_SYMBOL_FONT). Not meant to be
   * offered as a text face.
   */
  wuss_FONT_CLASS_SYSTEM
}
wuss_font_class_t;

/**
 * One font slot passed to wuss_create: the font itself, what it is for, and
 * the name a caller-side picker (e.g. wuss_fontmenu) should know it by.
 */
typedef struct wuss_font_desc
{
  /** Font handle. Not owned; must outlive the wuss_t. */
  bmfont_t *font;

  wuss_font_class_t font_class; /**< What the font is for. */

  /**
   * Borrowed; the font's leafname sans ".png", for a picker to match
   * against. NULL if the slot has no name (e.g. a NONE-class font that no
   * picker needs to skip).
   */
  const char *name;
}
wuss_font_desc_t;

/**
 * Create a window manager.
 *
 * \param[in]  scr      Screen to draw windows onto. Not owned; must outlive
 *                      the wuss_t.
 * \param[in]  fonts    Up to \ref wuss_MAX_FONTS font slots, copied into the
 *                      wuss_t (the descriptors, not the fonts -- the fonts
 *                      are not owned and must outlive it). Slot 0 is the
 *                      system font, used for titlebar labels and any icon
 *                      that does not select another. NULL, or nfonts 0,
 *                      leaves titlebars unlabelled.
 * \param[in]  nfonts   Number of entries in \p fonts, 0..\ref
 *                      wuss_MAX_FONTS; more than that is an error.
 * \param[in]  palette  System palette, copied in, or NULL to use a built-in
 *                      default palette.
 * \param[in]  npalette Number of entries in palette. Ignored if palette is
 *                      NULL.
 * \param[in]  config   Creation-time configuration, or NULL for defaults.
 * \param[in]  alloc    Allocator hooks, copied in, or NULL for \ref
 *                      wuss_alloc (plain stdlib). Must outlive nothing --
 *                      only the three function pointers are kept.
 * \param[out] wuss     Newly created window manager.
 * \return \ref result_OK on success, \ref result_BAD_ARG if \p nfonts is
 *         negative or exceeds \ref wuss_MAX_FONTS, \ref
 *         result_WUSS_BAD_COLOUR if any of config's palette entries are out
 *         of range for the palette, or another appropriate result code.
 */
result_t wuss_create(screen_t               *scr,
                     const wuss_font_desc_t *fonts,
                     int                     nfonts,
                     const colour_t         *palette,
                     int                     npalette,
                     const wuss_config_t    *config,
                     const wuss_alloc_t     *alloc,
                     wuss_t                **wuss);

/**
 * Replace the system palette partway through a session.
 *
 * Copies \p palette in over the existing one (same semantics as
 * wuss_create's palette argument), refreshes the cached nearest-black /
 * nearest-white indices, broadcasts a \ref wuss_EVENT_PALETTE event once to
 * every registered task (in registration order, window == NULL) so they can
 * recache any wuss_nearest_colour selections, then invalidates the whole
 * screen. The caller is still responsible for the next wuss_redraw /
 * wuss_redraw_dirty, and -- on a paletted screen -- for updating the screen
 * bitmap's own palette to match.
 *
 * \param[in] wuss     Window manager.
 * \param[in] palette  New system palette, copied in.
 * \param[in] npalette Number of entries in palette. Must equal the count
 *                     passed to wuss_create.
 * \return \ref result_OK on success, \ref result_BAD_ARG if npalette does
 *         not match the current palette length, \ref result_WUSS_BAD_COLOUR
 *         if a configured furniture/bevel/backdrop colour index is now out
 *         of range (in which case the palette is left unchanged), else the
 *         first non-OK result returned by a task's handle callback
 *         (iteration still continues past it).
 */
result_t wuss_set_palette(wuss_t         *wuss,
                          const colour_t *palette,
                          int             npalette);

/**
 * Replace the desktop backdrop partway through a session.
 *
 * Validates \p backdrop against the current palette (as wuss_create does its
 * config->backdrop), copies it in over the existing one, then invalidates
 * the whole screen so the next wuss_redraw / wuss_redraw_dirty repaints it
 * behind every window. Tasks are not notified. The caller is still
 * responsible for the next redraw.
 *
 * \param[in] wuss     Window manager.
 * \param[in] backdrop New backdrop, copied in. Set its colour to
 *                     wuss_NO_BACKGROUND for no backdrop.
 * \return \ref result_OK on success, \ref result_WUSS_BAD_COLOUR if a colour
 *         index in \p backdrop is out of range (the backdrop is left
 *         unchanged).
 */
result_t wuss_set_backdrop(wuss_t *wuss, const wuss_backdrop_t *backdrop);

/**
 * Destroy a window manager, and any windows still open on it.
 *
 * \param[in] doomed Window manager to destroy.
 */
void wuss_destroy(wuss_t *doomed);

/**
 * Fetch the system font (see wuss_create), for tasks to draw their own
 * content in the same face as window titlebars. Equivalent to
 * wuss_get_font_n(wuss, 0).
 *
 * \param[in] wuss Window manager.
 * \return System font, or NULL if none was given to wuss_create.
 */
bmfont_t *wuss_get_font(const wuss_t *wuss);

/**
 * Fetch one of the fonts passed to wuss_create by slot.
 *
 * \param[in] wuss  Window manager.
 * \param[in] index Font slot, 0..\ref wuss_MAX_FONTS - 1.
 * \return The font in that slot, or NULL if the slot is out of range or was
 *         not filled.
 */
bmfont_t *wuss_get_font_n(const wuss_t *wuss, int index);

/**
 * Fetch the class of one of the fonts passed to wuss_create by slot (see
 * \ref wuss_font_desc_t).
 *
 * \param[in] wuss  Window manager.
 * \param[in] index Font slot, 0..\ref wuss_MAX_FONTS - 1.
 * \return The slot's class, or \ref wuss_FONT_CLASS_NONE if the slot is out
 *         of range or was not filled.
 */
wuss_font_class_t wuss_get_font_class_n(const wuss_t *wuss, int index);

/**
 * Fetch the name of one of the fonts passed to wuss_create by slot (see \ref
 * wuss_font_desc_t).
 *
 * \param[in] wuss  Window manager.
 * \param[in] index Font slot, 0..\ref wuss_MAX_FONTS - 1.
 * \return The slot's name (borrowed, valid until wuss_destroy), or NULL if
 *         the slot is out of range, was not filled, or was given no name.
 */
const char *wuss_get_font_name_n(const wuss_t *wuss, int index);

/**
 * The last pointer position seen by wuss_mouse_click or wuss_mouse_move,
 * screen space. (0,0) until the first mouse event. Handy for opening a
 * pop-up menu under the pointer from a task's icon handler, which gets no
 * coordinate of its own.
 *
 * \param[in] wuss Window manager.
 * \return Last pointer position, screen space.
 */
point_t wuss_get_pointer(const wuss_t *wuss);

/**
 * Find the system palette entry (see wuss_create) closest to an RGB value,
 * by squared Euclidean distance in RGB space. Alpha is ignored. Ties keep
 * the lower index.
 *
 * \param[in] wuss Window manager.
 * \param[in] r    Red component, 0..255.
 * \param[in] g    Green component, 0..255.
 * \param[in] b    Blue component, 0..255.
 * \return Palette index, 0..npalette-1.
 */
wuss_colour_t wuss_nearest_colour(const wuss_t *wuss, int r, int g, int b);

/**
 * Redraw every window, back-to-front, having first painted the configured
 * backdrop colour (see wuss_config_t::backdrop) behind them, if any.
 *
 * \param[in] wuss Window manager.
 * \return \ref result_OK on success, or the last non-OK result returned by a
 *         task's redraw callback (drawing continues past a failing window
 *         rather than stopping).
 */
result_t wuss_redraw(wuss_t *wuss);

/**
 * Mark a screen-space region dirty. Window creation, destruction, move,
 * resize and bring-to-front invalidate their own affected regions
 * automatically; tasks must call this themselves when their content changes
 * (e.g. an animation), passing the union of the old and new screen-space
 * areas that need repainting.
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
 * dirty. Each dirty region has the configured backdrop colour (see
 * wuss_config_t::backdrop) painted into it first, if any.
 *
 * \param[in] wuss Window manager.
 * \return \ref result_OK on success, or the last non-OK result returned by a
 *         task's redraw callback.
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
 * Fetch one of the current accumulated dirty regions (see wuss_invalidate).
 * If no backdrop colour was configured (see wuss_config_t::backdrop), wuss
 * only repaints windows, not background between/behind them, so a caller
 * whose invalidations can expose background (e.g. after a window move)
 * should clear these regions itself before calling wuss_redraw_dirty.
 *
 * \param[in]  wuss  Window manager.
 * \param[in]  index Index of the region to fetch, 0 to
 *                   wuss_get_dirty_count() - 1.
 * \param[out] out   Filled in with the dirty region.
 */
void wuss_get_dirty(const wuss_t *wuss, int index, box_t *out);

/**
 * Deliver a mouse-down or mouse-up event (action must be wuss_MOUSE_DOWN or
 * wuss_MOUSE_UP). Hit-tests the topmost window at (x,y). On a down, a
 * titlebar click brings the window to front if button is Select (Adjust and
 * Menu leave the z-order unchanged) and starts a drag; on an up, an
 * in-progress drag is ended instead of hit-testing (an Adjust click with no
 * move in between sends the window to the back rather than dragging it). A
 * click on the window's content never changes the z-order and is delivered
 * to the task in window-local content coordinates.
 *
 * \param[in]  wuss   Window manager.
 * \param[in]  p      Screen coordinate.
 * \param[in]  button Button pressed or released.
 * \param[in]  action wuss_MOUSE_DOWN or wuss_MOUSE_UP.
 * \param[out] hit    Window under the pointer (or being dragged), or NULL if
 *                    none. May be NULL if not needed.
 * \return \ref result_OK, or a result code returned by the task's mouse
 *         callback.
 */
result_t wuss_mouse_click(wuss_t             *wuss,
                          point_t             p,
                          wuss_button_t       button,
                          wuss_mouse_action_t action,
                          wuss_window_t     **hit);

/**
 * Deliver a mouse-move event. Updates the dragged window's position if a
 * drag is active (invalidating the affected region; call wuss_redraw_dirty
 * to actually repaint it), otherwise hit-tests and delivers to the window's
 * task as per wuss_mouse_click.
 *
 * \param[in]  wuss Window manager.
 * \param[in]  p    Screen coordinate.
 * \param[out] hit  Window under the pointer (or being dragged), or NULL if
 *                  none. May be NULL if not needed.
 * \return \ref result_OK, or a result code returned by the task's mouse
 *         callback.
 */
result_t wuss_mouse_move(wuss_t *wuss, point_t p, wuss_window_t **hit);

/**
 * Deliver a scroll event. Hit-tests the topmost window at p as per
 * wuss_mouse_click, and delivers to the window's task in window-local
 * content coordinates; dropped if the hit window has no scroll callback, or
 * the pointer is over its titlebar.
 *
 * \param[in]  wuss  Window manager.
 * \param[in]  p     Screen coordinate.
 * \param[in]  delta Scroll amount; sign and units are caller-defined.
 * \param[out] hit   Window under the pointer, or NULL if none. May be NULL
 *                   if not needed.
 * \return \ref result_OK, or a result code returned by the task's scroll
 *         callback.
 */
result_t wuss_scroll(wuss_t         *wuss,
                     point_t         p,
                     int             delta,
                     wuss_window_t **hit);

/**
 * Broadcast a wuss_EVENT_IDLE event once to every registered task, in
 * registration order (window == NULL). Intended to be called once per
 * main-loop iteration, after other pending input has been handled, so tasks
 * can drive their own animation/timers -- iterating their own window lists
 * -- without the caller stepping each one individually.
 *
 * \param[in] wuss Window manager whose tasks should go idle.
 * \return \ref result_OK on success, else the first non-OK result returned
 *         by a task's handle callback (iteration still continues past it).
 */
result_t wuss_idle(wuss_t *wuss);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_WUSS_H */
