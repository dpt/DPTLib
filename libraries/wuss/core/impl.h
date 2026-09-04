/* wuss/impl.h -- wuss - minimal window manager */

#ifndef IMPL_H
#define IMPL_H

#include <stddef.h>
#include <string.h>

#include "base/utils.h"
#include "datastruct/list.h"
#include "geom/box.h"
#include "geom/packer.h"
#include "geom/point.h"
#include "geom/size.h"
#include "framebuf/screen.h"
#include "framebuf/bmfont.h"
#include "utils/barith.h"

#include "wuss/wuss.h"
#include "wuss/window.h"
#include "wuss/task.h"

#ifdef WUSS_FURNITURE
#include "../furniture.h"
#endif
#ifdef WUSS_ICONS
#include "../icon.h"
#endif
#ifdef WUSS_MENUS
#include "../menu.h"
#endif

#define WUSS_TITLE_MAX               63
#define WUSS_DEFAULT_TITLEBAR_HEIGHT 20

#define WUSS_MAX_DIRTY 16 /* dirty regions tracked before further invalidations get merged into the last entry */

/* ponytail: fixed cap; if hit, remaining pieces are carried through
 * unclipped against further occluders rather than growing storage -- safe,
 * just some avoidable redraw work, never wrong */
#define WUSS_MAX_INVALIDATE_PIECES 32

#define WUSS_PLACE_GUTTER 6  /* px left between windows auto-placed by wuss_window_create_placed */

#define WUSS_BUTTON_INSET 3  /* shared by close/back/toggle/resize furniture buttons and scrollbar breadth */

#ifdef WUSS_ICONS
#define WUSS_FRAME_CAPTION_INSET 8 /* x offset of a wuss_ICON_TYPE_FRAME caption from the frame's left edge */
#define WUSS_FRAME_CAPTION_PAD   2 /* gap left in the frame's top edge either side of the caption */
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
  wuss_WINDOW_STATE_TOGGLED = 1 << 0 /* currently at TOGGLE_SIZE's "full" size */
}
wuss_window_state_t;

/* Internal wuss_task::flags bits. */
enum
{
  wuss_TASK__AUTOCLOSE = 1u << 0, /* self-destruct once the last window closes */
  wuss_TASK__REAPING   = 1u << 1  /* teardown in progress; suppress autoclose */
};

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
};

