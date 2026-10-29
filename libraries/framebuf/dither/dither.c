/* framebuf/dither/dither.c -- error-diffusion and ordered dithering to a
 * palette */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/colourspace.h"
#include "framebuf/pattern.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/pixelmap.h"
#include "utils/rng.h"

#include "framebuf/dither.h"

/* ----------------------------------------------------------------------- */

#define DITHER_MAX_W 5
#define DITHER_MAX_H 3
#define DITHER_PAD   3 /* columns either side absorbing off-image error: the
                        * furthest reach of any kernel, Shiau-Fan 2's */
#define OSTRO_DIV    64 /* Ostromoukhov's coefficients are rescaled to this */
#define LIMIT_STEPS  2 /* carried error limit, in palette gaps */

/* coeff[0] is the current row; entries at or left of cx on it are zero. An
 * ordered method has no coefficients and a threshold map instead. */
typedef struct dither_kernel
{
  int           w, h;
  int           cx;      /* column of the current pixel */
  int           divisor;
  unsigned char coeff[DITHER_MAX_H][DITHER_MAX_W];
  int         (*threshold)(int x, int y); /* 0..levels-1, or NULL to diffuse */
  int           levels;  /* distinct thresholds, at most DITHER_MAX_LEVELS */
}
dither_kernel_t;

#define DITHER_MAX_LEVELS 256 /* the 16x16 Bayer matrix's */

static int dither_bayer2(int x, int y)
{
  return pattern_bayer_threshold_n(x, y, 1);
}

static int dither_bayer4(int x, int y)
{
  return pattern_bayer_threshold_n(x, y, 2);
}

static int dither_bayer16(int x, int y)
{
  return pattern_bayer_threshold_n(x, y, 4);
}

/* White-noise threshold, 0..63: a hash of the pixel's position, so the
 * same image always dithers the same way. */
static int dither_random_threshold(int x, int y)
{
  uint32_t h;

  h  = (uint32_t) x * 0x8da6b343u ^ (uint32_t) y * 0xd8163841u;
  h ^= h >> 16;
  h *= 0x7feb352du;
  h ^= h >> 15;
  h *= 0x846ca68bu;
  h ^= h >> 16;

  return (int) (h >> 26);
}

static const dither_kernel_t dither_kernels[dither__LIMIT] =
{
  /* none */
  { 1, 1, 0, 1, { { 0 } }, NULL, 0 },
  /* clustered-dot halftone */
  { 1, 1, 0, 1,  { { 0 } }, pattern_clustered_threshold, 64 },
  /* random threshold */
  { 1, 1, 0, 1,  { { 0 } }, dither_random_threshold, 64 },
  /* Bayer, 2x2 to 16x16 */
  { 1, 1, 0, 1,  { { 0 } }, dither_bayer2, 4 },
  { 1, 1, 0, 1,  { { 0 } }, dither_bayer4, 16 },
  { 1, 1, 0, 1,  { { 0 } }, pattern_bayer_threshold, 64 },
  { 1, 1, 0, 1,  { { 0 } }, dither_bayer16, 256 },
  /* blue noise */
  { 1, 1, 0, 1,  { { 0 } }, pattern_blue_noise_threshold, 64 },
  /* Knoll: candidates chosen by Bayer threshold */
  { 1, 1, 0, 1,  { { 0 } }, pattern_bayer_threshold, 64 },
  /* Yliluoma: plan entries chosen by Bayer threshold */
  { 1, 1, 0, 1,  { { 0 } }, pattern_bayer_threshold, 64 },
  /* Floyd-Steinberg */
  { 3, 2, 1, 16, { { 0, 0, 7 },
                   { 3, 5, 1 } }, NULL, 0 },
  /* Jarvis, Judice and Ninke */
  { 5, 3, 2, 48, { { 0, 0, 0, 7, 5 },
                   { 3, 5, 7, 5, 3 },
                   { 1, 3, 5, 3, 1 } }, NULL, 0 },
  /* Stucki */
  { 5, 3, 2, 42, { { 0, 0, 0, 8, 4 },
                   { 2, 4, 8, 4, 2 },
                   { 1, 2, 4, 2, 1 } }, NULL, 0 },
  /* Atkinson */
  { 4, 3, 1, 8,  { { 0, 0, 1, 1 },
                   { 1, 1, 1, 0 },
                   { 0, 1, 0, 0 } }, NULL, 0 },
  /* Burkes */
  { 5, 2, 2, 32, { { 0, 0, 0, 8, 4 },
                   { 2, 4, 8, 4, 2 } }, NULL, 0 },
  /* Sierra */
  { 5, 3, 2, 32, { { 0, 0, 0, 5, 3 },
                   { 2, 4, 5, 4, 2 },
                   { 0, 2, 3, 2, 0 } }, NULL, 0 },
  /* Sierra two-row */
  { 5, 2, 2, 16, { { 0, 0, 0, 4, 3 },
                   { 1, 2, 3, 2, 1 } }, NULL, 0 },
  /* Sierra lite */
  { 3, 2, 1, 4,  { { 0, 0, 2 },
                   { 1, 1, 0 } }, NULL, 0 },
  /* Fan */
  { 4, 2, 2, 16, { { 0, 0, 0, 7 },
                   { 1, 3, 5, 0 } }, NULL, 0 },
  /* Shiau-Fan, four cells */
  { 4, 2, 2, 8,  { { 0, 0, 0, 4 },
                   { 1, 1, 2, 0 } }, NULL, 0 },
  /* Shiau-Fan, five cells */
  { 5, 2, 3, 16, { { 0, 0, 0, 0, 8 },
                   { 1, 1, 2, 4, 0 } }, NULL, 0 },
  /* Riemersma: no kernel; error travels along a Hilbert curve */
  { 1, 1, 0, 1,  { { 0 } }, NULL, 0 },
  /* Ostromoukhov: coefficients vary per pixel; only the shape is fixed */
  { 3, 2, 1, OSTRO_DIV, { { 0 } }, NULL, 0 },
  /* Zhou-Fang: Ostromoukhov's shape */
  { 3, 2, 1, OSTRO_DIV, { { 0 } }, NULL, 0 },
};

