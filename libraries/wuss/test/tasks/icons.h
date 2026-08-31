/* icons.h -- wuss test - work-area icons task */

#ifndef TASKS_ICONS_H
#define TASKS_ICONS_H

#ifdef USE_SDL

#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/icon.h"
#include "wuss/window.h"

/* demonstrates wuss-managed work-area icons: a couple of labels, a button that
 * bumps a counter, and a second button placed far down the document to show
 * icons scroll with the content and stay clickable */
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

wuss_event_fn_t icons_handle;

/* create the icons window against the given wuss instance */
result_t icons_create(wuss_t         *wuss,
                      const colour_t *palette,
                      bmfont_t       *font,
                      const char     *resources,
                      icons_task_t   *task);


#endif /* USE_SDL */

#endif /* TASKS_ICONS_H */