struct wuss
{
  screen_t                   *scr;
  bmfont_t                   *fonts[wuss_MAX_FONTS]; /* slot 0 is the system
                                          * font; unset slots NULL; not owned */
  int                         nfonts;    /* entries filled from wuss_create */
  wuss_alloc_t                alloc;     /* malloc/realloc/free hooks, copied in
                                          * by wuss_create; used for every heap
                                          * block this wuss_t owns */
  colour_t                   *palette;   /* owned */
  int                         npalette;
  /* Concrete 0..npalette-1 index for each symbolic wuss_colour_t, indexed by
   * (c - wuss_COLOUR_SYMBOLIC). Unused slots hold wuss_COLOUR_SYMBOLIC so a
   * stray symbolic still fails a range check. Rebuilt by
   * wuss__rebuild_palettecache on create and every palette/config change. */
  wuss_colour_t               palettecache[128];
#ifdef WUSS_FURNITURE
  wuss_furniture_palette_t              furniture_colours;
#endif
#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
  wuss_colour_t               bevel_light; /* work-area button top/left edge */
  wuss_colour_t               bevel_dark;  /* work-area button bottom/right edge */
  wuss_colour_t               accent_bg;   /* default action button fill */
  wuss_colour_t               accent_fg;   /* default action button text */
#endif
  wuss_backdrop_t             backdrop; /* colour==wuss_NO_BACKGROUND: none */
#ifdef WUSS_FURNITURE
  int                         titlebar_height;
#endif
  list_t                      z_order;   /* anchor; head = topmost window */
  list_t                      tasks;     /* anchor; registered tasks, in
                                          * wuss_task_create order */
#ifdef WUSS_FURNITURE
  struct wuss__furniture         furniture;    /* drag state */
  const wuss__furniture_ops_t   *furniture_ops; /* core->furniture dispatch;
                                                 * wuss_create defaults it to
                                                 * wuss__furniture_default_ops */
#endif
  box_t                       dirty[WUSS_MAX_DIRTY]; /* accumulated by wuss_invalidate; reset by a redraw */
  int                         ndirty;
  packer_t                   *layout;    /* owned; occupied screen area for
                                          * wuss_window_create_placed, lazily
                                          * created on first auto-placement */
  point_t                     cascade;   /* next cascade offset, used once the
                                          * layout packer has no room left */
  point_t                     pointer;   /* last pointer position, screen
                                          * space, from any mouse click/move */
#ifdef WUSS_ICONS
  wuss_icon_t                *pressed_icon; /* button icon held down, NULL when
                                            * idle; released on any MOUSE_UP
                                            * even if a new window now covers
                                            * its owner */
  wuss_icon_t                *hover_icon;   /* icon the pointer is currently
                                            * over, NULL when none; drives
                                            * hover-highlight repaint of
                                            * menu-entry icons */
#endif
#ifdef WUSS_MENUS
  struct wuss__menu          *menu_chain;   /* head (root) of the open pop-up
                                            * menu chain, NULL when none open;
                                            * consulted by mouse-click.c to
                                            * dismiss on a click-away */
  wuss_task_t                *menu_task;    /* internal task owning every
                                            * borderless menu window; created
                                            * lazily on first wuss_menu_open,
                                            * freed by wuss_destroy's task
                                            * sweep */
  int                         menu_eat_up;  /* a menu opened on the last
                                            * MOUSE_DOWN; swallow its matching
                                            * MOUSE_UP so the release does not
                                            * immediately pick row 0 */
#endif
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
  point_t             scroll; /* offset into virtual content space of the
                               * content box's top-left; see wuss_window_set_scroll */
  size2d_t            doc;    /* virtual document extent, set at creation */
  size2d_t            min_doc; /* resize floor, set at creation; see
                                * wuss__min_content */
#ifdef WUSS_FURNITURE
  wuss_window_state_t state;        /* see wuss_window_state_t */
#endif
  box_t               packed;       /* region wuss_window_create_placed took
                                     * out of wuss->layout (footprint + gutter),
                                     * to give back on close/move; empty if not
                                     * auto-placed or already released */
#ifdef WUSS_FURNITURE
  box_t               pre_toggle;   /* visible bounds to restore on the next toggle */
  char                title[WUSS_TITLE_MAX + 1];
#endif
#ifdef WUSS_ICONS
  wuss_icon_t       **icons;        /* owned; array of owned icon pointers */
  int                 nicons;
  int                 cap_icons;
#endif
};

/* Thin wrappers over the furniture_ops dispatch so core call sites that only
 * poke furniture for a redraw don't each need their own #ifdef WUSS_FURNITURE
 * guard -- they compile to nothing in a no-furniture build. The routing hooks
 * (hit_test, toggle_size, drag_*) stay unwrapped: their call sites sit inside
 * furniture-only logic that is already whole-block guarded. Named "chrome"
 * rather than "furniture" to steer clear of the wuss__furniture_* dispatch
 * targets these forward to. */
#ifdef WUSS_FURNITURE
static inline void wuss__chrome_draw(wuss_t        *wuss,
                                     wuss_window_t *window,
                                     const box_t   *full)
{
  wuss->furniture_ops->draw(wuss, window, full);
}

static inline void wuss__chrome_repaint(wuss_window_t *window)
{
  window->wuss->furniture_ops->invalidate(window);
}

static inline void wuss__chrome_repaint_for(wuss_window_t *window,
                                            const box_t   *visible)
{
  window->wuss->furniture_ops->invalidate_for(window, visible);
}
#else
static inline void wuss__chrome_draw(wuss_t        *wuss,
                                     wuss_window_t *window,
                                     const box_t   *full)
{
  (void) wuss;
  (void) window;
  (void) full;
}

static inline void wuss__chrome_repaint(wuss_window_t *window)
{
  (void) window;
}

static inline void wuss__chrome_repaint_for(wuss_window_t *window,
                                            const box_t   *visible)
{
  (void) window;
  (void) visible;
}
#endif