/* ----------------------------------------------------------------------- */

/* add accumulated error `e`, scaled by `total`, to channel value `c`,
 * clamping to 0..max */
static unsigned int dither_apply(unsigned int c, int e, int total, int max)
{
  int v;

  v = (e >= 0) ? (e + total / 2) / total : -((-e + total / 2) / total);

  return (unsigned int) CLAMP((int) c + v, 0, max);
}

/* Add accumulated error e[0..2], scaled by `total`, to a pixel's channels,
 * clamping each to 0..max. Error the palette cannot express, such as a
 * colour's chroma against a grey palette, would otherwise grow without bound
 * and drive channels to their limits, losing brightness where they clip. So
 * an error beyond lim in any channel is scaled down whole, keeping its
 * direction and so its effect on brightness. */
static void dither_apply3(unsigned int *r,
                          unsigned int *g,
                          unsigned int *b,
                          const int    *e,
                          int           total,
                          int           lim,
                          int           max)
{
  int v[3];
  int i;
  int m;

  for (i = 0; i < 3; i++)
    v[i] = (e[i] >= 0) ? (e[i] + total / 2) / total
                       : -((-e[i] + total / 2) / total);

  m = MAX(abs(v[0]), MAX(abs(v[1]), abs(v[2])));
  if (m > lim)
    for (i = 0; i < 3; i++)
      v[i] = v[i] * lim / m;

  *r = (unsigned int) CLAMP((int) *r + v[0], 0, max);
  *g = (unsigned int) CLAMP((int) *g + v[1], 0, max);
  *b = (unsigned int) CLAMP((int) *b + v[2], 0, max);
}

/* The most error dither_apply3 lets a channel carry: LIMIT_STEPS of the
 * palette's typical gap, enough to leave ordinary error alone. */
static int dither_limit(const pixelmap_t *pm, int max)
{
  return LIMIT_STEPS * (int) pm->step * max / 255;
}

static unsigned int dither_lookup(const pixelmap_t *pm,
                                  unsigned int      r,
                                  unsigned int      g,
                                  unsigned int      b)
{
  unsigned int idx;
  unsigned int per_byte;
  unsigned int bit;

  idx = ((r >> (8 - pm->rbits)) << (pm->gbits + pm->bbits)) |
        ((g >> (8 - pm->gbits)) << pm->bbits) |
        ( b >> (8 - pm->bbits));

  if (pm->dest_log2bpp == 3)
    return pm->entries[idx];

  per_byte = 8u >> pm->dest_log2bpp;
  bit      = (idx % per_byte) << pm->dest_log2bpp;

  return (pm->entries[idx / per_byte] >> bit) &
         ((1u << (1u << pm->dest_log2bpp)) - 1);
}

/* Nearest palette index for channels r, g, b: sRGB, or linear when dec is
 * set. */
static unsigned int dither_nearest(const pixelmap_t *pm,
                                   const uint16_t   *dec,
                                   unsigned int      r,
                                   unsigned int      g,
                                   unsigned int      b)
{
  const uint8_t *enc;

  if (dec == NULL)
    return dither_lookup(pm, r, g, b);

  enc = colourspace_srgb_encode();

  return dither_lookup(pm,
                       enc[r >> (16 - colourspace_ENCODE_BITS)],
                       enc[g >> (16 - colourspace_ENCODE_BITS)],
                       enc[b >> (16 - colourspace_ENCODE_BITS)]);
}

/* Nudge sRGB channel c by an ordered method's bias t, a share of the
 * palette's sRGB step. With dec, c is first moved within the step holding
 * it to where its linear light falls between the step's ends, so that the
 * threshold splits pixels between the palette entries either side in
 * proportion to their physical brightness.
 * ponytail: assumes palette levels at multiples of pm->step, as on an even
 * grid; an uneven palette's true brackets would need a search per pixel. */
static unsigned int dither_bias(const pixelmap_t *pm,
                                const uint16_t   *dec,
                                unsigned int      c,
                                int               t)
{
  unsigned int a, b;

  if (dec && pm->step > 0)
  {
    a = c / pm->step * pm->step;
    b = MIN(a + pm->step, 255u);
    if (b > a)
      c = a + (unsigned int) ((dec[c] - dec[a]) * (b - a) / (dec[b] - dec[a]));
  }

  return (unsigned int) CLAMP((int) c + t, 0, 255);
}

/* Store a palette index using the bitmap module's packing: p4 low nibble
 * first, p1 and p2 most significant bits first. */
static void dither_put(unsigned char *row,
                       int            x,
                       int            log2bpp,
                       unsigned int   index)
{
  switch (log2bpp)
  {
  case 0:
    row[x >> 3] |= (unsigned char) (index << (7 - (x & 7)));
    break;
  case 1:
    row[x >> 2] |= (unsigned char) (index << (6 - ((x & 3) << 1)));
    break;
  case 2:
    row[x >> 1] |= (unsigned char) (index << ((x & 1) << 2));
    break;
  default:
    row[x] = (unsigned char) index;
    break;
  }
}

