/* framebuf/colourspace/colourspace.c -- colour model conversions */

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/colour.h"

#include "framebuf/colourspace.h"

/* ----------------------------------------------------------------------- */

void colourspace_from_colour(colour_t c, colourspace_rgb_t *rgb)
{
  unsigned int r, g, b;

  colour_get_rgb(&c, &r, &g, &b);

  rgb->r = r / 255.0;
  rgb->g = g / 255.0;
  rgb->b = b / 255.0;
}

static unsigned int colourspace_level(double x)
{
  return (unsigned int) lround(CLAMP(x, 0.0, 1.0) * 255.0);
}

colour_t colourspace_to_colour(const colourspace_rgb_t *rgb)
{
  return colour_rgb(colourspace_level(rgb->r),
                    colourspace_level(rgb->g),
                    colourspace_level(rgb->b));
}

static int colourspace_hexdigit(int ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';

  ch = tolower(ch);
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;

  return -1;
}

result_t colourspace_parse_hex(const char *s, colour_t *c)
{
  int          digits[6];
  int          n;
  unsigned int r, g, b;

  if (*s == '#')
    s++;

  for (n = 0; s[n] != '\0'; n++)
  {
    if (n == 6)
      return result_BAD_ARG; /* too long */

    digits[n] = colourspace_hexdigit((unsigned char) s[n]);
    if (digits[n] < 0)
      return result_BAD_ARG;
  }

  if (n == 3)
  {
    r = digits[0] * 17;
    g = digits[1] * 17;
    b = digits[2] * 17;
  }
  else if (n == 6)
  {
    r = digits[0] * 16 + digits[1];
    g = digits[2] * 16 + digits[3];
    b = digits[4] * 16 + digits[5];
  }
  else
  {
    return result_BAD_ARG;
  }

  *c = colour_rgb(r, g, b);

  return result_OK;
}

/* ----------------------------------------------------------------------- */

void colourspace_rgb_to_cmyk(const colourspace_rgb_t *rgb,
                             colourspace_cmyk_t      *cmyk)
{
  double k;
  double w;

  k = 1.0 - MAX(MAX(rgb->r, rgb->g), rgb->b);
  w = 1.0 - k;
  if (w <= 0.0)
  {
    cmyk->c = cmyk->m = cmyk->y = 0.0;
    cmyk->k = k;
    return;
  }

  cmyk->c = (w - rgb->r) / w;
  cmyk->m = (w - rgb->g) / w;
  cmyk->y = (w - rgb->b) / w;
  cmyk->k = k;
}

void colourspace_cmyk_to_rgb(const colourspace_cmyk_t *cmyk,
                             colourspace_rgb_t        *rgb)
{
  double w;

  w = 1.0 - cmyk->k;

  rgb->r = (1.0 - cmyk->c) * w;
  rgb->g = (1.0 - cmyk->m) * w;
  rgb->b = (1.0 - cmyk->y) * w;
}

/* ----------------------------------------------------------------------- */

/* Hue in degrees, 0..360, of rgb whose largest channel is max and whose
 * range is delta. Greys (delta 0) give 0. */
static double colourspace_hue(const colourspace_rgb_t *rgb,
                              double                   max,
                              double                   delta)
{
  double h;

  if (delta <= 0.0)
    return 0.0;

  if (max == rgb->r)
    h = (rgb->g - rgb->b) / delta;
  else if (max == rgb->g)
    h = (rgb->b - rgb->r) / delta + 2.0;
  else
    h = (rgb->r - rgb->g) / delta + 4.0;

  h *= 60.0;
  if (h < 0.0)
    h += 360.0;

  return h;
}

/* Build rgb from hue h (degrees, wrapped here), chroma and the offset m
 * added to every channel. Shared by HSL and HSV. */
static void colourspace_from_hue(double             h,
                                 double             chroma,
                                 double             m,
                                 colourspace_rgb_t *rgb)
{
  /* for each 60 degree sextant, which of v[] (0 = chroma, 1 = x, 2 = zero,
   * each plus m) goes to red, green and blue */
  static const unsigned char order[6][3] =
  {
    { 0, 1, 2 }, { 1, 0, 2 }, { 2, 0, 1 },
    { 2, 1, 0 }, { 1, 2, 0 }, { 0, 2, 1 }
  };

  double hp;
  int    sextant;
  double v[3];

  h = fmod(h, 360.0);
  if (h < 0.0)
    h += 360.0;

  hp      = h / 60.0;
  sextant = MIN((int) hp, 5);
  v[0]    = chroma + m;
  v[1]    = chroma * (1.0 - fabs(fmod(hp, 2.0) - 1.0)) + m;
  v[2]    = m;

  rgb->r = v[order[sextant][0]];
  rgb->g = v[order[sextant][1]];
  rgb->b = v[order[sextant][2]];
}

