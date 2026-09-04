/* wuss/test/tasks/icons.h -- work-area icons task */

#ifndef TASKS_ICONS_H
#define TASKS_ICONS_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/icon.h"
#include "wuss/window.h"

/* demonstrates wuss-managed work-area icons: labelled groups covering every
 * icon type (buttons, radios/options, bitmaps, a pattern swatch, a menu-entry
 * strip) plus a button placed far down the document to show icons scroll with
 * the content and stay clickable */
typedef struct icons_task
{
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
  wuss_icon_t   *hotspot; /* the interactive BITMAP icon */
}
icons_task_t;

wuss_window_fn_t icons_handle;

/* create the icons window against the given wuss instance */
result_t icons_create(wuss_t       *wuss,
                      bmfont_t     *font,
                      const char   *resources,
                      icons_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_ICONS_H */