/* ----- Knoll pattern dithering ----- */

/* ponytail: 16 candidates gives 16 mixing levels per pixel; raise to 64 for
 * the full Bayer range at four times the lookups */
#define KNOLL_N 16

static int dither_luma(const colour_t *c)
{
  unsigned int r, g, b;

  colour_get_rgb(c, &r, &g, &b);

  return (int) (299 * r + 587 * g + 114 * b);
}

/* rank[i] is palette entry i's position in luma order, darkest first */
static void dither_luma_ranks(const colour_t *palette,
                              int             ncolours,
                              unsigned char  *rank)
{
  unsigned char order[256];
  int           i, j;
  unsigned char c;

  for (i = 0; i < ncolours; i++)
  {
    c = (unsigned char) i;
    for (j = i; j > 0 && dither_luma(&palette[order[j - 1]]) >
                         dither_luma(&palette[c]); j--)
      order[j] = order[j - 1];
    order[j] = c;
  }

  for (i = 0; i < ncolours; i++)
    rank[order[i]] = (unsigned char) i;
}

/* Thomas Knoll's pattern dither: pick KNOLL_N candidates by looking up the
 * source plus the error the candidates so far have accumulated (scaled by
 * strength), so that their mix approximates the source. Then take the
 * candidate at threshold t (0..63) in luma order. r, g, b are sRGB; with
 * dec set the error is measured in linear light, 0..max. */
static unsigned int dither_knoll(const pixelmap_t    *pm,
                                 const colour_t      *palette,
                                 const unsigned char *rank,
                                 const uint16_t      *dec,
                                 int                  max,
                                 unsigned int         r,
                                 unsigned int         g,
                                 unsigned int         b,
                                 int                  t,
                                 int                  strength)
{
  unsigned int idx[KNOLL_N];
  int          count[KNOLL_N];
  int          n;
  int          er, eg, eb;
  int          i, j;
  unsigned int c;
  unsigned int pr, pg, pb;
  int          want;

  if (dec)
  {
    r = dec[r];
    g = dec[g];
    b = dec[b];
  }

  /* distinct candidates in idx[0..n), each seen count[] times */
  n  = 0;
  er = eg = eb = 0;
  for (i = 0; i < KNOLL_N; i++)
  {
    c = dither_nearest(pm, dec,
                       dither_apply(r, er * strength, 100, max),
                       dither_apply(g, eg * strength, 100, max),
                       dither_apply(b, eb * strength, 100, max));
    colour_get_rgb(&palette[c], &pr, &pg, &pb);
    if (dec)
    {
      pr = dec[pr];
      pg = dec[pg];
      pb = dec[pb];
    }
    er += (int) r - (int) pr;
    eg += (int) g - (int) pg;
    eb += (int) b - (int) pb;

    for (j = 0; j < n && idx[j] != c; j++)
      ;
    if (j == n)
    {
      /* insert in luma order */
      for (; j > 0 && rank[idx[j - 1]] > rank[c]; j--)
      {
        idx[j]   = idx[j - 1];
        count[j] = count[j - 1];
      }
      idx[j]   = c;
      count[j] = 0;
      n++;
    }
    count[j]++;
  }

  want = t * KNOLL_N / 64;
  for (j = 0; want >= count[j]; j++)
    want -= count[j];

  return idx[j];
}

/* ----- Yliluoma dithering ----- */

#define YLILUOMA_N 16 /* plan entries */

/* Plans are cached per source colour cut to 5:6:5 bits, each built for its
 * bucket's centre: a plan only mixes in sixteenths, so the cut costs
 * nothing visible and bounds the work at 65536 plans. */
#define YLILUOMA_KEY(r, g, b) \
  ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

typedef struct dither_yliluoma
{
  double        pal[256][3]; /* palette channels, 0..255 in sRGB or in
                              * linear light */
  unsigned char made[65536]; /* non-zero: plan[key] is built */
  unsigned char plan[65536][YLILUOMA_N];
}
dither_yliluoma_t;

/* Yliluoma's psychovisual distance between colours a and b, channels
 * 0..255: channel differences weighted by their luma share, plus the luma
 * difference, which counts for more. */
static double dither_yliluoma_compare(const double *a, const double *b)
{
  double la, lb;
  double dl, dr, dg, db;

  la = (a[0] * 299 + a[1] * 587 + a[2] * 114) / (255.0 * 1000);
  lb = (b[0] * 299 + b[1] * 587 + b[2] * 114) / (255.0 * 1000);
  dl = la - lb;
  dr = (a[0] - b[0]) / 255.0;
  dg = (a[1] - b[1]) / 255.0;
  db = (a[2] - b[2]) / 255.0;

  return (dr * dr * 0.299 + dg * dg * 0.587 + db * db * 0.114) * 0.75 +
         dl * dl;
}

/* Build into plan Yliluoma's mix of YLILUOMA_N palette entries for colour
 * want: each step adds the entry and power-of-two quantity whose mix with
 * the plan so far best matches want, the quantity at most the plan's size
 * so far. The plan ends sorted into luma order. */
