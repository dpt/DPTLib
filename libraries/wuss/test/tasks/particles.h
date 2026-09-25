/* wuss/test/tasks/particles.h -- particle explosion task */

#ifndef TASKS_PARTICLES_H
#define TASKS_PARTICLES_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "utils/rng.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "explosion.h" /* fetched from github.com/dpt/Explosion */

#define PARTICLES_NSTYLES 4

/* window task: the Explosion particle engine. Select bursts a random mix of
 * styles at the click; Adjust bursts flecks. A fresh burst fires by itself
 * whenever the window falls quiet. */
typedef struct particles_task
{
  wuss_t             *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[1]; /* per-instance: shared static "Info" row
                                       * would leak one instance's .window
                                       * pointer into another's menu */
  wuss_menu_t         menu;
  rng_t               rng;
  unsigned int        now_ms;   /* simulated clock, advanced per idle tick */
  particle_style_t    styles[PARTICLES_NSTYLES];
  colour_t            palette[PARTICLES_NSTYLES * PALETTE_SIZE];
  particle_system_t   ps;

  /* valid only during redraw, for the engine's render callback */
  screen_t           *scr;
  int                 ox, oy;
}
particles_task_t;

wuss_window_fn_t particles_handle;

/* create the particles window against the given wuss instance; the task
 * block is allocated here, owned by the window, and freed when it closes.
 * if out is non-NULL, the task block is also returned through it */
result_t particles_create(wuss_t *wuss, particles_task_t **out);

/* free a task block allocated by particles_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void particles_destroy(particles_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_PARTICLES_H */
