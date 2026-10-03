/* wuss/test/tasks/fireball.c -- reusable fire-coloured particle burst */

#ifdef WUSS_APP

#include <stdlib.h>

#include "base/utils.h"
#include "geom/size.h"

#include "fireball.h"

/* fire colours, hot to cool, indexed by particle age */
static const unsigned char g_fireball_fire[PALETTE_SIZE][3] =
{
  { 255, 255, 255 }, { 255, 232,   8 }, { 255, 206,   0 }, { 255, 154,   0 },
  { 255,  90,   0 }, { 200,  40,   0 }, { 100,  20,  20 }, {  30,  30,  30 }
};

/* engine callbacks */

static unsigned int fireball_rand(int nbits, void *opaque)
{
  fireball_t *fb;
  uint32_t    r;

  fb = opaque;

  r = rng_xorshift32(&fb->rng);
  return (nbits >= 32) ? r : r & ((1u << nbits) - 1);
}

static unsigned int fireball_time(void *opaque)
{
  fireball_t *fb;

  fb = opaque;

  return fb->now_ms;
}

static void fireball_render_particle(int   x,
                                     int   y,
                                     int   size,
                                     int   palette_index,
                                     void *opaque)
{
  fireball_t *fb;

  fb = opaque;

  screen_fill_rect_value(fb->scr, fb->ox + x - size / 2, fb->oy + y - size / 2,
                         SIZE2D(size, size), fb->pixels[palette_index]);
}

/* ----------------------------------------------------------------------- */

void fireball_init(fireball_t *fb)
{
  int i;

  rng_seed(&fb->rng, (uint32_t) rand());

  for (i = 0; i < PALETTE_SIZE; i++)
    fb->palette[i] = colour_rgb(g_fireball_fire[i][0],
                                g_fireball_fire[i][1],
                                g_fireball_fire[i][2]);

  set_default_style(&fb->style, 1000.0f / PHYSICS_FPS);
  fb->style.min_size     *= 4;
  fb->style.max_size     *= 4;
  fb->style.palette_index = 0;
  init_particle_system(&fb->ps, 0, &fb->style, 1, 0.2f, fireball_rand,
                       fireball_time, fireball_render_particle, fb);
}

void fireball_reset(fireball_t *fb)
{
  reset_particle_system(&fb->ps);
}

void fireball_burst(fireball_t *fb, int x, int y, int w, int h, int count)
{
  fb->ps.width  = w;
  fb->ps.height = h;
  create_explosion(&fb->ps, 0, x, y, 0.0f, 0.0f, count);
}

void fireball_step(fireball_t *fb)
{
  fb->now_ms += 1000 / PHYSICS_FPS;
  update_particles(&fb->ps, 1.0f / PHYSICS_FPS);
}

int fireball_active(const fireball_t *fb)
{
  return is_active(&fb->ps);
}

void fireball_render(fireball_t *fb, screen_t *scr, int ox, int oy)
{
  int i;

  fb->scr = scr;
  fb->ox  = ox;
  fb->oy  = oy;
  for (i = 0; i < PALETTE_SIZE; i++)
    fb->pixels[i] = screen_colour_to_pixel(scr, fb->palette[i]);
  render_particles(&fb->ps);
  fb->scr = NULL;
}

#endif /* WUSS_APP */
