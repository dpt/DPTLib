/* wuss/core/types.h -- wuss - data model: constants, flags and the core structs */

#ifndef WUSS_CORE_TYPES_H
#define WUSS_CORE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "base/debug.h"
#include "base/utils.h"
#include "datastruct/list.h"
#include "geom/box.h"
#include "geom/packer.h"
#include "geom/point.h"
#include "geom/size.h"
#include "framebuf/screen.h"
#include "framebuf/bmfont.h"
#include "framebuf/bmfontcache.h"
#include "utils/barith.h"

#include "wuss/wuss.h"
#include "wuss/window.h"
#include "wuss/task.h"
#include "wuss/message.h"

#include "../font/font.h"


#ifdef WUSS_FURNITURE
#include "../furniture.h"
#endif
#ifdef WUSS_ICONBAR
#include "../iconbar.h"
#endif
#ifdef WUSS_ICONS
#include "../icon.h"
#endif
#include "../menu.h"


#define WUSS_TITLE_MAX               255
#define WUSS_DEFAULT_TITLEBAR_HEIGHT 20

#define WUSS_DEFAULT_DOUBLE_CLICK_MS  400
#define WUSS_DEFAULT_DOUBLE_CLICK_PX  4
#define WUSS_DEFAULT_DRAG_THRESHOLD_PX 4

#define WUSS_DRAG_ANTS_ON  4 /* marching-ants dash pattern for a core drag box */
#define WUSS_DRAG_ANTS_OFF 4

#define WUSS_MAX_DIRTY 16 /* dirty regions tracked before further invalidations get merged into the last entry */

/* ponytail: fixed cap; if hit, remaining pieces are carried through
 * unclipped against further occluders rather than growing storage -- safe,
 * just some avoidable redraw work, never wrong */
#define WUSS_MAX_INVALIDATE_PIECES 32

#define WUSS_STACK_COUNT 3 /* wuss_stack_t values; indexes wuss_t::z_order */

#define WUSS_MESSAGE_DRAIN_CAP 256 /* deliveries in one drain before the
                                    * remainder is deferred to the next entry
                                    * point; see wuss__message_drain */

#define WUSS_PLACE_GUTTER 6 /* px left between windows auto-placed by wuss_window_create_at */

#define WUSS_BUTTON_INSET 3  /* shared by close/back/toggle/resize furniture buttons and scrollbar breadth */

#define WUSS_BUTTON_CLICK (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST) /* either click button; excludes MENU */

/* Font slot (see wuss_create's fonts[]) consulted for menu decoration glyphs:
 * the selection tick and the submenu arrow. When the slot is empty those are
 * drawn as vector strokes instead. A font in this slot is expected to carry a
 * checkmark glyph at codepoint WUSS_GLYPH_TICK and an arrowhead at
 * WUSS_GLYPH_SUBMENU; a font lacking either gets the vector strokes. */
#define WUSS_SYMBOL_FONT 2
#ifndef WUSS_GLYPH_TICK
#define WUSS_GLYPH_TICK    0x2713ul /* CHECK MARK */
#endif
#ifndef WUSS_GLYPH_SUBMENU
#define WUSS_GLYPH_SUBMENU 0x25B6ul /* BLACK RIGHT-POINTING TRIANGLE */
#endif

#ifdef WUSS_ICONS
#define WUSS_FRAME_CAPTION_TOP   4  /* y shift to make frame neatly meet caption */
#define WUSS_FRAME_CAPTION_INSET 8  /* x offset of a wuss_ICON_TYPE_FRAME caption from the frame's left edge */
#define WUSS_FRAME_CAPTION_PAD   22 /* gap left in the frame's top edge either side of the caption */

#define WUSS_SLIDER_GAP 4 /* fixed inset on all four sides of a wuss_ICON_TYPE_SLIDER bbox, between the surround and the inner (clickable) rect */

#endif
#define WUSS_MIN_CONTENT  20 /* resize-drag floor: content can never be squeezed smaller than this */
#define WUSS_SCROLL_INSET 2  /* sausage cross-axis margin from its well's edges, purely cosmetic */
#define WUSS_DIVIDER_PX   1  /* interior rule between the content area and the furniture on its right/bottom */

/* Internal per-window state, distinct from the public wuss_window_flags_t
 * appearance flags a caller sets at creation -- room to grow without
 * widening struct wuss_window by an int per flag. */
typedef enum wuss_window_state
{
  wuss_WINDOW_STATE_NONE    = 0,
  wuss_WINDOW_STATE_TOGGLED = 1 << 0, /* currently at TOGGLE_SIZE's "full" size */
  wuss_WINDOW_STATE_TASK_HIDDEN = 1 << 1 /* hidden by wuss_task_set_hidden */
}
wuss_window_state_t;

