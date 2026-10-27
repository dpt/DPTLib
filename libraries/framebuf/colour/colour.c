/* framebuf/colour/colour.c */

#include <assert.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>

#include "framebuf/pixelfmt.h"
#include "framebuf/colour.h"

colour_t colour_rgb(unsigned int r, unsigned int g, unsigned int b)
{
  return colour_rgba(r, g, b, PIXELFMT_OPAQUE);
}

colour_t colour_random(rng_t *rng)
{
  unsigned int r, g, b;

  r = (unsigned int) rng_range(rng, 256);
  g = (unsigned int) rng_range(rng, 256);
  b = (unsigned int) rng_range(rng, 256);

  return colour_rgb(r, g, b);
}

/* Rec.601 luma, 0..255 */
static int colour_luma(colour_t c)
{
  unsigned int r, g, b;

  colour_get_rgb(&c, &r, &g, &b);

  return (int) ((77 * r + 150 * g + 29 * b) >> 8);
}

colour_t colour_random_contrast(rng_t *rng, colour_t against)
{
  colour_t c;
  int      ref;
  int      i;

  ref = colour_luma(against);

  for (i = 0; i < 16; i++)
  {
    c = colour_random(rng);
    if (abs(colour_luma(c) - ref) >= 96)
      return c;
  }

  return ref < 128 ? colour_rgb(255, 255, 255) : colour_rgb(0, 0, 0);
}

colour_t colour_rgba(unsigned int r,
                     unsigned int g,
                     unsigned int b,
                     unsigned int a)
{
  colour_t c;

  c.primary = PIXELFMT_MAKE_RGBA8888(r, g, b, a);
  return c;
}

colour_t colour_mix(colour_t     a,
                    colour_t     b,
                    unsigned int num,
                    unsigned int den)
{
  unsigned int ar, ag, ab;
  unsigned int br, bg, bb;

  colour_get_rgb(&a, &ar, &ag, &ab);
  colour_get_rgb(&b, &br, &bg, &bb);

  return colour_rgb((ar * (den - num) + br * num) / den,
                    (ag * (den - num) + bg * num) / den,
                    (ab * (den - num) + bb * num) / den);
}

colour_t colour_mix_rgba(colour_t     a,
                         colour_t     b,
                         unsigned int num,
                         unsigned int den)
{
  unsigned int ar, ag, ab, aa;
  unsigned int br, bg, bb, ba;
  unsigned int half;

  colour_get_rgb(&a, &ar, &ag, &ab);
  aa = colour_get_alpha(&a);
  colour_get_rgb(&b, &br, &bg, &bb);
  ba = colour_get_alpha(&b);
  half = den / 2;

  return colour_rgba((ar * (den - num) + br * num + half) / den,
                     (ag * (den - num) + bg * num + half) / den,
                     (ab * (den - num) + bb * num + half) / den,
                     (aa * (den - num) + ba * num + half) / den);
}

static unsigned int closest_palette_entry(const colour_t *palette,
                                          int             nentries,
                                          colour_t        required)
{
#if 1
  /* libjpeg's weights */
  const int red_weight   = 19595; /* 0.29900 * 65536 (rounded down) */
  const int green_weight = 38470; /* 0.58700 * 65536 (rounded up)   */
  const int blue_weight  =  7471; /* 0.11400 * 65536 (rounded down) */
#else
  /* libpng's weights */
  const int red_weight   = 13938; /* 0.212671 * 65536 (rounded up)   */
  const int green_weight = 46869; /* 0.715160 * 65536 (rounded up)   */
  const int blue_weight  =  4729; /* 0.072169 * 65536 (rounded down) */
#endif

  pixelfmt_rgba8888_t req;
  int                 closest;
  unsigned int        dist;
  int                 req_r, req_g, req_b;
  int                 i;
  pixelfmt_rgba8888_t ent;
  int                 ent_r, ent_g, ent_b;
  int                 dr, dg, db;
  unsigned int        curdist;

  req = required.primary;
  req_r = PIXELFMT_Rxxx8888(req);
  req_g = PIXELFMT_xGxx8888(req);
  req_b = PIXELFMT_xxBx8888(req);

  closest = -1;
  dist    = INT_MAX;

  for (i = 0; i < nentries; i++)
  {
    ent = palette[i].primary;
    ent_r = PIXELFMT_Rxxx8888(ent);
    ent_g = PIXELFMT_xGxx8888(ent);
    ent_b = PIXELFMT_xxBx8888(ent);
    dr = ent_r - req_r;
    dg = ent_g - req_g;
    db = ent_b - req_b;
    curdist = ((unsigned int) (dr * dr) * red_weight   +
               (unsigned int) (dg * dg) * green_weight +
               (unsigned int) (db * db) * blue_weight) >> 16;
    if (curdist < dist)
    {
      dist    = curdist;
      closest = i;
    }
  }

  return closest;
}