wuss_window_t *wuss__window_at(wuss_t *wuss, point_t p);

/* Rebuild wuss->palettecache (white, black and the symbolic[] table) from
 * the current palette and the stored chrome colours. Call after the palette
 * or any chrome colour changes; the chrome fields must already be concrete
 * indices (resolve config through wuss__resolve_colour before storing). */
void wuss__rebuild_palettecache(wuss_t *wuss);

/* Concrete 0..npalette-1 palette index for any wuss_colour_t: a symbolic
 * value (>= wuss_COLOUR_SYMBOLIC, bar wuss_NO_BACKGROUND) via the cache,
 * everything else -- raw indices, wuss_NO_BACKGROUND -- unchanged. */
static inline wuss_colour_t wuss__resolve_colour(const wuss_t *wuss,
                                                 wuss_colour_t c)
{
  if (c >= wuss_COLOUR_SYMBOLIC && c != wuss_NO_BACKGROUND)
    return wuss->palettecache[c - wuss_COLOUR_SYMBOLIC];
  return c;
}

/* Allocation through a wuss_t's configured hooks (see struct wuss::alloc).
 * Every heap block a wuss_t owns -- windows, icons, icon-pointer arrays, menu
 * nodes -- goes through these so a caller can supply its own allocator. */
static inline void *wuss__malloc(const wuss_t *wuss, size_t size)
{
  return wuss->alloc.malloc(size);
}

static inline void *wuss__realloc(const wuss_t *wuss, void *ptr, size_t size)
{
  return wuss->alloc.realloc(ptr, size);
}

static inline void wuss__free(const wuss_t *wuss, void *ptr)
{
  wuss->alloc.free(ptr);
}

/* Duplicate a NUL-terminated string through an allocator's malloc hook
 * (strdup is POSIX, not C99, so it is not assumed). Returns NULL on OOM or a
 * NULL s. Takes wuss_alloc_t, not wuss_t, so the shared components (which
 * keep a copy of the hooks, not the wuss_t) can use it too. */
static inline char *wuss__alloc_strdup(const wuss_alloc_t *a, const char *s)
{
  size_t len;
  char  *copy;

  if (s == NULL)
    return NULL;

  len  = strlen(s) + 1;
  copy = a->malloc(len);
  if (copy != NULL)
    memcpy(copy, s, len);
  return copy;
}

/* array_grow (utils/array.h) done through an allocator's realloc hook: the
 * next-power-of-two doubling, same element/return semantics (0 ok, 1 OOM).
 * used/need/minimum are element counts. See libraries/utils/array/grow.c. */
static inline int wuss__array_grow(const wuss_alloc_t *a,
                                   void              **block,
                                   size_t              elemsize,
                                   int                 used,
                                   int                *allocated,
                                   int                 need,
                                   int                 minimum)
{
  int   to_allocate;
  void *grown;

  need += used;
  if (need < minimum)
    need = minimum;
  if (need <= *allocated)
    return 0;

  to_allocate = power2gt(need - 1);

  grown = a->realloc(*block, elemsize * (size_t) to_allocate);
  if (grown == NULL)
    return 1;

  *block     = grown;
  *allocated = to_allocate;
  return 0;
}

/* Range-check a backdrop spec against the palette: its colour, and -- when a
 * non-solid pattern is set -- its pattern index and background colour.
 * Returns result_OK or result_WUSS_BAD_COLOUR. */
result_t wuss__validate_backdrop(const wuss_t          *wuss,
                                 const wuss_backdrop_t *backdrop);

/* Paint "backdrop" into "area" on "scr": a flat fill in the SOLID case,
 * otherwise the 8x8 pattern tiled in colour over pattern_bg, phased against
 * (origin_x, origin_y). No-op when colour is wuss_NO_BACKGROUND. Caller sets
 * scr->clip. */
void wuss__fill_backdrop(screen_t              *scr,
                         const colour_t        *palette,
                         const wuss_backdrop_t *backdrop,
                         const box_t           *area,
                         int                    origin_x,
                         int                    origin_y);