/* Internal wuss_task::flags bits. */
enum
{
  wuss_TASK__AUTOCLOSE = 1u << 0, /* self-destruct once the last window closes */
  wuss_TASK__REAPING   = 1u << 1  /* teardown in progress; suppress autoclose */
};

#ifdef WUSS_ICONS
/* A set of named, RLE-compressed icons loaded from a directory of PNGs: the
 * wuss-wide shared pool (wuss::icon) and, optionally, one private pool per
 * task (wuss_task::iconpool). Index i is the i-th ".png" in name order
 * (strcmp -- still address by name). names interns the leafnames-sans-".png";
 * atoms[i] is entry i's atom in it (atom values are not array indices, so the
 * position is tracked explicitly); bitmaps[i] is the matching RLE-compressed
 * bitmap. All owned; freed by wuss__iconpool_clear. NULL / 0 until loaded. */
struct wuss_iconpool
{
  struct atom_set *names;
  int             *atoms;
  bitmap_t        *bitmaps;
  int              nbitmaps;
};
#endif

/* A registered task. Owns a list of windows (linked through each window's
 * second embedded list_t, task_link) and is the single delivery target for
 * their events plus the app-wide IDLE/PALETTE/MENU_SELECT notifications.
 * Tasks are chained through wuss::tasks in registration order. */
struct wuss_task
{
  list_t             link;      /* anchor node in wuss::tasks; must be first */
  wuss_t            *wuss;
  wuss_window_fn_t  *handle;    /* nullable: no events delivered */
  void              *task_data;
  const char        *name;      /* borrowed, may be NULL */
  list_t             windows;   /* anchor; nodes are window->task_link */
  unsigned int       flags;     /* wuss_TASK__* */
  wuss_task_t       *parent;    /* NULL, or the task whose destruction takes
                                 * this one with it; see wuss_set_task_parent */
#ifdef WUSS_ICONS
  struct wuss_iconpool iconpool; /* private pool, empty until loaded; looked up
                                  * ahead of the shared pool for this task's
                                  * windows. See wuss_task_iconpool_load. */
#endif
};

struct wuss
{
  screen_t                   *scr;
  /* Borrowed root path from wuss_create, for wuss_get_resources; caller
   * convention only, unused internally. */
  const char                 *resources;
  /* Font slots from wuss_create; slot 0 is the system font. See font/font.h */
  struct wuss_fontset         fonts;
  /* Owned; shared loader for fonts tasks pick beyond the fixed fonts[] slots
   * above, e.g. via a font-picker menu. See wuss_get_font_cache. */
  bmfontcache_t              *font_cache;
  /* malloc/realloc/free hooks, copied in by wuss_create; used for every heap
   * block this wuss_t owns. */
  wuss_alloc_t                alloc;

  /* ----- palette and colours ----- */

  /* Owned. */
  colour_t                   *palette;
  int                         npalette;

  /* ----- clipboard ----- */

  /* Owned UTF-8 text; NULL while empty. See wuss_clipboard_set. */
  char                       *clipboard;
  /* Bumped by every wuss_clipboard_set, so a frontend can tell when to push
   * the text to the host. */
  unsigned int                clipboard_gen;

  /* Concrete 0..npalette-1 index for each symbolic wuss_colour_t, indexed by
   * (c - wuss_COLOUR_SYMBOLIC). Unused slots hold wuss_COLOUR_SYMBOLIC so a
   * stray symbolic still fails a range check. Rebuilt by
   * wuss__rebuild_palettecache on create and every palette/config change. */
  wuss_colour_t               palettecache[128];
  /* User fill override for each palette index (palettes hold at most 128
   * entries); colour==wuss_NO_BACKGROUND: none, use the automatic fill. See
   * wuss_set_colour_fill. */
  wuss_fill_t                 fills[128];
  /* Automatic fill for each palette index, matched against the screen's
   * physical palette (see wuss__rebuild_fills): a dither tile, or -- when
   * autosolid[i] -- just the solid physical colour in autofill[i].fg. Used
   * where fills[i].colour is wuss_NO_BACKGROUND. */
  pattern_t                   autofill[128];
  bool                        autosolid[128];
#ifdef WUSS_FURNITURE
  wuss_furniture_palette_t    furniture_colours;
#endif
#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
  /* Work-area bevel edge shades. */
  struct
  {
    /* Button top/left edge. */
    wuss_colour_t light;
    /* Button bottom/right edge. */
    wuss_colour_t dark;
    /* Lighter edge for a DIVIDER border. */
    wuss_colour_t divider;
  }
  bevel;
  /* Work-area button colours. */
  struct
  {
    /* Button face when the spec bg is NONE. */
    wuss_colour_t bg;
    /* Button label. */
    wuss_colour_t fg;
    /* Button face while held. */
    wuss_colour_t pressed;
  }
  button;
  /* Default action button fill. */
  wuss_colour_t               accent;
#endif
#ifdef WUSS_ICONS
  /* Slider icon colours. */
  struct
  {
    /* Groove ground. */
    wuss_colour_t track;
    /* Fill. */
    wuss_colour_t value;
    /* Bounding box and gap. */
    wuss_colour_t surround;
  }
  slider;
#endif
  /* Conventional body fills; read back as wuss_COLOUR_WINDOW / _MENU. */
  struct
  {
    /* Work-area body fill. */
    wuss_colour_t window;
    /* Menu body fill. */
    wuss_colour_t menu;
  }
  body;
  /* Desktop backdrop; colour==wuss_NO_BACKGROUND: none. */
  wuss_backdrop_t             backdrop;

