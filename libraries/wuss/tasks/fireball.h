/* wuss/tasks/fireball.h -- reusable fire-coloured particle burst */

#ifndef TASKS_FIREBALL_H
#define TASKS_FIREBALL_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "utils/rng.h"

#include "explosion.h" /* fetched from github.com/dpt/Explosion */

/* one Explosion particle system with a fire palette, for a task to embed.
 * The engine holds a pointer back into this block, so it must not move once
 * fireball_init has run. Typical use: fireball_init at create,
 * fireball_burst on the event, fireball_step from each idle tick while
 * fireball_active, fireball_render from the redraw handler. */
typedef struct fireball
{
  rng_t              rng;
  unsigned int       now_ms; /* simulated clock, advanced per step */
  particle_style_t   style;
  colour_t           palette[PALETTE_SIZE];
  particle_system_t  ps;

  /* valid only during fireball_render, for the engine's render callback */
  screen_t          *scr;
  int                ox, oy;
  pixelfmt_any_t     pixels[PALETTE_SIZE]; /* palette[] in scr's format */
}
fireball_t;

/* prepare fb for use; seeds the generator from rand() */
void fireball_init(fireball_t *fb);

/* drops every live particle */
void fireball_reset(fireball_t *fb);

/* bursts count particles at (x,y), kept within a w x h area */
void fireball_burst(fireball_t *fb, int x, int y, int w, int h, int count);

/* advances the simulation by one 1/PHYSICS_FPS tick */
void fireball_step(fireball_t *fb);

/* non-zero while any particle is live */
int fireball_active(const fireball_t *fb);

/* draws the live particles onto scr, offset by (ox,oy) */
void fireball_render(fireball_t *fb, screen_t *scr, int ox, int oy);

#endif /* WUSS_APP */

#endif /* TASKS_FIREBALL_H */
