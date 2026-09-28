/* wuss/test/tasks/particles.h -- particle explosion task */

#ifndef TASKS_PARTICLES_H
#define TASKS_PARTICLES_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "utils/rng.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "explosion.h" /* fetched from github.com/dpt/Explosion */

#define PARTICLES_NSTYLES 4

/* window task: the Explosion particle engine. Select bursts a random mix of
 * styles at the click; Adjust bursts flecks. The pointer trails particles
 * while over the content. Menu > Add > Emitter adds a smoke emitter, at a
 * chosen intensity, where the menu was opened; Menu > Add > Repeller and
 * Menu > Add > Attractor add a repeller/attractor, at a chosen strength,
 * there too; Menu > Background picks the fill; Menu > Gravity scales every
 * style's pull; Menu > Walls bounces
 * particles off the edges; Menu > Clear removes every particle and emitter.
 * Space toggles pause, W toggles walls and C clears, once a click has given
 * the window the input focus. A fresh burst fires by itself whenever the
 * window falls quiet. */
typedef struct particles_task
{
  wuss_t             *wuss;     /* for wuss_get_pointer when opening the menu */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[7]; /* per-instance: shared static "Info" row
                                       * would leak one instance's .window
                                       * pointer into another's menu */
  wuss_menu_t         menu;
  int                 paused; /* Menu > Pause; idle does nothing while set */
  wuss_menu_item_t    add_items[3]; /* Add > Emitter / Repeller / Attractor */
  wuss_menu_t         add_menu;
  wuss_menu_item_t    emitter_items[3]; /* one row per intensity */
  wuss_menu_t         emitter_menu;
  wuss_menu_item_t    repeller_items[3]; /* one row per strength */
  wuss_menu_t         repeller_menu;
  wuss_menu_item_t    attractor_items[3]; /* one row per strength */
  wuss_menu_t         attractor_menu;
  wuss_menu_item_t    gravity_items[5]; /* one row per strength */
  wuss_menu_t         gravity_menu;
  int                 gravity; /* index into particles_gravities */
  int                 menu_x, menu_y; /* where the menu was opened: an
                                       * emitter picked from it goes here */
  rng_t               rng;
  unsigned int        now_ms;   /* simulated clock, advanced per idle tick */
  particle_style_t    styles[PARTICLES_NSTYLES];
  colour_t            bg;
  colour_t            palette[PARTICLES_NSTYLES * PALETTE_SIZE];
  particle_system_t   ps;

  /* pointer trail: last seen position and velocity (pixels/second) */
  int                 pointer_in; /* moved over content since last exit */
  int                 mx, my;
  float               mvx, mvy;
  unsigned int        last_move_ms;
  unsigned int        last_emit_ms;

  /* valid only during redraw, for the engine's render callback */
  screen_t           *scr;
  int                 ox, oy;
  pixelfmt_any_t      pixels[PARTICLES_NSTYLES * PALETTE_SIZE]; /* palette[] in scr's format */
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
