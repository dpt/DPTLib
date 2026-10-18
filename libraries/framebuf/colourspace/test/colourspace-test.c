/* framebuf/colourspace/test/colourspace-test.c */

#include <math.h>
#include <stdio.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/colourspace.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* Round-trip every model over a coarse RGB grid. Each must return the
 * starting colour exactly once quantised back to 8 bits. */
static int colourspace_test_round_trips(void)
{
  int                 r, g, b;
  colour_t            in;
  colourspace_rgb_t   rgb;
  colourspace_rgb_t   back;
  colourspace_cmyk_t  cmyk;
  colourspace_hsl_t   hsl;
  colourspace_hsv_t   hsv;
  colourspace_oklab_t oklab;
  int                 model;
  colour_t            out;

  for (r = 0; r < 256; r += 15)
    for (g = 0; g < 256; g += 15)
      for (b = 0; b < 256; b += 15)
      {
        in = colour_rgb(r, g, b);
        colourspace_from_colour(in, &rgb);

        for (model = 0; model < 4; model++)
        {
          switch (model)
          {
          case 0:
            colourspace_rgb_to_cmyk(&rgb, &cmyk);
            colourspace_cmyk_to_rgb(&cmyk, &back);
            break;
          case 1:
            colourspace_rgb_to_hsl(&rgb, &hsl);
            colourspace_hsl_to_rgb(&hsl, &back);
            break;
          case 2:
            colourspace_rgb_to_hsv(&rgb, &hsv);
            colourspace_hsv_to_rgb(&hsv, &back);
            break;
          default:
            colourspace_rgb_to_oklab(&rgb, &oklab);
            colourspace_oklab_to_rgb(&oklab, &back);
            break;
          }

          out = colourspace_to_colour(&back);
          if (out.primary != in.primary)
          {
            printf("colourspace: model %d: %d,%d,%d round-tripped wrong\n",
                   model, r, g, b);
            return 0;
          }
        }
      }

  return 1;
}

static int colourspace_test_near(const char *what, double got, double want)
{
  if (fabs(got - want) <= 1e-4)
    return 1;

  printf("colourspace: %s = %f, want %f\n", what, got, want);

  return 0;
}

/* Known reference values. */
static int colourspace_test_references(void)
{
  static const struct
  {
    unsigned char r, g, b;
    double        l, a, bb;
  }
  oklabs[] =
  {
    { 255, 255, 255, 1.000000,  0.000000,  0.000000 },
    { 255,   0,   0, 0.627955,  0.224863,  0.125846 },
    {   0, 255,   0, 0.866440, -0.233888,  0.179498 },
    {   0,   0, 255, 0.452014, -0.032457, -0.311528 },
  };

  int                 i;
  colourspace_rgb_t   rgb;
  colourspace_oklab_t oklab;
  colourspace_hsv_t   hsv;
  colourspace_hsl_t   hsl;
  colourspace_cmyk_t  cmyk;

  for (i = 0; i < NELEMS(oklabs); i++)
  {
    colourspace_from_colour(colour_rgb(oklabs[i].r, oklabs[i].g, oklabs[i].b),
                            &rgb);
    colourspace_rgb_to_oklab(&rgb, &oklab);
    if (!colourspace_test_near("oklab L", oklab.l, oklabs[i].l) ||
        !colourspace_test_near("oklab a", oklab.a, oklabs[i].a) ||
        !colourspace_test_near("oklab b", oklab.b, oklabs[i].bb))
      return 0;
  }

  /* orange: #ff8000 */
  colourspace_from_colour(colour_rgb(255, 128, 0), &rgb);

  colourspace_rgb_to_hsv(&rgb, &hsv);
  if (!colourspace_test_near("hsv h", hsv.h, 30.117647) ||
      !colourspace_test_near("hsv s", hsv.s, 1.0) ||
      !colourspace_test_near("hsv v", hsv.v, 1.0))
    return 0;

  colourspace_rgb_to_hsl(&rgb, &hsl);
  if (!colourspace_test_near("hsl h", hsl.h, 30.117647) ||
      !colourspace_test_near("hsl s", hsl.s, 1.0) ||
      !colourspace_test_near("hsl l", hsl.l, 0.5))
    return 0;

  colourspace_rgb_to_cmyk(&rgb, &cmyk);
  if (!colourspace_test_near("cmyk c", cmyk.c, 0.0) ||
      !colourspace_test_near("cmyk m", cmyk.m, 127.0 / 255.0) ||
      !colourspace_test_near("cmyk y", cmyk.y, 1.0) ||
      !colourspace_test_near("cmyk k", cmyk.k, 0.0))
    return 0;

  /* black has no hue in CMYK: all ink in K */
  colourspace_from_colour(colour_rgb(0, 0, 0), &rgb);
  colourspace_rgb_to_cmyk(&rgb, &cmyk);
  if (cmyk.c != 0.0 || cmyk.m != 0.0 || cmyk.y != 0.0 || cmyk.k != 1.0)
  {
    printf("colourspace: black cmyk wrong\n");
    return 0;
  }

  /* hue wraps */
  hsv.h = 360.0 + 120.0;
  hsv.s = 1.0;
  hsv.v = 1.0;
  colourspace_hsv_to_rgb(&hsv, &rgb);
  if (colourspace_to_colour(&rgb).primary != colour_rgb(0, 255, 0).primary)
  {
    printf("colourspace: hue 480 is not green\n");
    return 0;
  }

  return 1;
}

