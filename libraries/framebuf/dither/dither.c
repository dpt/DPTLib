/* framebuf/dither/dither.c -- error-diffusion and ordered dithering to a
 * palette */

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
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

/* coeff[0] is the current row; entries at or left of cx on it are zero. An
 * ordered method has no coefficients and a threshold map instead. */
typedef struct dither_kernel
{
  int           w, h;
  int           cx;      /* column of the current pixel */
  int           divisor;
  unsigned char coeff[DITHER_MAX_H][DITHER_MAX_W];
  int         (*threshold)(int x, int y); /* 0..63, or NULL to diffuse */
}
dither_kernel_t;

static const dither_kernel_t dither_kernels[dither__LIMIT] =
{
  /* none */
  { 1, 1, 0, 1, { { 0 } }, NULL },
  /* Floyd-Steinberg */
  { 3, 2, 1, 16, { { 0, 0, 7 },
                   { 3, 5, 1 } }, NULL },
  /* Jarvis, Judice and Ninke */
  { 5, 3, 2, 48, { { 0, 0, 0, 7, 5 },
                   { 3, 5, 7, 5, 3 },
                   { 1, 3, 5, 3, 1 } }, NULL },
  /* Stucki */
  { 5, 3, 2, 42, { { 0, 0, 0, 8, 4 },
                   { 2, 4, 8, 4, 2 },
                   { 1, 2, 4, 2, 1 } }, NULL },
  /* Atkinson */
  { 4, 3, 1, 8,  { { 0, 0, 1, 1 },
                   { 1, 1, 1, 0 },
                   { 0, 1, 0, 0 } }, NULL },
  /* Burkes */
  { 5, 2, 2, 32, { { 0, 0, 0, 8, 4 },
                   { 2, 4, 8, 4, 2 } }, NULL },
  /* Sierra */
  { 5, 3, 2, 32, { { 0, 0, 0, 5, 3 },
                   { 2, 4, 5, 4, 2 },
                   { 0, 2, 3, 2, 0 } }, NULL },
  /* Sierra two-row */
  { 5, 2, 2, 16, { { 0, 0, 0, 4, 3 },
                   { 1, 2, 3, 2, 1 } }, NULL },
  /* Sierra lite */
  { 3, 2, 1, 4,  { { 0, 0, 2 },
                   { 1, 1, 0 } }, NULL },
  /* Bayer */
  { 1, 1, 0, 1,  { { 0 } }, pattern_bayer_threshold },
  /* blue noise */
  { 1, 1, 0, 1,  { { 0 } }, pattern_blue_noise_threshold },
  /* Knoll: candidates chosen by Bayer threshold */
  { 1, 1, 0, 1,  { { 0 } }, pattern_bayer_threshold },
  /* Riemersma: no kernel; error travels along a Hilbert curve */
  { 1, 1, 0, 1,  { { 0 } }, NULL },
  /* Ostromoukhov: coefficients vary per pixel; only the shape is fixed */
  { 3, 2, 1, OSTRO_DIV, { { 0 } }, NULL },
  /* Zhou-Fang: Ostromoukhov's shape */
  { 3, 2, 1, OSTRO_DIV, { { 0 } }, NULL },
  /* Fan */
  { 4, 2, 2, 16, { { 0, 0, 0, 7 },
                   { 1, 3, 5, 0 } }, NULL },
  /* Shiau-Fan, four cells */
  { 4, 2, 2, 8,  { { 0, 0, 0, 4 },
                   { 1, 1, 2, 0 } }, NULL },
  /* Shiau-Fan, five cells */
  { 5, 2, 3, 16, { { 0, 0, 0, 0, 8 },
                   { 1, 1, 2, 4, 0 } }, NULL },
};

/* ----- sRGB <-> linear light ----- */

#define LINEAR_MAX  65535
#define ENCODE_BITS 12 /* linear-to-sRGB table has 2^ENCODE_BITS entries */

/* ponytail: same tables as bitmap/resize.c; share them if a third user
 * appears */
static struct
{
  unsigned short decode[256];
  unsigned char  encode[1 << ENCODE_BITS];
  int            ready;
}
dither_tables;

static void dither_make_tables(void)
{
  int    i;
  double c;

  if (dither_tables.ready)
    return;

  for (i = 0; i < 256; i++)
  {
    c = i / 255.0;
    c = c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
    dither_tables.decode[i] = (unsigned short) (c * LINEAR_MAX + 0.5);
  }

  for (i = 0; i < (1 << ENCODE_BITS); i++)
  {
    c = i / (double) ((1 << ENCODE_BITS) - 1);
    c = c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1 / 2.4) - 0.055;
    dither_tables.encode[i] = (unsigned char) (c * 255 + 0.5);
  }

  dither_tables.ready = 1;
}

/* ----------------------------------------------------------------------- */

/* add accumulated error `e`, scaled by `total`, to channel value `c`,
 * clamping to 0..max */