static void dither_yliluoma_plan(const dither_yliluoma_t *yl,
                                 int                      ncolours,
                                 const unsigned char     *rank,
                                 const double            *want,
                                 unsigned char           *plan)
{
  double        so_far[3];
  int           total;
  double        least;
  int           chosen;
  int           amount;
  int           i;
  int           p;
  int           ch;
  double        test[3];
  double        penalty;
  int           j;
  unsigned char c;

  so_far[0] = so_far[1] = so_far[2] = 0.0;

  total = 0;
  while (total < YLILUOMA_N)
  {
    least  = -1.0;
    chosen = 0;
    amount = 1;
    for (i = 0; i < ncolours; i++)
    {
      for (p = 1; p <= MAX(1, total) && total + p <= YLILUOMA_N; p *= 2)
      {
        for (ch = 0; ch < 3; ch++)
          test[ch] = (so_far[ch] + p * yl->pal[i][ch]) / (total + p);

        penalty = dither_yliluoma_compare(want, test);
        if (least < 0.0 || penalty < least)
        {
          least  = penalty;
          chosen = i;
          amount = p;
        }
      }
    }

    for (p = 0; p < amount; p++)
      plan[total + p] = (unsigned char) chosen;
    for (ch = 0; ch < 3; ch++)
      so_far[ch] += amount * yl->pal[chosen][ch];
    total += amount;
  }

  /* insertion sort into luma order */
  for (i = 1; i < YLILUOMA_N; i++)
  {
    c = plan[i];
    for (j = i; j > 0 && rank[plan[j - 1]] > rank[c]; j--)
      plan[j] = plan[j - 1];
    plan[j] = c;
  }
}

/* Joel Yliluoma's arbitrary-palette positional dither, algorithm 2: take the
 * entry at threshold t (0..63) from the plan for r, g, b, pulled toward the
 * plan's middle as strength falls. r, g, b are sRGB; with dec set the plan
 * mixes in linear light, as yl->pal must then hold. */
static unsigned int dither_yliluoma(dither_yliluoma_t   *yl,
                                    int                  ncolours,
                                    const unsigned char *rank,
                                    const uint16_t      *dec,
                                    unsigned int         r,
                                    unsigned int         g,
                                    unsigned int         b,
                                    int                  t,
                                    int                  strength)
{
  unsigned int key;
  double       want[3];
  int          ch;
  int          pos;

  key = YLILUOMA_KEY(r, g, b);
  if (!yl->made[key])
  {
    want[0] = (r & ~7u) | 4;
    want[1] = (g & ~3u) | 2;
    want[2] = (b & ~7u) | 4;
    if (dec)
      for (ch = 0; ch < 3; ch++)
        want[ch] = dec[(int) want[ch]] / 257.0;
    dither_yliluoma_plan(yl, ncolours, rank, want, yl->plan[key]);
    yl->made[key] = 1;
  }

  pos = t * YLILUOMA_N / 64 - YLILUOMA_N / 2;

  return yl->plan[key][YLILUOMA_N / 2 + pos * strength / 100];
}

/* ----- Ostromoukhov dithering ----- */

/* Ostromoukhov's variable coefficients (SIGGRAPH 2001) for levels 0..127:
 * right, down-left and down neighbours, and their sum. Levels 128..255
 * mirror them. */