pixelfmt_any_t colour_to_pixel(const colour_t *palette,
                               int             nentries,
                               colour_t        required,
                               pixelfmt_t      fmt)
{
  pixelfmt_any_t r,g,b,a;

  switch (fmt)
  {
  case pixelfmt_p1:
  case pixelfmt_p2:
  case pixelfmt_p4:
  case pixelfmt_p8:
    return closest_palette_entry(palette, nentries, required);

  case pixelfmt_rgbx8888:
    return (PIXELFMT_OPAQUE << PIXELFMT_xxxA8888_SHIFT) | required.primary; /* ensure opaque */

  case pixelfmt_rgba8888:
    return required.primary;

  case pixelfmt_bgrx8888:
  case pixelfmt_bgra8888:
    r = PIXELFMT_Rxxx8888(required.primary);
    g = PIXELFMT_xGxx8888(required.primary);
    b = PIXELFMT_xxBx8888(required.primary);
    a = (fmt == pixelfmt_bgra8888) ? PIXELFMT_xxxA8888(required.primary) : PIXELFMT_OPAQUE;
    return PIXELFMT_MAKE_BGRA8888(r, g, b, a);

  case pixelfmt_rgb565:
    r = PIXELFMT_Rxxx8888(required.primary) >> 3;
    g = PIXELFMT_xGxx8888(required.primary) >> 2;
    b = PIXELFMT_xxBx8888(required.primary) >> 3;
    return PIXELFMT_MAKE_RGB565(r, g, b);

  case pixelfmt_bgr565: /* rgb565 with red and blue swapped */
    r = PIXELFMT_Rxxx8888(required.primary) >> 3;
    g = PIXELFMT_xGxx8888(required.primary) >> 2;
    b = PIXELFMT_xxBx8888(required.primary) >> 3;
    return PIXELFMT_MAKE_RGB565(b, g, r);

  case pixelfmt_rgbx5551:
    r = PIXELFMT_Rxxx8888(required.primary) >> 3;
    g = PIXELFMT_xGxx8888(required.primary) >> 3;
    b = PIXELFMT_xxBx8888(required.primary) >> 3;
    return PIXELFMT_MAKE_RGBX5551(r, g, b);

  case pixelfmt_bgrx5551: /* rgbx5551 with red and blue swapped */
    r = PIXELFMT_Rxxx8888(required.primary) >> 3;
    g = PIXELFMT_xGxx8888(required.primary) >> 3;
    b = PIXELFMT_xxBx8888(required.primary) >> 3;
    return PIXELFMT_MAKE_RGBX5551(b, g, r);

  default:
    assert(!"Unimplemented pixel format");
    return 0xFFFFFFFF;
  }
}

unsigned int colour_get_alpha(const colour_t *c)
{
  return PIXELFMT_xxxA8888(c->primary);
}

void colour_get_rgb(const colour_t *c,
                    unsigned int   *r,
                    unsigned int   *g,
                    unsigned int   *b)
{
  *r = PIXELFMT_Rxxx8888(c->primary);
  *g = PIXELFMT_xGxx8888(c->primary);
  *b = PIXELFMT_xxBx8888(c->primary);
}
