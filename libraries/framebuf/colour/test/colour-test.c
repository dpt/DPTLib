/* framebuf/colour/test/colour-test.c -- tests for colour construction and conversion */

#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "utils/rng.h"

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

#define FAIL(what) \
  do { printf("Failure: %s\n", what); return result_TEST_FAILED; } while (0)

/* Rec.601 luma, 0..255, as colour_random_contrast measures it */
static int luma(colour_t c)
{
  unsigned int r, g, b;

  colour_get_rgb(&c, &r, &g, &b);

  return (int) ((77 * r + 150 * g + 29 * b) >> 8);
}

result_t colour_test(const char *resources)
{
  static const colour_t palette[3] =
  {
    { 0xFF000000u }, { 0xFF0000FFu }, { 0xFFFFFFFFu }
  };

  colour_t     c;
  unsigned int r, g, b;
  colour_t     mid;
  rng_t        rng1;
  rng_t        rng2;
  int          i;
  int          against;
  colour_t     backs[3];

  NOT_USED(resources);

  printf("test: construct and take apart\n");

  c = colour_rgba(0x12, 0x34, 0x56, 0x78);
  colour_get_rgb(&c, &r, &g, &b);
  if (r != 0x12 || g != 0x34 || b != 0x56 || colour_get_alpha(&c) != 0x78)
    FAIL("rgba round trip");
  c = colour_rgb(0x12, 0x34, 0x56);
  if (colour_get_alpha(&c) != PIXELFMT_OPAQUE)
    FAIL("rgb is opaque");

  printf("test: mix\n");

  c = colour_mix(colour_rgb(0, 0, 0), colour_rgb(255, 100, 10), 0, 4);
  if (c.primary != colour_rgb(0, 0, 0).primary)
    FAIL("mix none of b");
  c = colour_mix(colour_rgb(0, 0, 0), colour_rgb(255, 100, 10), 4, 4);
  if (c.primary != colour_rgb(255, 100, 10).primary)
    FAIL("mix all of b");
  mid = colour_mix(colour_rgb(0, 200, 40), colour_rgb(255, 100, 10), 1, 2);
  if (mid.primary != colour_rgb(127, 150, 25).primary)
    FAIL("mix halfway");

  printf("test: colour to pixel\n");

  c = colour_rgba(0x12, 0x34, 0x56, 0x78);
  if (colour_to_pixel(NULL, 0, c, pixelfmt_rgbx8888) != 0xFF563412u ||
      colour_to_pixel(NULL, 0, c, pixelfmt_rgba8888) != 0x78563412u ||
      colour_to_pixel(NULL, 0, c, pixelfmt_bgrx8888) != 0xFF123456u ||
      colour_to_pixel(NULL, 0, c, pixelfmt_bgra8888) != 0x78123456u ||
      colour_to_pixel(NULL, 0, c, pixelfmt_rgb565)   != 0x51A2u ||
      colour_to_pixel(NULL, 0, c, pixelfmt_rgbx5551) != 0x28C2u)
    FAIL("direct formats");

  /* every paletted depth picks the nearest entry */
  if (colour_to_pixel(palette, 3, colour_rgb(10, 0, 0), pixelfmt_p8) != 0 ||
      colour_to_pixel(palette, 3, colour_rgb(200, 0, 0), pixelfmt_p4) != 1 ||
      colour_to_pixel(palette, 3, colour_rgb(250, 240, 250), pixelfmt_p2) != 2 ||
      colour_to_pixel(palette, 3, colour_rgb(0, 0, 0), pixelfmt_p1) != 0)
    FAIL("paletted formats");

  printf("test: random colours repeat by seed and stand out\n");

  rng_seed(&rng1, 1234);
  rng_seed(&rng2, 1234);
  for (i = 0; i < 8; i++)
  {
    c = colour_random(&rng1);
    if (c.primary != colour_random(&rng2).primary ||
        colour_get_alpha(&c) != PIXELFMT_OPAQUE)
      FAIL("random");
  }

  /* mid grey is hard to contrast with, so some seeds take the fallback */
  backs[0] = colour_rgb(0, 0, 0);
  backs[1] = colour_rgb(255, 255, 255);
  backs[2] = colour_rgb(128, 128, 128);
  for (against = 0; against < (int) NELEMS(backs); against++)
    for (i = 1; i <= 64; i++)
    {
      rng_seed(&rng1, (uint32_t) i);
      c = colour_random_contrast(&rng1, backs[against]);
      if (abs(luma(c) - luma(backs[against])) < 96)
        FAIL("random_contrast too close");
    }

  return result_TEST_PASSED;
}
