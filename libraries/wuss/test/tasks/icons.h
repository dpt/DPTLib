/* wuss/test/tasks/icons.h -- work-area icons task */

#ifndef TASKS_ICONS_H
#define TASKS_ICONS_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/proginfo.h"
#include "wuss/gadget/stringset.h"
#include "wuss/icon.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* demonstrates wuss-managed work-area icons: labelled groups covering every
 * icon type (buttons, radios/options, bitmaps, a pattern swatch, display
 * fields, sliders, a menu-entry strip, a string set gadget) plus a button
 * placed far down the document to show icons scroll with the content and
 * stay clickable */
typedef struct icons_task
{
  wuss_t             *wuss;   /* borrowed; for wuss_get_pointer on MENU click */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[1]; /* per-instance: a shared static would
                                       * leak one instance's .window pointer
                                       * into another's menu */
  wuss_menu_t         menu;
  wuss_window_t *window;
  bmfont_t      *font;
  colour_t       label;  /* axis coordinate text */
  colour_t       paper;  /* window bg, for bmfont_draw glyph blending */
  wuss_icon_t   *button;  /* "Press me" */
  wuss_icon_t   *counter; /* label showing hit count */
  int            count;
  wuss_icon_t   *opt;     /* standalone option button */
  wuss_icon_t   *state;   /* label echoing radio/option selection */
  bitmap_t       sprite;  /* borrowed by the two BITMAP icons; freed on close */
  int            has_sprite;
  wuss_icon_t   *hotspot;      /* the interactive BITMAP icon */
  wuss_icon_t   *slider_horiz; /* horizontal slider */
  wuss_icon_t   *slider_vert;  /* vertical slider */
  wuss_icon_t   *slider_state; /* label echoing whichever slider last moved */
  wuss_icon_t   *tally;        /* display field mirroring the counter */
  wuss_icon_t   *echo;         /* label echoing whichever writable last changed */
  wuss_stringset_t *sset;      /* gadget; its icons belong to the window */
  wuss_icon_t   *sset_echo;    /* label echoing the string set's pick */
}
icons_task_t;

wuss_window_fn_t icons_handle;

/* create the icons window against the given wuss instance, lettering the
 * axis rulers with wuss's own regular font (wuss_get_font_n(wuss, 0)); the
 * sprite and icon set are loaded from
 * wuss_get_resources(wuss)/resources/wuss/. if out is non-NULL, the task
 * block is also returned through it */
result_t icons_create(wuss_t *wuss, icons_task_t **out);

/* free a task block allocated by icons_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void icons_destroy(icons_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_ICONS_H */