void colourspace_rgb_to_hsl(const colourspace_rgb_t *rgb,
                            colourspace_hsl_t       *hsl)
{
  double max;
  double min;
  double delta;
  double denom;

  max   = MAX(MAX(rgb->r, rgb->g), rgb->b);
  min   = MIN(MIN(rgb->r, rgb->g), rgb->b);
  delta = max - min;

  hsl->h = colourspace_hue(rgb, max, delta);
  hsl->l = (max + min) / 2.0;

  denom = 1.0 - fabs(2.0 * hsl->l - 1.0);
  hsl->s = (delta <= 0.0 || denom <= 0.0) ? 0.0 : delta / denom;
}

void colourspace_hsl_to_rgb(const colourspace_hsl_t *hsl,
                            colourspace_rgb_t       *rgb)
{
  double chroma;

  chroma = (1.0 - fabs(2.0 * hsl->l - 1.0)) * hsl->s;

  colourspace_from_hue(hsl->h, chroma, hsl->l - chroma / 2.0, rgb);
}

void colourspace_rgb_to_hsv(const colourspace_rgb_t *rgb,
                            colourspace_hsv_t       *hsv)
{
  double max;
  double min;
  double delta;

  max   = MAX(MAX(rgb->r, rgb->g), rgb->b);
  min   = MIN(MIN(rgb->r, rgb->g), rgb->b);
  delta = max - min;

  hsv->h = colourspace_hue(rgb, max, delta);
  hsv->s = (max <= 0.0 || delta <= 0.0) ? 0.0 : delta / max;
  hsv->v = max;
}

void colourspace_hsv_to_rgb(const colourspace_hsv_t *hsv,
                            colourspace_rgb_t       *rgb)
{
  double chroma;

  chroma = hsv->v * hsv->s;

  colourspace_from_hue(hsv->h, chroma, hsv->v - chroma, rgb);
}

/* ----------------------------------------------------------------------- */

/* sRGB transfer function, extended to negative values by odd symmetry so
 * out-of-gamut colours survive a round trip. */
static double colourspace_to_linear(double x)
{
  double a;

  a = fabs(x);
  a = (a <= 0.04045) ? a / 12.92 : pow((a + 0.055) / 1.055, 2.4);

  return (x < 0.0) ? -a : a;
}

static double colourspace_from_linear(double x)
{
  double a;

  a = fabs(x);
  a = (a <= 0.0031308) ? a * 12.92 : 1.055 * pow(a, 1.0 / 2.4) - 0.055;

  return (x < 0.0) ? -a : a;
}

static struct
{
  uint16_t decode[256];
  uint8_t  encode[1 << colourspace_ENCODE_BITS];
  int      ready;
}
colourspace_tables;

static void colourspace_make_tables(void)
{
  int    i;
  double c;

  if (colourspace_tables.ready)
    return;

  for (i = 0; i < 256; i++)
  {
    c = colourspace_to_linear(i / 255.0);
    colourspace_tables.decode[i] =
      (uint16_t) (c * colourspace_LINEAR_MAX + 0.5);
  }

  for (i = 0; i < (1 << colourspace_ENCODE_BITS); i++)
  {
    c = i / (double) ((1 << colourspace_ENCODE_BITS) - 1);
    colourspace_tables.encode[i] =
      (uint8_t) (colourspace_from_linear(c) * 255 + 0.5);
  }

  colourspace_tables.ready = 1;
}

const uint16_t *colourspace_srgb_decode(void)
{
  colourspace_make_tables();

  return colourspace_tables.decode;
}

const uint8_t *colourspace_srgb_encode(void)
{
  colourspace_make_tables();

  return colourspace_tables.encode;
}

void colourspace_rgb_to_oklab(const colourspace_rgb_t *rgb,
                              colourspace_oklab_t     *oklab)
{
  double r, g, b;
  double l, m, s;

  r = colourspace_to_linear(rgb->r);
  g = colourspace_to_linear(rgb->g);
  b = colourspace_to_linear(rgb->b);

  l = cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  m = cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  s = cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);

  oklab->l = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s;
  oklab->a = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s;
  oklab->b = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s;
}

void colourspace_oklab_to_rgb(const colourspace_oklab_t *oklab,
                              colourspace_rgb_t         *rgb)
{
  double l, m, s;
  double r, g, b;

  l = oklab->l + 0.3963377774 * oklab->a + 0.2158037573 * oklab->b;
  m = oklab->l - 0.1055613458 * oklab->a - 0.0638541728 * oklab->b;
  s = oklab->l - 0.0894841775 * oklab->a - 1.2914855480 * oklab->b;

  l = l * l * l;
  m = m * m * m;
  s = s * s * s;

  r =  4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
  g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
  b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

  rgb->r = colourspace_from_linear(r);
  rgb->g = colourspace_from_linear(g);
  rgb->b = colourspace_from_linear(b);
}
