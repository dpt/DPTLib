/* wuss/tasks/oscillating.h -- Lissajous figure task */

#ifndef TASKS_OSCILLATING_H
#define TASKS_OSCILLATING_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/saveas.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#define OSCILLATING_POINTS 512 /* samples plotted along the curve */
#define OSCILLATING_NFREQS 12  /* frequency pairs offered */
#define OSCILLATING_NDECAYS 4   /* trail lengths offered; Off is the first */
#define OSCILLATING_MAX_TRAIL 16 /* most previous frames drawn dimmed */
#define OSCILLATING_NEASINGS 11 /* easing_NKINDS; None is the first */

/* window task: a Lissajous figure x=sin(a*t+phase), y=sin(b*t) plotted as a
 * ring of dots. The phase drifts each idle tick so the figure slowly morphs.
 * Select cycles the frequency pair (a,b); Adjust reverses the drift. The
 * menu's Ratio row picks the frequency pair directly; its Easing row picks
 * the curve for the random backwards runs (None by default); its Decay row
 * picks how many previous frames trail behind, dimmed (Off by default); its
 * Colours
 * row has Foreground and Background rows that pick fg and bg from the shared
 * colourmenu; Save PNG
 * opens a Save As dialogue (drag its icon onto a Filer window, or Save with
 * a full path already typed) that writes the window's content out. Once a
 * click has given the window the input focus, Space pauses/resumes and
 * Left/Right step the frequency pair back/forward. */
typedef struct oscillating_task
{
  wuss_t             *wuss;   /* borrowed; for wuss_get_pointer on MENU click */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_saveas_t       *saveas;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[7]; /* per-instance: a shared static would
                                       * leak one instance's .window pointer
                                       * into another's menu */
  wuss_menu_t         menu;
  int                 paused; /* Menu > Pause; idle does nothing while set */
  int                 reversals; /* Menu > Reversals; random backwards runs */
  wuss_menu_item_t    ratio_items[OSCILLATING_NFREQS]; /* one row per pair */
  wuss_menu_t         ratio_menu;
  wuss_menu_item_t    easing_items[OSCILLATING_NEASINGS]; /* one per curve */
  wuss_menu_t         easing_menu;
  wuss_colourpair_t   colours; /* Foreground, Background */
  wuss_menu_item_t    decay_items[OSCILLATING_NDECAYS]; /* one per length */
  wuss_menu_t         decay_menu;
  int                 decay_index; /* index into oscillating_decays */
  double              trail[OSCILLATING_MAX_TRAIL]; /* ring of past phases */
  int                 trail_head; /* newest entry in trail */
  int                 trail_count; /* valid entries in trail */
  int                 easing_index; /* easing_kind_t; easing_LINEAR = None */
  wuss_window_t *window;
  colour_t       bg, fg;
  int            a, b;    /* frequency ratio */
  double         phase;
  double         drift;
  double         reversing; /* fraction of the backwards run still to do */
  double         reverse_from; /* phase at the start of the backwards run */
  int            freq_index; /* index into oscillating_freqs */
}
oscillating_task_t;

wuss_window_fn_t oscillating_handle;

/* create the Lissajous window against the given wuss instance; "task" is a
 * per-instance block owned by the window and freed when it closes. if out is
 * non-NULL, the task block is also returned through it */
result_t oscillating_create(wuss_t *wuss, oscillating_task_t **out);

/* install the icon bar app that opens lissajous windows on demand */
result_t oscillating_app_create(wuss_t *wuss);

/* free a task block allocated by oscillating_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void oscillating_destroy(oscillating_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_OSCILLATING_H */
