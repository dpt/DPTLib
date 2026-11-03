/* bench/main.c -- time and checksum the pixel inner loops */

/* Each line gives milliseconds per call and an FNV-1a checksum of the
 * output, so an optimisation can be timed and shown to be bit-exact by
 * comparing runs before and after. Build in Release. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base/result.h"
#include "base/utils.h"

#include "framebuf/bitmap.h"
#include "framebuf/composite.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "framebuf/span.h"
#include "framebuf/span-registry.h"

/* ----------------------------------------------------------------------- */

#define W 1920
#define H 1080
#define N ((size_t) W * H)

static unsigned int bench_rng_state = 0x12345678u;

static unsigned int bench_rng(void)
{
  bench_rng_state ^= bench_rng_state << 13;
  bench_rng_state ^= bench_rng_state >> 17;
  bench_rng_state ^= bench_rng_state << 5;
  return bench_rng_state;
}

static unsigned int bench_hash(const void *data, size_t n)
{
  const unsigned char *p;
  unsigned int         h;
  size_t               i;

  p = data;
  h = 2166136261u;
  for (i = 0; i < n; i++)
  {
    h ^= p[i];
    h *= 16777619u;
  }

  return h;
}

static double bench_ms(clock_t start, int reps)
{
  return (double) (clock() - start) * 1000.0 / CLOCKS_PER_SEC / reps;
}

/* ----------------------------------------------------------------------- */

static void bench_spans(const char          *tag,
                        const unsigned int  *src1,
                        const unsigned int  *src2,
                        const unsigned char *alphas,
                        unsigned int        *dst)
{
  static const struct
  {
    pixelfmt_t  fmt;
    const char *name;
    size_t      bpp; /* bytes per pixel */
  }
  fmts[] =
  {
    { pixelfmt_bgrx8888, "bgrx8888", 4 },
    { pixelfmt_bgra8888, "bgra8888", 4 },
    { pixelfmt_rgb565,   "rgb565",   2 },
    { pixelfmt_rgbx5551, "rgbx5551", 2 },
  };

  const span_t *span;
  clock_t       start;
  int           i;
  int           a;
  int           r;
  double        ms;
  unsigned int  h;

  for (i = 0; i < (int) NELEMS(fmts); i++)
  {
    span = spanregistry_get(fmts[i].fmt);

    start = clock();
    for (r = 0; r < 10; r++)
      span->blendarray(dst, src1, src2, (int) N, alphas);
    printf("blendarray %-8s %-4s %8.2f ms  %08x\n", fmts[i].name, tag,
           bench_ms(start, 10), bench_hash(dst, N * fmts[i].bpp));

    /* in place, as the screen blits do */
    memcpy(dst, src1, N * fmts[i].bpp);
    start = clock();
    for (r = 0; r < 10; r++)
      span->blendarray(dst, dst, src2, (int) N, alphas);
    printf("blendarray %-8s %-4s %8.2f ms  %08x  in place\n", fmts[i].name,
           tag, bench_ms(start, 10), bench_hash(dst, N * fmts[i].bpp));

    /* timed over the whole buffer at 16 alphas from 0 to 255 */
    start = clock();
    for (a = 0; a < 256; a += 17)
      span->blendconst(dst, src1, src2, (int) N, a, NULL);
    ms = bench_ms(start, 16);

    /* checksummed at every alpha, a row each */
    h = 0;
    for (a = 0; a < 256; a++)
    {
      span->blendconst(dst, src1, src2, W * 4, a, NULL);
      h ^= bench_hash(dst, W * 4 * fmts[i].bpp) + (unsigned int) a;
    }
    printf("blendconst %-8s %-4s %8.2f ms  %08x\n", fmts[i].name, tag, ms, h);

    memcpy(dst, src1, N * fmts[i].bpp);
    start = clock();
    for (a = 0; a < 256; a += 17)
      span->blendconst(dst, dst, src2, (int) N, a, NULL);
    printf("blendconst %-8s %-4s %8.2f ms  %08x  in place\n", fmts[i].name,
           tag, bench_ms(start, 16), bench_hash(dst, N * fmts[i].bpp));
  }
}

static void bench_composite(bitmap_t *src, bitmap_t *dst, const void *orig)
{
  static const char *names[] =
  {
    "clear", "src", "dst", "src_over", "dst_over", "src_in", "dst_in",
    "src_out", "dst_out", "src_atop", "dst_atop", "xor"
  };

  clock_t      start;
  int          rule;
  int          r;
  unsigned int h;

  for (rule = 0; rule < composite_RULE__LIMIT; rule++)
  {
    h     = 0;
    start = clock();
    for (r = 0; r < 4; r++)
    {
      memcpy(dst->base, orig, N * 4);
      composite((composite_rule_t) rule, src, dst);
    }
    h = bench_hash(dst->base, N * 4);
    printf("composite  %-8s %8.2f ms  %08x\n", names[rule],
           bench_ms(start, 4), h);
  }
}

/* blit a W x H bitmap of src's pixels in each source format onto a screen of
 * each deep format */