static const unsigned int dither_ostro[128][4] =
{
  {   13,    0,    5,   18 }, {   13,    0,    5,   18 },
  {   21,    0,   10,   31 }, {    7,    0,    4,   11 },
  {    8,    0,    5,   13 }, {   47,    3,   28,   78 },
  {   23,    3,   13,   39 }, {   15,    3,    8,   26 },
  {   22,    6,   11,   39 }, {   43,   15,   20,   78 },
  {    7,    3,    3,   13 }, {  501,  224,  211,  936 },
  {  249,  116,  103,  468 }, {  165,   80,   67,  312 },
  {  123,   62,   49,  234 }, {  489,  256,  191,  936 },
  {   81,   44,   31,  156 }, {  483,  272,  181,  936 },
  {   60,   35,   22,  117 }, {   53,   32,   19,  104 },
  {  237,  148,   83,  468 }, {  471,  304,  161,  936 },
  {    3,    2,    1,    6 }, {  459,  304,  161,  924 },
  {   38,   25,   14,   77 }, {  453,  296,  175,  924 },
  {  225,  146,   91,  462 }, {  149,   96,   63,  308 },
  {  111,   71,   49,  231 }, {   63,   40,   29,  132 },
  {   73,   46,   35,  154 }, {  435,  272,  217,  924 },
  {  108,   67,   56,  231 }, {   13,    8,    7,   28 },
  {  213,  130,  119,  462 }, {  423,  256,  245,  924 },
  {    5,    3,    3,   11 }, {  281,  173,  162,  616 },
  {  141,   89,   78,  308 }, {  283,  183,  150,  616 },
  {   71,   47,   36,  154 }, {  285,  193,  138,  616 },
  {   13,    9,    6,   28 }, {   41,   29,   18,   88 },
  {   36,   26,   15,   77 }, {  289,  213,  114,  616 },
  {  145,  109,   54,  308 }, {  291,  223,  102,  616 },
  {   73,   57,   24,  154 }, {  293,  233,   90,  616 },
  {   21,   17,    6,   44 }, {  295,  243,   78,  616 },
  {   37,   31,    9,   77 }, {   27,   23,    6,   56 },
  {  149,  129,   30,  308 }, {  299,  263,   54,  616 },
  {   75,   67,   12,  154 }, {   43,   39,    6,   88 },
  {  151,  139,   18,  308 }, {  303,  283,   30,  616 },
  {   38,   36,    3,   77 }, {  305,  293,   18,  616 },
  {  153,  149,    6,  308 }, {  307,  303,    6,  616 },
  {    1,    1,    0,    2 }, {  101,  105,    2,  208 },
  {   49,   53,    2,  104 }, {   95,  107,    6,  208 },
  {   23,   27,    2,   52 }, {   89,  109,   10,  208 },
  {   43,   55,    6,  104 }, {   83,  111,   14,  208 },
  {    5,    7,    1,   13 }, {  172,  181,   37,  390 },
  {   97,   76,   22,  195 }, {   72,   41,   17,  130 },
  {  119,   47,   29,  195 }, {    4,    1,    1,    6 },
  {    4,    1,    1,    6 }, {    4,    1,    1,    6 },
  {    4,    1,    1,    6 }, {    4,    1,    1,    6 },
  {    4,    1,    1,    6 }, {    4,    1,    1,    6 },
  {    4,    1,    1,    6 }, {    4,    1,    1,    6 },
  {   65,   18,   17,  100 }, {   95,   29,   26,  150 },
  {  185,   62,   53,  300 }, {   30,   11,    9,   50 },
  {   35,   14,   11,   60 }, {   85,   37,   28,  150 },
  {   55,   26,   19,  100 }, {   80,   41,   29,  150 },
  {  155,   86,   59,  300 }, {    5,    3,    2,   10 },
  {    5,    3,    2,   10 }, {    5,    3,    2,   10 },
  {    5,    3,    2,   10 }, {    5,    3,    2,   10 },
  {    5,    3,    2,   10 }, {    5,    3,    2,   10 },
  {    5,    3,    2,   10 }, {    5,    3,    2,   10 },
  {    5,    3,    2,   10 }, {    5,    3,    2,   10 },
  {    5,    3,    2,   10 }, {    5,    3,    2,   10 },
  {  305,  176,  119,  600 }, {  155,   86,   59,  300 },
  {  105,   56,   39,  200 }, {   80,   41,   29,  150 },
  {   65,   32,   23,  120 }, {   55,   26,   19,  100 },
  {  335,  152,  113,  600 }, {   85,   37,   28,  150 },
  {  115,   48,   37,  200 }, {   35,   14,   11,   60 },
  {  355,  136,  109,  600 }, {   30,   11,    9,   50 },
  {  365,  128,  107,  600 }, {  185,   62,   53,  300 },
  {   25,    8,    7,   40 }, {   95,   29,   26,  150 },
  {  385,  112,  103,  600 }, {   65,   18,   17,  100 },
  {  395,  104,  101,  600 }, {    4,    1,    1,    6 }
};

/* Zhou and Fang's coefficients (SIGGRAPH 2003) at key levels in 0..127:
 * level, then right, down-left and down neighbours. Levels between are
 * interpolated and levels 128..255 mirror them. */
static const unsigned int dither_zf_coeffs[][4] =
{
  {   0,      13,      0,      5 },
  {   1, 1300249,      0, 499250 },
  {   2,  213113,    287,  99357 },
  {   3,  351854,      0, 199965 },
  {   4,  801100,      0, 490999 },
  {  10,  704075, 297466, 303694 },
  {  22,   46613,  31917,  21469 },
  {  32,   47482,  30617,  21900 },
  {  44,   43024,  42131,  14826 },
  {  64,   36411,  43219,  20369 },
  {  72,   38477,  53843,   7678 },
  {  77,   40503,  51547,   7948 },
  {  85,   35865,  34108,  30026 },
  {  95,   34117,  36899,  28983 },
  { 102,   35464,  35049,  29485 },
  { 107,   16477,  18810,  14712 },
  { 112,   33360,  37954,  28685 },
  { 127,   35269,  36066,  28664 }
};

/* Zhou and Fang's coefficients for level v, 0..127, into c: right,
 * down-left, down and their sum. */
static void dither_zf_coeffs_for(unsigned int v, unsigned int c[4])
{
  const unsigned int *a;
  const unsigned int *b;
  unsigned int        d;
  int                 i;

  for (i = 1; dither_zf_coeffs[i][0] < v; i++)
    ;
  a = dither_zf_coeffs[i - 1];
  b = dither_zf_coeffs[i];
  d = b[0] - a[0];

  for (i = 0; i < 3; i++)
    c[i] = (b[i + 1] * (v - a[0]) + a[i + 1] * (b[0] - v)) / d;
  c[3] = c[0] + c[1] + c[2];
}

/* Spread one pixel's error Ostromoukhov's way: each channel's split
 * between the right, down-left and down neighbours depends on that
 * channel's source level, rescaled to sum to OSTRO_DIV. Zhou and Fang's
 * coefficients replace Ostromoukhov's when zhou_fang is set. cur and next
 * are the error rows; dir mirrors the split on right-to-left rows. */