/* Out-of-gamut OKLab survives a round trip unclamped, then clamps. */
static int colourspace_test_gamut(void)
{
  colourspace_oklab_t oklab;
  colourspace_rgb_t   rgb;
  colourspace_oklab_t back;
  colour_t            c;

  oklab.l = 0.5;
  oklab.a = 0.4;
  oklab.b = -0.4;
  colourspace_oklab_to_rgb(&oklab, &rgb);
  if (rgb.r <= 1.0 && rgb.g >= 0.0 && rgb.b <= 1.0)
  {
    printf("colourspace: expected an out-of-gamut colour\n");
    return 0;
  }

  colourspace_rgb_to_oklab(&rgb, &back);
  if (!colourspace_test_near("gamut L", back.l, oklab.l) ||
      !colourspace_test_near("gamut a", back.a, oklab.a) ||
      !colourspace_test_near("gamut b", back.b, oklab.b))
    return 0;

  c = colourspace_to_colour(&rgb);
  if (colour_get_alpha(&c) != 255)
  {
    printf("colourspace: clamped colour not opaque\n");
    return 0;
  }

  return 1;
}

static int colourspace_test_hex(void)
{
  static const struct
  {
    const char   *text;
    int           ok;
    unsigned char r, g, b;
  }
  cases[] =
  {
    { "#ff8000",  1, 255, 128,   0 },
    { "FF8000",   1, 255, 128,   0 },
    { "#f80",     1, 255, 136,   0 },
    { "AbC",      1, 170, 187, 204 },
    { "#ff80",    0,   0,   0,   0 },
    { "#ff80001", 0,   0,   0,   0 },
    { "#gg0000",  0,   0,   0,   0 },
    { "",         0,   0,   0,   0 },
    { "#",        0,   0,   0,   0 },
  };

  int      i;
  colour_t c;
  result_t got;

  for (i = 0; i < NELEMS(cases); i++)
  {
    c   = colour_rgb(1, 2, 3);
    got = colourspace_parse_hex(cases[i].text, &c);
    if ((got == result_OK) != cases[i].ok)
    {
      printf("colourspace: hex \"%s\" accepted wrongly\n", cases[i].text);
      return 0;
    }

    if (cases[i].ok &&
        c.primary != colour_rgb(cases[i].r, cases[i].g, cases[i].b).primary)
    {
      printf("colourspace: hex \"%s\" parsed wrong\n", cases[i].text);
      return 0;
    }
  }

  return 1;
}

result_t colourspace_test(const char *resources)
{
  (void) resources;

  if (!colourspace_test_round_trips() ||
      !colourspace_test_references() ||
      !colourspace_test_gamut() ||
      !colourspace_test_hex())
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}