/* clamp "desired" to the window's scrollable range; step the current offset
 * by "delta" and apply it. Core (furniture-independent) -- used by the wheel
 * and, when built, the scrollbar furniture. */
point_t wuss__scroll_clamp(const wuss_window_t *window, point_t desired);
void    wuss__scroll_step(wuss_window_t *window, point_t delta);

#ifdef WUSS_FURNITURE
void            wuss__titlebar_box(const wuss_window_t *window, box_t *out);
void            wuss__close_box(const wuss_window_t *window, box_t *out);
void            wuss__content_box(const wuss_window_t *window, box_t *out);
#else
/* No furniture: the content area is the whole visible footprint. */
static inline void wuss__content_box(const wuss_window_t *window, box_t *out)
{
  *out = window->visible;
}
#endif
void            wuss__invalidate_clipped(wuss_window_t *window,
                                         const box_t   *box);
void            wuss__invalidate_minus(wuss_t      *wuss,
                                       const box_t *whole,
                                       const box_t *keep);
void            wuss__invalidate_uncovered(wuss_window_t *window);

/* Clip "box" (screen space) down to the parts not already covered by
 * windows above "window" in the z-order, writing the surviving pieces to
 * "out" (capacity WUSS_MAX_INVALIDATE_PIECES) and returning their count. */
int             wuss__clip_to_visible(wuss_window_t *window,
                                      const box_t   *box,
                                      box_t         *out);

/* Subtract each of "cuts" (an array of "ncuts" boxes) from "whole", writing
 * the surviving pieces to "out" (capacity WUSS_MAX_INVALIDATE_PIECES) and
 * returning their count. */
int             wuss__subtract_boxes(const box_t *whole,
                                     const box_t *cuts,
                                     int          ncuts,
                                     box_t       *out);

/* Given "n" single-rect blits, each moving "clean[i]" to "dest[i]", find an
 * order in which no blit's destination overwrites a still-unread source of a
 * later blit. Writes the piece indices to "order" (capacity
 * WUSS_MAX_INVALIDATE_PIECES) and returns non-zero on success; returns zero
 * when the overlap graph has a cycle and no safe order exists. */
int             wuss__order_pieces(const box_t *clean,
                                   const box_t *dest,
                                   int          n,
                                   int         *order);

/* Slide the pixels of "src" (nsrc pieces the caller has already carved clear
 * of occluders and stale regions) by (dx, dy), clipping each destination
 * against the screen and the windows above "window", clobber-ordered so no
 * blit steps on another's source; if "clip" is non-NULL scr->clip is pinned
 * to it for the copy (and restored after), else the clip is left alone. On
 * success
 * fills "copied" (capacity WUSS_MAX_INVALIDATE_PIECES) with the pieces
 * actually blitted, sets "*ncopied" and returns 1 -- the caller repaints
 * whatever "copied" misses. Returns 0 (with "*ncopied" zeroed) when there is
 * no safe fast path, and the caller must fall back to a full invalidate. */
int             wuss__blit_pieces(wuss_window_t *window,
                                  const box_t   *src,
                                  int            nsrc,
                                  int            dx,
                                  int            dy,
                                  const box_t   *clip,
                                  box_t         *copied,
                                  int           *ncopied);

/* Recover the owning wuss_window_t from a node in a task's window list
 * (window->task_link). task_link is not the first member, so a plain cast
 * won't do. */
#define wuss__window_from_task_link(node) \
  ((wuss_window_t *) ((char *) (node) - offsetof(struct wuss_window, task_link)))

/* Recover the owning wuss_window_t from a node in the z-order chain
 * (window->link). link is the first member, so this is just a cast -- but
 * naming it keeps every z-order walk honest about depending on that. */
#define wuss__window_from_link(node) \
  ((wuss_window_t *) (void *) (node))

/* The single dispatch chokepoint. Delivers "ev" to "task"'s handle, passing
 * "win_or_null" as the window (NULL for task-view events). No-op (returns
 * result_OK) when task is NULL or has no handle. A debug build also asserts
 * ev->kind is valid for the recipient view. Every emit site routes through
 * here. */
result_t wuss__deliver(wuss_task_t        *task,
                       wuss_window_t      *win_or_null,
                       const wuss_event_t *ev);

