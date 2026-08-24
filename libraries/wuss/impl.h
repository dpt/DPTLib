/* impl.h -- wuss - minimal window manager */

#ifndef IMPL_H
#define IMPL_H

#include "datastruct/list.h"
#include "geom/box.h"
#include "framebuf/screen.h"
#include "framebuf/bmfont.h"

#include "wuss/wuss.h"
#include "wuss/window.h"

#define WUSS_TITLE_MAX               63
#define WUSS_DEFAULT_TITLEBAR_HEIGHT 20

struct wuss
{
  screen_t      *scr;
  bmfont_t      *font;      /* nullable, not owned */
  colour_t      *palette;   /* owned */
  int            npalette;
  wuss_colour_t  titlebar_bg;
  wuss_colour_t  titlebar_fg;
  int            titlebar_height;
  list_t         z_order;   /* anchor; head = topmost window */
  wuss_window_t *dragging;  /* NULL when idle */
  int            drag_dx, drag_dy;
};

struct wuss_window
{
  list_t        link;   /* must be first member */
  wuss_t       *wuss;
  box_t         visible;
  wuss_client_t client;
  char          title[WUSS_TITLE_MAX + 1];
};

wuss_window_t *wuss__window_at(wuss_t *wuss, int x, int y);
void            wuss__titlebar_box(const wuss_window_t *window, box_t *out);
void            wuss__content_box(const wuss_window_t *window, box_t *out);

static inline int wuss__size_ok(int width, int height, int titlebar_height)
{
  return width > 0 && height > titlebar_height;
}

#endif /* IMPL_H */