static void bench_blit(unsigned int *src, unsigned int *dst)
{
  static const struct
  {
    pixelfmt_t  fmt;
    const char *name;
    size_t      bpp; /* bytes per pixel */
  }
  scrs[] =
  {
    { pixelfmt_bgrx8888, "bgrx8888", 4 },
    { pixelfmt_rgb565,   "rgb565",   2 },
  },
  srcs[] =
  {
    { pixelfmt_bgra8888, "bgra8888", 4 },
    { pixelfmt_bgrx8888, "bgrx8888", 4 },
    { pixelfmt_rgba8888, "rgba8888", 4 },
  };

  bitmap_t bm;
  screen_t scr;
  clock_t  start;
  int      i, j, r;

  for (i = 0; i < (int) NELEMS(scrs); i++)
    for (j = 0; j < (int) NELEMS(srcs); j++)
    {
      bitmap_init(&bm, SIZE2D(W, H), srcs[j].fmt, W * 4, NULL, src);
      screen_init(&scr, SIZE2D(W, H), scrs[i].fmt, W * (int) scrs[i].bpp,
                  NULL, dst);

      memset(dst, 0x80, N * scrs[i].bpp);
      start = clock();
      for (r = 0; r < 4; r++)
        (void) screen_copy_bitmap(&scr, 0, 0, &bm);
      printf("blit %-8s to %-8s %8.2f ms  %08x\n", srcs[j].name,
             scrs[i].name, bench_ms(start, 4),
             bench_hash(dst, N * scrs[i].bpp));
    }
}

static void bench_resize(const bitmap_t *src)
{
  static const struct
  {
    int         w, h;
    const char *name;
  }
  sizes[] =
  {
    { W * 2,  H * 2,  "up2"    },
    { W / 2,  H / 2,  "down2"  },
    { 1000,   777,    "odd"    },
  };

  static const struct
  {
    bitmap_resample_t method;
    int               linear;
    const char       *name;
  }
  methods[] =
  {
    { bitmap_resample_NEAREST,  0, "nearest" },
    { bitmap_resample_BILINEAR, 0, "bilin"   },
    { bitmap_resample_BILINEAR, 1, "bilin-l" },
  };

  result_t  rc;
  bitmap_t *out;
  size2d_t  size;
  clock_t   start;
  int       i;
  int       m;

  for (m = 0; m < (int) NELEMS(methods); m++)
    for (i = 0; i < (int) NELEMS(sizes); i++)
    {
      size.w = sizes[i].w;
      size.h = sizes[i].h;
      start  = clock();
      rc     = bitmap_resize(src, size, methods[m].method, methods[m].linear,
                             &out);
      if (rc != result_OK)
      {
        printf("resize failed\n");
        return;
      }
      printf("resize %-7s %-5s %8.2f ms  %08x\n", methods[m].name,
             sizes[i].name, bench_ms(start, 1),
             bench_hash(out->base, (size_t) out->rowbytes * size.h));
      free(out->base);
      free(out);
    }
}

/* ----------------------------------------------------------------------- */

int main(void)
{
  unsigned int  *src1;
  unsigned int  *src2;
  unsigned char *alphas;
  unsigned int  *dst;
  unsigned int  *opaque;
  int            run;
  int            solid;
  size_t         i;
  bitmap_t       bsrc;
  bitmap_t       bdst;
  size2d_t       size;

  src1   = malloc(N * 4);
  src2   = malloc(N * 4);
  alphas = malloc(N);
  dst    = malloc(N * 4);
  opaque = malloc(N * 4);
  if (src1 == NULL || src2 == NULL || alphas == NULL || dst == NULL ||
      opaque == NULL)
    return EXIT_FAILURE;

  /* like a coverage mask: runs of 0 and 255 alternating, each followed by a
   * soft edge of two arbitrary values */
  run   = 0;
  solid = 0;
  for (i = 0; i < N; i++)
  {
    src1[i] = bench_rng();
    src2[i] = bench_rng();
    if (run == 0)
    {
      run   = 3 + (int) (bench_rng() % 32);
      solid = 255 - solid;
    }
    alphas[i] = (unsigned char) (run <= 2 ? bench_rng() : (unsigned) solid);
    run--;
  }
  /* likewise for the pixels' own alpha bytes */
  for (i = 0; i < N; i += 7)
  {
    src1[i] |= 0xFF000000u;
    src2[i + 1 < N ? i + 1 : i] &= 0x00FFFFFFu;
  }

  bench_spans("mix", src1, src2, alphas, dst);

  /* again over an opaque destination, as a screen is */
  for (i = 0; i < N; i++)
    opaque[i] = src1[i] | 0xFF000000u;
  bench_spans("opq", opaque, src2, alphas, dst);

  size.w = W;
  size.h = H;
  bitmap_init(&bsrc, size, pixelfmt_bgra8888, W * 4, NULL, src2);
  bitmap_init(&bdst, size, pixelfmt_bgra8888, W * 4, NULL, dst);
  bench_composite(&bsrc, &bdst, src1);

  bench_resize(&bsrc);

  bench_blit(src1, dst);

  free(opaque);
  free(dst);
  free(alphas);
  free(src2);
  free(src1);

  return EXIT_SUCCESS;
}
