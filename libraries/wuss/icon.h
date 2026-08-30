/* icon.h -- wuss - work-area icons, internal */

#ifndef WUSS_ICON_IMPL_H
#define WUSS_ICON_IMPL_H

#include "geom/box.h"
#include "geom/point.h"

#include "framebuf/screen.h"

#include "wuss/wuss.h"
#include "wuss/icon.h"

struct wuss_icon
{
  wuss_window_t    *window;  /* owner; back-pointer for invalidate/get_window */
  box_t             bbox;    /* virtual document space */
  wuss_icon_type_t  type;
  char             *text;    /* owned; never NULL ("" instead) */
  wuss_colour_t     fg;
  wuss_colour_t     bg;
  screen_pattern_t  pattern; /* wuss_ICON_TYPE_PATTERN tile; 0 otherwise */
  wuss_icon_flags_t flags;
  int               pressed; /* button: 1 while held with the pointer inside */
};

/* Convert an icon's bbox (virtual document space) to a screen-space box, using
 * the owning window's current content box and scroll offset:
 * screen = content.x0 - scroll.x + bbox. Mirrors wuss_window_invalidate. */
void wuss__icon_screen_box(const wuss_icon_t *icon, box_t *out);

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