static void dither_ostromoukhov(int                       *err,
                                int                        stride,
                                int                        cur,
                                int                        next,
                                int                        x,
                                int                        dir,
                                const pixelfmt_rgbx8888_t *px,
                                int                        qr,
                                int                        qg,
                                int                        qb,
                                int                        zhou_fang)
{
  unsigned int v[3];
  int          q[3];
  int          ch;
  unsigned int l;
  unsigned int c[4];
  int          wr, wdl, wd;

  v[0] = PIXELFMT_Rxxx8888(*px);
  v[1] = PIXELFMT_xGxx8888(*px);
  v[2] = PIXELFMT_xxBx8888(*px);
  q[0] = qr;
  q[1] = qg;
  q[2] = qb;

  for (ch = 0; ch < 3; ch++)
  {
    l = v[ch] < 128 ? v[ch] : 255 - v[ch];
    if (zhou_fang)
      dither_zf_coeffs_for(l, c);
    else
      memcpy(c, dither_ostro[l], sizeof(c));

    /* Zhou and Fang's largest coefficient times OSTRO_DIV fits an int */
    wr  = (int) ((c[0] * OSTRO_DIV + c[3] / 2) / c[3]);
    wdl = (int) ((c[1] * OSTRO_DIV + c[3] / 2) / c[3]);
    wd  = OSTRO_DIV - wr - wdl; /* so the mean is kept exactly */

    err[(cur  * stride + x + dir + DITHER_PAD) * 3 + ch] += q[ch] * wr;
    err[(next * stride + x - dir + DITHER_PAD) * 3 + ch] += q[ch] * wdl;
    err[(next * stride + x       + DITHER_PAD) * 3 + ch] += q[ch] * wd;
  }
}

/* ----- Zhou-Fang dithering ----- */

/* Zhou and Fang's threshold modulation strength (SIGGRAPH 2003), in percent,
 * at key levels in 0..127; levels between are interpolated and levels
 * 128..255 mirror them. */
static const unsigned char dither_zf_keys[][2] =
{
  {   0,   0 },
  {  44,  34 },
  {  64,  50 },
  {  85, 100 },
  {  95,  17 },
  { 102,  50 },
  { 107,  70 },
  { 112,  79 },
  { 127, 100 }
};

/* Nudge channel c, already carrying its error, for the lookup only. v is
 * the channel's source level and t the pixel's noise, -128..127. The nudge
 * is at most half pm's step, scaled by the strength for v, by opts'
 * strength and to max. */
static unsigned int dither_zhou_fang(unsigned int            c,
                                     unsigned int            v,
                                     int                     t,
                                     const pixelmap_t       *pm,
                                     const dither_options_t *opts,
                                     int                     max)
{
  int i;
  int m;
  int n;

  if (v >= 128)
    v = 255 - v;

  for (i = 1; dither_zf_keys[i][0] < v; i++)
    ;
  m = dither_zf_keys[i - 1][1] +
      (dither_zf_keys[i][1] - dither_zf_keys[i - 1][1]) *
      ((int) v - dither_zf_keys[i - 1][0]) /
      (dither_zf_keys[i][0] - dither_zf_keys[i - 1][0]);

  n = t * m * (int) pm->step / (256 * 100);

  return dither_apply(c, n * max / 255 * opts->strength, 100, max);
}

/* ----- Riemersma dithering ----- */

#define RIEMERSMA_Q     16 /* errors remembered along the curve */
#define RIEMERSMA_RATIO 16 /* newest error's weight over the oldest's */
#define RIEMERSMA_ONE   1024 /* the newest error's weight */

/* Position d along a Hilbert curve filling an n by n square, n a power of
 * two. */
static void dither_hilbert(int n, int d, int *x, int *y)
{
  int s;
  int rx, ry;
  int tmp;

  *x = *y = 0;
  for (s = 1; s < n; s *= 2)
  {
    rx = 1 & (d / 2);
    ry = 1 & (d ^ rx);
    if (ry == 0)
    {
      if (rx == 1)
      {
        *x = s - 1 - *x;
        *y = s - 1 - *y;
      }
      tmp = *x;
      *x  = *y;
      *y  = tmp;
    }
    *x += s * rx;
    *y += s * ry;
    d /= 4;
  }
}

/* Riemersma's dither: walk a Hilbert curve over the image, adding to each
 * pixel a weighted sum of the last RIEMERSMA_Q quantisation errors, the
 * newest weighted 1 and the oldest 1 / RIEMERSMA_RATIO. As in Riemersma's
 * original, an error is the source pixel less its output, not counting the
 * sum added to it: the sum then tracks the error left over across the
 * window, so the mean is kept. The curve has no preferred direction, so
 * there are no directional artefacts.
 *
 * ponytail: the curve covers the enclosing power-of-two square and skips
 * the cells outside the image, wasting up to 4x the steps on long thin
 * images; use a generalised (gilbert) curve if that matters. */