  /* ----- input thresholds ----- */

  /* Resolved by wuss__apply_config; all > 0. */
  struct
  {
    int double_click_ms;
    int double_click_px;
    int drag_threshold_px;
  }
  input;
#ifdef WUSS_FURNITURE
  int                         titlebar_height;
#endif

  /* ----- windows and tasks ----- */

  /* Anchor per wuss_stack_t, frontmost stack first; head = frontmost window
   * of that stack. */
  list_t                      z_order[WUSS_STACK_COUNT];
  /* Anchor; registered tasks, in wuss_task_create order. */
  list_t                      tasks;
  /* Parent given to each task at wuss_task_create; see wuss_set_task_parent. */
  wuss_task_t                *task_parent;

  /* ----- message queue ----- */

  struct
  {
    /* Owned; array grown by wuss__array_grow, FIFO -- consumed from the
     * front, see wuss__message_drain. */
    wuss_message_t       *queue;
    int                   count;
    int                   cap;
    /* Next my_ref to hand out; skips 0 on wrap, see wuss__message_next_ref. */
    unsigned int          next_ref;
    /* >0 while inside a public entry point's call tree; sends queue rather
     * than deliver inline, queue drains only back at depth 0. */
    int                   dispatch_depth;
    /* The recorded message currently being handled by its recipient's
     * wuss_EVENT_MESSAGE handler, NULL otherwise; wuss_send /
     * wuss_send_recorded / wuss_acknowledge with your_ref == acking->my_ref
     * mark it acknowledged. */
    const wuss_message_t *acking;
    /* Set while acking != NULL if it has been acknowledged; read after the
     * handler returns to decide whether to bounce it. */
    int                   acked;
  }
  msg;
#ifdef WUSS_FURNITURE
  /* Drag state. */
  struct wuss__furniture      furniture;
  /* core->furniture dispatch; wuss_create defaults it to
   * wuss__furniture_default_ops. */
  const wuss__furniture_ops_t *furniture_ops;
#endif

  /* ----- redraw regions ----- */

  /* Accumulated by wuss_invalidate; reset by a redraw. */
  struct
  {
    box_t box[WUSS_MAX_DIRTY];
    int   n;
  }
  dirty;
  /* Accumulated by wuss__touch; reset by wuss_clear_touched. */
  struct
  {
    box_t box[WUSS_MAX_DIRTY];
    int   n;
  }
  touched;

  /* ----- pointer and focus ----- */

