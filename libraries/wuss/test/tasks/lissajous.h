/* wuss/test/tasks/lissajous.h -- Lissajous figure task */

#ifndef TASKS_LISSAJOUS_H
#define TASKS_LISSAJOUS_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#define LISSAJOUS_POINTS 512 /* samples plotted along the curve */
#define LISSAJOUS_NFREQS 12  /* frequency pairs offered */

/* window task: a Lissajous figure x=sin(a*t+phase), y=sin(b*t) plotted as a
 * ring of dots. The phase drifts each idle tick so the figure slowly morphs.
 * Select cycles the frequency pair (a,b); Adjust reverses the drift. The
 * menu's Background row picks bg from the shared colourmenu; its Ratio row
 * picks the frequency pair directly. */
typedef struct lissajous_task
{
  wuss_t             *wuss;   /* borrowed; for wuss_get_pointer on MENU click */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[4]; /* per-instance: a shared static would
                                       * leak one instance's .window pointer
                                       * into another's menu */
  wuss_menu_t         menu;
  int                 paused; /* Menu > Pause; idle does nothing while set */
  wuss_menu_item_t    ratio_items[LISSAJOUS_NFREQS]; /* one row per pair */
  wuss_menu_t         ratio_menu;
  wuss_window_t *window;
  colour_t       bg, fg;
  int            a, b;    /* frequency ratio */
  double         phase;
  double         drift;
  int            freq_index; /* index into lissajous_freqs */
}
lissajous_task_t;

wuss_window_fn_t lissajous_handle;

/* create the Lissajous window against the given wuss instance; "task" is a
 * per-instance block owned by the window and freed when it closes. if out is
 * non-NULL, the task block is also returned through it */
result_t lissajous_create(wuss_t *wuss, lissajous_task_t **out);

/* free a task block allocated by lissajous_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void lissajous_destroy(lissajous_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_LISSAJOUS_H */