/* Notify a window's task that it has been moved or resized, via
 * wuss_EVENT_OPEN; the return value is discarded, matching how furniture
 * drawing and other in-line notifications are treated. */
static inline void wuss__notify_open(wuss_window_t *window)
{
  wuss_event_t event;

  event.kind = wuss_EVENT_OPEN;
  (void) wuss__deliver(window->task, window, &event);
}

static inline int wuss__size_ok(int width, int height)
{
  return width > 0 && height > 0;
}

/* Give an auto-placed window's slot back to the layout packer and stop
 * tracking it, so a later close/move/resize doesn't release it twice. A
 * no-op for windows that were never auto-placed (empty "packed"). */
static inline void wuss__release_packed(wuss_window_t *window)
{
  if (box_is_empty(&window->packed))
    return;

  (void) packer_release(window->wuss->layout, &window->packed);
  box_reset(&window->packed);
}

/* The floor a resize-drag or toggle-size will shrink a window's content to:
 * the client's min_doc where it set one, but never below WUSS_MIN_CONTENT (a
 * window must stay big enough to grab) nor above the window's own doc extent
 * (a window can't be forced larger than the document it shows). */
static inline void wuss__min_content(const wuss_window_t *window,
                                     size2d_t            *min)
{
  min->w = CLAMP(window->min_doc.w, WUSS_MIN_CONTENT, MAX(window->doc.w,
                                                          WUSS_MIN_CONTENT));
  min->h = CLAMP(window->min_doc.h, WUSS_MIN_CONTENT, MAX(window->doc.h,
                                                          WUSS_MIN_CONTENT));
}

#ifdef WUSS_FURNITURE
static inline int wuss__titlebar_height_for(const wuss_t       *wuss,
                                            wuss_window_flags_t flags)
{
  return (flags & wuss_WINDOW_NO_TITLEBAR) ? 0 : wuss->titlebar_height;
}

static inline int wuss__titlebar_height(const wuss_window_t *window)
{
  return wuss__titlebar_height_for(window->wuss, window->flags);
}

static inline int wuss__window_toggled(const wuss_window_t *window)
{
  return (window->state & wuss_WINDOW_STATE_TOGGLED) != 0;
}

static inline void wuss__window_set_toggled(wuss_window_t *window,
                                            int            toggled)
{
  if (toggled)
    window->state |= wuss_WINDOW_STATE_TOGGLED;
  else
    window->state &= (wuss_window_state_t) ~wuss_WINDOW_STATE_TOGGLED;
}

static inline int wuss__outline_px_for(wuss_window_flags_t flags)
{
  return (flags & wuss_WINDOW_NO_OUTLINE) ? 0 : 1;
}

static inline int wuss__outline_px(const wuss_window_t *window)
{
  return wuss__outline_px_for(window->flags);
}

/* ponytail: falls back to wuss's own titlebar height when the window has
 * none, so NO_TITLEBAR windows that still opt into scrollbars/resize match
 * their titled siblings instead of a hardcoded size; the hardcoded default
 * is only a last-resort floor if even that isn't positive */
static inline int wuss__button_size_for(const wuss_t       *wuss,
                                        wuss_window_flags_t flags)
{
  int size;

  size = wuss__titlebar_height_for(wuss, flags) - 2 * WUSS_BUTTON_INSET;
  if (size > 0)
    return size;

  size = wuss->titlebar_height - 2 * WUSS_BUTTON_INSET;

  return (size > 0) ? size : WUSS_DEFAULT_TITLEBAR_HEIGHT - 2 * WUSS_BUTTON_INSET;
}

static inline int wuss__button_size(const wuss_window_t *window)
{
  return wuss__button_size_for(window->wuss, window->flags);
}

/* how much of a content box's width/height is furniture (scrollbars, the
 * resize icon's corner), reserved outside the content area rather than
 * carved out of it -- shared by wuss__content_box (subtracts it back off
 * visible) and window creation/resize (add it to visible up front) so the
 * two stay consistent with each other */