static void dither_riemersma(const bitmap_t         *src,
                             const dither_options_t *opts,
                             const pixelmap_t       *pm,
                             const colour_t         *palette,
                             const uint16_t         *dec,
                             int                     max,
                             bitmap_t               *out)
{
  int                        weight[RIEMERSMA_Q];
  int                        queue[RIEMERSMA_Q][3];
  int                        head;
  int                        n;
  int                        d;
  int                        x, y;
  int                        a;
  const pixelfmt_rgbx8888_t *px;
  unsigned int               c[3];
  int                        ch;
  unsigned int               s[3];
  int                        sum[3];
  unsigned int               index;
  unsigned int               p[3];
  int                        log2bpp;

  /* weight[a] for an error a steps old: RATIO^(-a / (Q - 1)), scaled by
   * RIEMERSMA_ONE */
  for (a = 0; a < RIEMERSMA_Q; a++)
    weight[a] = (int) (RIEMERSMA_ONE * pow(RIEMERSMA_RATIO,
                                           -a / (double) (RIEMERSMA_Q - 1)) +
                       0.5);

  memset(queue, 0, sizeof(queue));
  head = 0;

  for (n = 1; n < src->size.w || n < src->size.h; n *= 2)
    ;

  log2bpp = pixelfmt_log2bpp(out->format);

  for (d = 0; d < n * n; d++)
  {
    dither_hilbert(n, d, &x, &y);
    if (x >= src->size.w || y >= src->size.h)
      continue;

    px = (const pixelfmt_rgbx8888_t *)
         ((const unsigned char *) src->base + y * src->rowbytes) + x;
    c[0] = PIXELFMT_Rxxx8888(*px);
    c[1] = PIXELFMT_xGxx8888(*px);
    c[2] = PIXELFMT_xxBx8888(*px);

    /* the newest error is at head - 1; raw errors are at most 65535 and the
     * weights RIEMERSMA_ONE, so 16 of them fit an int */
    for (ch = 0; ch < 3; ch++)
    {
      if (dec)
        c[ch] = dec[c[ch]];
      s[ch] = c[ch];

      sum[ch] = 0;
      for (a = 0; a < RIEMERSMA_Q; a++)
        sum[ch] += queue[(head - 1 - a + RIEMERSMA_Q) % RIEMERSMA_Q][ch] *
                   weight[a];
      sum[ch] = sum[ch] / RIEMERSMA_ONE * opts->strength;
    }
    dither_apply3(&c[0], &c[1], &c[2], sum, 100, dither_limit(pm, max), max);

    index = dither_nearest(pm, dec, c[0], c[1], c[2]);
    dither_put((unsigned char *) out->base + y * out->rowbytes, x, log2bpp,
               index);

    colour_get_rgb(&palette[index], &p[0], &p[1], &p[2]);
    for (ch = 0; ch < 3; ch++)
      queue[head][ch] = (int) s[ch] - (int) (dec ? dec[p[ch]] : p[ch]);
    head = (head + 1) % RIEMERSMA_Q;
  }
}

/* ----------------------------------------------------------------------- */

