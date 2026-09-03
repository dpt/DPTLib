/* wuss/icon.h -- work-area icons, internal */

#ifndef WUSS_ICON_IMPL_H
#define WUSS_ICON_IMPL_H

#include "geom/box.h"
#include "geom/point.h"

#include "framebuf/screen.h"

#include "wuss/wuss.h"
#include "wuss/icon.h"

/* Per-instance transient state, kept as bitflags with wuss__icon_* accessors
 * (mirrors wuss_window_state_t) so more can be added without growing the
 * struct. Not part of the public appearance API. */
typedef enum wuss_icon_state
{
  wuss_ICON_STATE_NONE     = 0,
  wuss_ICON_STATE_PRESSED  = 1 << 0, /* button: held with the pointer inside */
  wuss_ICON_STATE_SELECTED = 1 << 1, /* radio/option: latched on; menu entry:
                                      * draw a tick */
  wuss_ICON_STATE_HOVERED  = 1 << 2  /* menu entry: pointer is over it */
}
wuss_icon_state_t;

struct wuss_icon
{
  wuss_window_t    *window;  /* owner; back-pointer for invalidate/get_window */
  box_t             bbox;    /* virtual document space */
  wuss_icon_type_t  type;
  char             *text;    /* owned; never NULL ("" instead) */
  wuss_colour_t     fg, bg;
  screen_pattern_t  pattern; /* wuss_ICON_TYPE_PATTERN tile; 0 otherwise */
  const bitmap_t   *bitmap;  /* wuss_ICON_TYPE_BITMAP image; borrowed, or NULL */
  int               group;   /* radio: exclusive-selection group; 0 = none */
  wuss_colour_t     swatch;  /* menu entry + FLAGS_SWATCH: left-gutter chip
                              * colour; wuss_NO_BACKGROUND otherwise */
  wuss_icon_flags_t flags;
  wuss_icon_state_t state;
};

static inline int wuss__icon_pressed(const wuss_icon_t *icon)
{
  return (icon->state & wuss_ICON_STATE_PRESSED) != 0;
}

static inline int wuss__icon_selected(const wuss_icon_t *icon)
{
  return (icon->state & wuss_ICON_STATE_SELECTED) != 0;
}

static inline int wuss__icon_hovered(const wuss_icon_t *icon)
{
  return (icon->state & wuss_ICON_STATE_HOVERED) != 0;
}

static inline void wuss__icon_set_state(wuss_icon_t      *icon,
                                        wuss_icon_state_t bit,
                                        int               on)
{
  if (on)
    icon->state |= bit;
  else
    icon->state &= (wuss_icon_state_t) ~bit;
}

/* Set icon->selected, invalidating it. For a radio with a non-zero group,
 * selecting it also clears every other selected radio on the same window with
 * that group. Ignored for types with no latched state. */
void wuss__icon_select(wuss_icon_t *icon, int selected);

/* Make "icon" (may be NULL) the hovered icon: clears the hovered flag on the
 * previous wuss->hover_icon and sets it on the new one, invalidating whichever
 * of the two changed. A no-op if nothing changed. */
void wuss__icon_set_hover(wuss_t *wuss, wuss_icon_t *icon);

/* Map an icon bbox (virtual document space) into screen space:
 * screen = content.x0 - scroll.x + bbox. wuss__icon_draw paints through this
 * so hit-testing and invalidation cannot drift from what is drawn. */
void wuss__icon_box_to_screen(const box_t *content,
                              point_t      scroll,
                              const box_t *bbox,
                              box_t       *out);

/* Invalidate exactly this icon's bbox, via wuss_window_invalidate, so a
 * set_text / pressed-state / hide change repaints just the icon. */
void wuss__icon_invalidate(const wuss_icon_t *icon);

/* Draw one icon. Called from redraw_window with wuss->scr->clip already set to
 * the surviving content piece and the background already filled. "content" is
 * the window's full (unclipped) content box, screen space; "scroll" is
 * window->scroll. */
void wuss__icon_draw(wuss_t            *wuss,
                     const wuss_icon_t *icon,
                     const box_t       *content,
                     point_t            scroll);

/* Hit-test every visible, enabled button icon of "window" against a point given
 * in virtual document space. Returns the topmost (last-created wins) match, or
 * NULL. Label, hidden and disabled icons are skipped. */
wuss_icon_t *wuss__icon_hit_test(wuss_window_t *window, point_t doc_point);

/* Free a window's whole icon store (text + nodes + array). Teardown only: does
 * not invalidate or swap-remove. */
void wuss__icons_free(wuss_window_t *window);

#endif /* WUSS_ICON_IMPL_H */