static inline void wuss__furniture_carve_for(wuss_window_flags_t flags,
                                             int                 button_size,
                                             point_t            *carve)
{
  carve->x = (flags & wuss_WINDOW_NO_VSCROLL) ? 0 : button_size;
  carve->y = (flags & wuss_WINDOW_NO_HSCROLL) ? 0 : button_size;

  if (!(flags & wuss_WINDOW_NO_RESIZE) &&
      (flags & wuss_WINDOW_NO_VSCROLL) &&
      (flags & wuss_WINDOW_NO_HSCROLL))
  {
    carve->x = button_size;
    carve->y = button_size;
  }

  /* where furniture abuts the content area, a rule divides the two */
  if (carve->x > 0)
    carve->x += WUSS_DIVIDER_PX;
  if (carve->y > 0)
    carve->y += WUSS_DIVIDER_PX;
}
#else /* !WUSS_FURNITURE */
/* No furniture: every geometry helper collapses to "no chrome", so the core
 * window create/move/resize maths still compiles and yields visible ==
 * content. */
static inline int wuss__titlebar_height_for(const wuss_t       *wuss,
                                            wuss_window_flags_t flags)
{
  (void) wuss; (void) flags;
  return 0;
}

static inline int wuss__titlebar_height(const wuss_window_t *window)
{
  (void) window;
  return 0;
}

static inline int wuss__outline_px_for(wuss_window_flags_t flags)
{
  (void) flags;
  return 0;
}

static inline int wuss__outline_px(const wuss_window_t *window)
{
  (void) window;
  return 0;
}

static inline int wuss__button_size_for(const wuss_t       *wuss,
                                        wuss_window_flags_t flags)
{
  (void) wuss; (void) flags;
  return 0;
}

static inline int wuss__button_size(const wuss_window_t *window)
{
  (void) window;
  return 0;
}

static inline void wuss__furniture_carve_for(wuss_window_flags_t flags,
                                             int                 button_size,
                                             point_t            *carve)
{
  (void) flags; (void) button_size;
  carve->x = 0;
  carve->y = 0;
}
#endif /* WUSS_FURNITURE */

/* Largest content width/height whose visible box (content + outline +
 * titlebar + scrollbar/resize carve) still fits the screen from the
 * window's current top-left. Never returns below WUSS_MIN_CONTENT: a
 * window jammed hard against the far edge stays grabbable even though it
 * then overhangs. Shared by wuss_window_create and wuss_window_resize so
 * no path can produce a window larger than the screen. */
static inline void wuss__max_content_on_screen(const wuss_window_t *window,
                                               size2d_t            *max)
{
  int     outline_px, titlebar_height;
  point_t carve;

  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  max->w = window->wuss->scr->size.w - window->visible.x0
         - 2 * outline_px - carve.x;
  max->h = window->wuss->scr->size.h - window->visible.y0
         - 2 * outline_px - titlebar_height - carve.y;

  if (max->w < WUSS_MIN_CONTENT)
    max->w = WUSS_MIN_CONTENT;
  if (max->h < WUSS_MIN_CONTENT)
    max->h = WUSS_MIN_CONTENT;
}

/* Largest content width/height whose visible box fits the screen when the
 * window may be repositioned -- i.e. the whole screen less chrome, with no
 * allowance for the window's current top-left. Toggle-size uses this and
 * then nudges the top-left toward the origin by the minimum needed to fit.
 * Floored at WUSS_MIN_CONTENT like wuss__max_content_on_screen.
 * ponytail: a named helper (~6 lines) to stay parallel with its sibling
 * above rather than open-coded into toggle-action.c. */
static inline void wuss__max_content_anywhere_on_screen(const wuss_window_t *window,
                                                        size2d_t            *max)
{
  int     outline_px, titlebar_height;
  point_t carve;

  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  max->w = window->wuss->scr->size.w - 2 * outline_px - carve.x;
  max->h = window->wuss->scr->size.h
         - 2 * outline_px - titlebar_height - carve.y;

  if (max->w < WUSS_MIN_CONTENT)
    max->w = WUSS_MIN_CONTENT;
  if (max->h < WUSS_MIN_CONTENT)
    max->h = WUSS_MIN_CONTENT;
}

#endif /* IMPL_H */