  /* Last pointer position, screen space, from any mouse click/move. */
  point_t                     pointer;
  /* Modifier keys held, from wuss_set_modifiers. */
  wuss_key_modifiers_t        modifiers;
  /* Set by wuss_set_time; arbitrary epoch, only differences matter. */
  unsigned int                now_ms;
  /* The last plain (non-double) DOWN, for double-click detection; see
   * mouse-click.c. */
  struct
  {
    /* now_ms at the DOWN. */
    unsigned int  ms;
    point_t       point;
    wuss_button_t button;
    /* Cleared to NULL by wuss_window_close like pointer_window/focus, so a
     * reused allocation can never false-match. */
    wuss_window_t *window;
  }
  last_down;
  /* Window whose on-screen footprint the pointer was last inside (content or
   * furniture), NULL if none; drives wuss_EVENT_POINTER_ENTER/EXIT. */
  wuss_window_t              *pointer_window;
  /* Window holding the input focus, NULL if none; see wuss_set_focus. */
  wuss_window_t              *focus;
  /* Window that took a Select/Adjust DOWN on its plain content (no icon hit);
   * it receives every mouse event, wherever the pointer goes, until the
   * matching MOUSE_UP. NULL when none. See capture.c. */
  wuss_window_t              *capture;
#ifdef WUSS_ICONS
  /* Button icon held down; released on any MOUSE_UP even if a new window now
   * covers its owner. */
  struct
  {
    /* NULL when idle. */
    wuss_icon_t   *icon;
    /* The window icon is on; NULL iff icon is NULL. */
    wuss_window_t *window;
    /* Screen point of the DOWN that set icon; a DRAGGABLE measures
     * input.drag_threshold_px from here before starting a core drag. */
    point_t        point;
    /* Button held from that DOWN; carried into drag.button if the press turns
     * into a core drag. */
    wuss_button_t  button;
  }
  pressed;
  /* Icon the pointer is currently over; drives hover-highlight repaint of
   * menu-entry icons. */
  struct
  {
    /* NULL when none. */
    wuss_icon_t   *icon;
    /* The window icon is on; NULL iff icon is NULL. */
    wuss_window_t *window;
  }
  hover;
  /* Writable holding the text caret. */
  struct
  {
    /* NULL when none. */
    wuss_icon_t   *icon;
    /* The window icon is on; NULL iff icon is NULL, and always the focus
     * window. */
    wuss_window_t *window;
    /* Caret byte offset into icon's text. */
    int            index;
    /* Other end of the selection: the text between anchor and index is
     * selected. Equal to index when nothing is. */
    int            anchor;
  }
  caret;
  /* Shared icon pool loaded by wuss_iconpool_load. */
  struct wuss_iconpool       icon;
#endif
#ifdef WUSS_MENUS
  /* Opaque; defined in menu/menu.c, reached only via wuss__menu_* (menu.h). */
  struct wuss__menu_session *menu;
#endif
#ifdef WUSS_ICONBAR
  struct
  {
    /* Pinned, chromeless window hosting the bar; created lazily on the first
     * wuss_iconbar_icon_create, NULL until then. */
    wuss_window_t        *window;
    /* Internal task owning window; created lazily alongside it, freed by
     * wuss_destroy's task sweep like menu.task. */
    wuss_task_t          *task;
    /* Owned; array of owned icon pointers, left-to-right slot order. */
    wuss_iconbar_icon_t **icons;
    int                   n;
    int                   cap;
    /* Held down, NULL when idle; released on any MOUSE_UP regardless of where
     * the pointer now is. */
    wuss_iconbar_icon_t  *pressed;
    /* Horizontal scroll offset in pixels; non-zero only while the icons
     * overflow the bar. */
    int                   scroll;
  }
  iconbar;
#endif
  /* Set by wuss_window_reveal_now / wuss_menu_open_window_now while a
   * wuss_EVENT_PRE_SHOW is being delivered, to opt in to the reveal; read and
   * cleared by wuss__window_set_hidden_ex once delivery returns. */
  int                         pre_show_proceed;

  /* ----- core drag ----- */

  struct
  {
    /* wuss_EVENT_DRAG_END recipient; NULL when no core drag is active -- the
     * sole "is a drag running" flag, see drag.c. */
    wuss_window_t *window;
    /* Ants box size, set at wuss_drag_start. */
    size2d_t       size;
    /* Button that started the drag, copied from pressed.button; reported in
     * wuss_EVENT_DRAG_END so a client can tell a SELECT drag from an ADJUST
     * one. */
    wuss_button_t  button;
    /* Offset within the box that tracks the pointer. */
    point_t        hotspot;
    /* Current ants box, screen space; recomputed on every move. */
    box_t          box;
    /* Box the ants are painted at: caught up with box once a frame by
     * wuss__drag_tick, which invalidates both boxes' edges (only) as it
     * does. */
    box_t          drawn;
    /* Counts wuss_idle calls while dragging, for the ants' marching phase. */
    int            frame;
  }
  drag;
};

struct wuss_window
{
  list_t              link;   /* z-order chain; must be first member */
  list_t              task_link; /* chain through the owning task's window
                                  * list (wuss_task::windows) */
  wuss_t             *wuss;
  box_t               visible; /* full on-screen footprint: content expanded
                                * outward by any titlebar/outline furniture */
  wuss_task_t        *task;   /* owning task, set at create, immutable; never
                              * NULL */
  wuss_backdrop_t     bg; /* content background; colour==wuss_NO_BACKGROUND: none */
  wuss_window_flags_t flags;
  wuss_stack_t        stack;  /* which wuss_t::z_order list link is on */
  point_t             scroll; /* offset into virtual content space of the
                               * content box's top-left; see wuss_window_set_scroll */
  size2d_t            doc;    /* virtual document extent, set at creation */
  size2d_t            min_doc; /* resize floor, set at creation; see
                                * wuss__min_content */
  wuss_window_state_t state;        /* see wuss_window_state_t */
#ifdef WUSS_FURNITURE
  box_t               pre_toggle;   /* visible bounds to restore on the next toggle */
  char                title[WUSS_TITLE_MAX + 1];
  wuss__furniture_layout_t furniture_layout; /* lazily built; see furniture.h */
#endif
#ifdef WUSS_ICONS
  wuss_icon_t       **icons;        /* owned; array of owned icon pointers */
  int                 nicons;
  int                 cap_icons;
#endif
};

#endif /* WUSS_CORE_TYPES_H */