result_t dither_bitmap(const bitmap_t         *src,
                       const dither_options_t *opts,
                       bitmap_t              **dst)
{
  result_t                   rc;
  const dither_kernel_t     *k;
  int                        log2bpp;
  pixelfmt_t                 fmt;
  const pixelmap_t          *pm;
  colour_t                   palette[256];
  int                        rowbytes;
  unsigned char             *base;
  bitmap_t                  *out;
  int                        stride;
  int                       *err;
  int                        total;
  const uint16_t            *dec;
  int                        max;
  int                        bias[DITHER_MAX_LEVELS];
  unsigned char              rank[256];
  dither_yliluoma_t         *yl;
  int                        i;
  unsigned int               pr, pg, pb;
  int                        x, y;
  int                        cur;
  int                        dir;
  const unsigned char       *srcrow;
  unsigned char             *dstrow;
  const pixelfmt_rgbx8888_t *px;
  int                       *e;
  unsigned int               r, g, b;
  int                        t;
  unsigned int               index;
  int                        qr, qg, qb;
  int                        mx, my;
  int                        weight;
  int                        lim;
  rng_t                      rng;

  *dst = NULL;

  if (src->format != pixelfmt_rgbx8888 && src->format != pixelfmt_rgba8888)
    return result_BAD_ARG;
  if (opts->method < 0 || opts->method >= dither__LIMIT ||
      opts->palette == NULL ||
      opts->ncolours < 1 || opts->ncolours > 256 ||
      opts->strength < 0 || opts->strength > 100)
    return result_BAD_ARG;

  k = &dither_kernels[opts->method];

  if (opts->ncolours <= 2)
  {
    log2bpp = 0;
    fmt     = pixelfmt_p1;
  }
  else if (opts->ncolours <= 4)
  {
    log2bpp = 1;
    fmt     = pixelfmt_p2;
  }
  else if (opts->ncolours <= 16)
  {
    log2bpp = 2;
    fmt     = pixelfmt_p4;
  }
  else
  {
    log2bpp = 3;
    fmt     = pixelfmt_p8;
  }

  pm = pixelmap_get(src->format, fmt, opts->palette, opts->ncolours);
  if (pm == NULL)
    return result_OOM;

  for (i = 0; i < 256; i++)
    palette[i] = (i < opts->ncolours) ? opts->palette[i] : colour_rgb(0, 0, 0);

  rowbytes = (((src->size.w << log2bpp) + 31) >> 5) << 2;

  base   = NULL;
  out    = NULL;
  err    = NULL;
  yl     = NULL;
  stride = src->size.w + 2 * DITHER_PAD;

  base = calloc((size_t) rowbytes * src->size.h, 1);
  out  = malloc(sizeof(*out));
  err  = calloc((size_t) stride * k->h * 3, sizeof(*err));
  if (opts->method == dither_YLILUOMA)
    yl = calloc(1, sizeof(*yl));
  if (base == NULL || out == NULL || err == NULL ||
      (opts->method == dither_YLILUOMA && yl == NULL))
  {
    rc = result_OOM;
    goto failure;
  }

  rc = bitmap_init(out, src->size, fmt, rowbytes, palette, base);
  if (rc != result_OK)
    goto failure;

  /* error is stored scaled by divisor * 100 so that strength and the kernel
   * weights need only one division, at read time */
  total = k->divisor * 100;

  /* in linear light channels run 0..colourspace_LINEAR_MAX through dec[],
   * and are re-encoded only for the nearest-colour lookup */
  dec = NULL;
  max = 255;
  if (opts->linear)
  {
    dec = colourspace_srgb_decode();
    max = colourspace_LINEAR_MAX;
  }
  lim = dither_limit(pm, max);

  /* an ordered method's bias per threshold: -step/2..step/2, symmetric */
  for (i = 0; i < k->levels; i++)
    bias[i] = ((2 * i - (k->levels - 1)) * (int) pm->step * opts->strength) /
              (2 * k->levels * 100);

  if (opts->method == dither_KNOLL || opts->method == dither_YLILUOMA)
    dither_luma_ranks(palette, opts->ncolours, rank);

  if (yl)
  {
    for (i = 0; i < opts->ncolours; i++)
    {
      colour_get_rgb(&palette[i], &pr, &pg, &pb);
      yl->pal[i][0] = dec ? dec[pr] / 257.0 : pr;
      yl->pal[i][1] = dec ? dec[pg] / 257.0 : pg;
      yl->pal[i][2] = dec ? dec[pb] / 257.0 : pb;
    }
  }

  /* Riemersma walks its own curve rather than rows */
  if (opts->method == dither_RIEMERSMA)
  {
    dither_riemersma(src, opts, pm, palette, dec, max, out);
    free(err);
    *dst = out;
    return result_OK;
  }

  rng_seed(&rng, 1); /* Zhou-Fang's noise, the same every call */

  srcrow = src->base;
  dstrow = base;
  for (y = 0; y < src->size.h; y++)
  {
    cur = y % k->h;
    dir = (opts->serpentine && (y & 1)) ? -1 : 1;
    px  = (const pixelfmt_rgbx8888_t *) srcrow;

    for (i = 0; i < src->size.w; i++)
    {
      x = (dir > 0) ? i : src->size.w - 1 - i;
      e = &err[(cur * stride + x + DITHER_PAD) * 3];

      r = PIXELFMT_Rxxx8888(px[x]);
      g = PIXELFMT_xGxx8888(px[x]);
      b = PIXELFMT_xxBx8888(px[x]);
      /* the threshold methods decode for themselves */
      if (dec && k->threshold == NULL)
      {
        r = dec[r];
        g = dec[g];
        b = dec[b];
      }

      if (opts->method == dither_KNOLL)
      {
        index = dither_knoll(pm, palette, rank, dec, max, r, g, b,
                             k->threshold(x, y), opts->strength);
      }
      else if (opts->method == dither_YLILUOMA)
      {
        index = dither_yliluoma(yl, opts->ncolours, rank, dec, r, g, b,
                                k->threshold(x, y), opts->strength);
      }
      else if (k->threshold)
      {
        t = bias[k->threshold(x, y)];
        r = dither_bias(pm, dec, r, t);
        g = dither_bias(pm, dec, g, t);
        b = dither_bias(pm, dec, b, t);
        index = dither_lookup(pm, r, g, b);
      }
      else
      {
        dither_apply3(&r, &g, &b, e, total, lim, max);
        if (opts->method == dither_ZHOU_FANG)
        {
          /* pr, pg, pb hold the nudged pixel until the palette colour
           * replaces them; the error is measured from the un-nudged r, g, b */
          t  = (int) (rng_xorshift32(&rng) & 255) - 128;
          pr = dither_zhou_fang(r, PIXELFMT_Rxxx8888(px[x]), t, pm, opts, max);
          pg = dither_zhou_fang(g, PIXELFMT_xGxx8888(px[x]), t, pm, opts, max);
          pb = dither_zhou_fang(b, PIXELFMT_xxBx8888(px[x]), t, pm, opts, max);
          index = dither_nearest(pm, dec, pr, pg, pb);
        }
        else
        {
          index = dither_nearest(pm, dec, r, g, b);
        }
      }
      dither_put(dstrow, x, log2bpp, index);

      if (opts->method == dither_NONE || k->threshold ||
          opts->strength == 0)
        continue;

      colour_get_rgb(&palette[index], &pr, &pg, &pb);
      if (dec)
      {
        pr = dec[pr];
        pg = dec[pg];
        pb = dec[pb];
      }
      qr = ((int) r - (int) pr) * opts->strength;
      qg = ((int) g - (int) pg) * opts->strength;
      qb = ((int) b - (int) pb) * opts->strength;

      if (opts->method == dither_OSTROMOUKHOV ||
          opts->method == dither_ZHOU_FANG)
      {
        dither_ostromoukhov(err, stride, cur, (cur + 1) % k->h, x, dir, &px[x],
                            qr, qg, qb, opts->method == dither_ZHOU_FANG);
        continue;
      }

      /* walking right to left mirrors the kernel horizontally */
      for (my = 0; my < k->h; my++)
      {
        for (mx = 0; mx < k->w; mx++)
        {
          weight = k->coeff[my][mx];
          if (weight == 0)
            continue;

          e = &err[(((cur + my) % k->h) * stride +
                    x + (mx - k->cx) * dir + DITHER_PAD) * 3];
          e[0] += qr * weight;
          e[1] += qg * weight;
          e[2] += qb * weight;
        }
      }
    }

    /* this row's slot becomes row y + k->h */
    memset(&err[cur * stride * 3], 0, (size_t) stride * 3 * sizeof(*err));

    srcrow += src->rowbytes;
    dstrow += rowbytes;
  }

  free(yl);
  free(err);
  *dst = out;

  return result_OK;

failure:
  free(yl);
  free(err);
  free(out);
  free(base);

  return rc;
}