static unsigned int dither_apply(unsigned int c, int e, int total, int max)
{
  int v;

  v = (e >= 0) ? (e + total / 2) / total : -((-e + total / 2) / total);

  return (unsigned int) CLAMP((int) c + v, 0, max);
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
 * candidate at threshold t (0..63) in luma order. */
static unsigned int dither_knoll(const pixelmap_t    *pm,
                                 const colour_t      *palette,
                                 const unsigned char *rank,
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

  /* distinct candidates in idx[0..n), each seen count[] times */
  n  = 0;
  er = eg = eb = 0;
  for (i = 0; i < KNOLL_N; i++)
  {
    c = dither_lookup(pm,
                      dither_apply(r, er * strength, 100, 255),
                      dither_apply(g, eg * strength, 100, 255),
                      dither_apply(b, eb * strength, 100, 255));
    colour_get_rgb(&palette[c], &pr, &pg, &pb);
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

/* Nearest palette index for channels r, g, b: sRGB, or linear when dec is
 * set. */
static unsigned int dither_nearest(const pixelmap_t     *pm,
                                   const unsigned short *dec,
                                   unsigned int          r,
                                   unsigned int          g,
                                   unsigned int          b)
{
  if (dec == NULL)
    return dither_lookup(pm, r, g, b);

  return dither_lookup(pm,
                       dither_tables.encode[r >> (16 - ENCODE_BITS)],
                       dither_tables.encode[g >> (16 - ENCODE_BITS)],
                       dither_tables.encode[b >> (16 - ENCODE_BITS)]);
}

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
 * newest weighted most. The weights sum to one, so the mean is kept, and the
 * curve has no preferred direction, so there are no directional artefacts.
 *
 * ponytail: the curve covers the enclosing power-of-two square and skips
 * the cells outside the image, wasting up to 4x the steps on long thin
 * images; use a generalised (gilbert) curve if that matters. */
static void dither_riemersma(const bitmap_t         *src,
                             const dither_options_t *opts,
                             const pixelmap_t       *pm,
                             const colour_t         *palette,
                             const unsigned short   *dec,
                             int                     max,
                             bitmap_t               *out)
{
  int                        weight[RIEMERSMA_Q];
  int                        wsum;
  int                        queue[RIEMERSMA_Q][3];
  int                        head;
  int                        n;
  int                        d;
  int                        x, y;
  int                        a;
  const pixelfmt_rgbx8888_t *px;
  unsigned int               c[3];
  int                        sum[3];
  int                        ch;
  unsigned int               index;
  unsigned int               p[3];
  int                        log2bpp;

  /* weight[a] for an error a steps old: RATIO^(-a / (Q - 1)), scaled */
  wsum = 0;
  for (a = 0; a < RIEMERSMA_Q; a++)
  {
    weight[a] = (int) (1024.0 * pow(RIEMERSMA_RATIO,
                                    -a / (double) (RIEMERSMA_Q - 1)) + 0.5);
    wsum += weight[a];
  }

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
     * weights 1024, so 16 of them fit an int */
    for (ch = 0; ch < 3; ch++)
    {
      if (dec)
        c[ch] = dec[c[ch]];

      sum[ch] = 0;
      for (a = 0; a < RIEMERSMA_Q; a++)
        sum[ch] += queue[(head - 1 - a + RIEMERSMA_Q) % RIEMERSMA_Q][ch] *
                   weight[a];

      c[ch] = dither_apply(c[ch], sum[ch] / wsum * opts->strength, 100, max);
    }

    index = dither_nearest(pm, dec, c[0], c[1], c[2]);
    dither_put((unsigned char *) out->base + y * out->rowbytes, x, log2bpp,
               index);

    colour_get_rgb(&palette[index], &p[0], &p[1], &p[2]);
    for (ch = 0; ch < 3; ch++)
      queue[head][ch] = (int) c[ch] - (int) (dec ? dec[p[ch]] : p[ch]);
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
  const unsigned short      *dec;
  int                        max;
  int                        bias[64];
  unsigned char              rank[256];
  int                        i;
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
  unsigned int               pr, pg, pb;
  int                        qr, qg, qb;
  int                        mx, my;
  int                        weight;
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
  stride = src->size.w + 2 * DITHER_PAD;

  base = calloc((size_t) rowbytes * src->size.h, 1);
  out  = malloc(sizeof(*out));
  err  = calloc((size_t) stride * k->h * 3, sizeof(*err));
  if (base == NULL || out == NULL || err == NULL)
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

  /* in linear light channels run 0..LINEAR_MAX through dec[], and are
   * re-encoded only for the nearest-colour lookup */
  dec = NULL;
  max = 255;
  if (opts->linear && k->threshold == NULL)
  {
    dither_make_tables();
    dec = dither_tables.decode;
    max = LINEAR_MAX;
  }

  /* an ordered method's bias per threshold: -step/2..step/2, symmetric */
  for (i = 0; i < 64; i++)
    bias[i] = ((2 * i - 63) * (int) pm->step * opts->strength) / (128 * 100);

  if (opts->method == dither_KNOLL)
    dither_luma_ranks(palette, opts->ncolours, rank);

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
      if (dec)
      {
        r = dec[r];
        g = dec[g];
        b = dec[b];
      }

      if (opts->method == dither_KNOLL)
      {
        index = dither_knoll(pm, palette, rank, r, g, b, k->threshold(x, y),
                             opts->strength);
      }
      else if (k->threshold)
      {
        t = bias[k->threshold(x, y)];
        r = (unsigned int) CLAMP((int) r + t, 0, 255);
        g = (unsigned int) CLAMP((int) g + t, 0, 255);
        b = (unsigned int) CLAMP((int) b + t, 0, 255);
        index = dither_lookup(pm, r, g, b);
      }
      else
      {
        r = dither_apply(r, e[0], total, max);
        g = dither_apply(g, e[1], total, max);
        b = dither_apply(b, e[2], total, max);
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

  free(err);
  *dst = out;

  return result_OK;

failure:
  free(err);
  free(out);
  free(base);

  return rc;
}
