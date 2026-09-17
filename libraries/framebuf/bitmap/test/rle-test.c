/* framebuf/bitmap/test/rle-test.c -- RLE-compressed bitmap tests */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "io/path.h"

#include "framebuf/pixelfmt.h"

#include "framebuf/bitmap.h"
#include "framebuf/screen.h"

#include "../rle.h"

/* ----------------------------------------------------------------------- */

#define W 256
#define H 200

/* ----------------------------------------------------------------------- */

/* Encode one row then decode it unclipped, compare byte-for-byte. */
static int roundtrip_row(const void *row,
                         int         npix,
                         int         log2bpp,
                         int         has_alpha)
{
  uint8_t  enc[4 * W + W / 64 + 16];
  uint8_t  dec[4 * W];
  size_t   used;
  result_t rc;
  int      bpp;

  bpp = 1 << (log2bpp - 3);

  memset(dec, 0xAB, sizeof dec);

  rc = bitmap__rle_encode_row(row, npix, log2bpp, has_alpha,
                              enc, sizeof enc, &used);
  if (rc != result_OK)
  {
    fprintf(stderr, "roundtrip_row: encode rc=&%x\n", rc);
    return 0;
  }

  (void) bitmap__rle_decode_row(enc, enc + used, log2bpp, dec, 0, npix, 1);

  if (memcmp(dec, row, (size_t) npix * bpp) != 0)
  {
    fprintf(stderr, "roundtrip_row: mismatch (npix=%d log2bpp=%d)\n",
            npix, log2bpp);
    return 0;
  }
  return 1;
}

/* ----------------------------------------------------------------------- */

static int test_synthetic(void)
{
  uint32_t r32[W];
  uint8_t  r8[W];
  int      i;
  int      ok = 1;

  /* flat */
  for (i = 0; i < W; i++) { r32[i] = 0xFF204060u; r8[i] = 0x7F; }
  ok &= roundtrip_row(r32, W, 5, 1);
  ok &= roundtrip_row(r8,  W, 3, 0);

  /* all transparent (fully zero: exercises skip runs) */
  for (i = 0; i < W; i++) r32[i] = 0x00000000u;
  ok &= roundtrip_row(r32, W, 5, 1);

  /* non-zero but alpha-0: must survive as literal/repeat, not skip */
  for (i = 0; i < W; i++) r32[i] = 0x00123456u;
  ok &= roundtrip_row(r32, W, 5, 1);

  /* zero and non-zero-transparent interleaved with opaque */
  for (i = 0; i < W; i++)
    r32[i] = (i % 5 == 0) ? 0x00000000u
           : (i % 5 == 1) ? 0x00AABBCCu
           : 0xFF000000u | (uint32_t) i;
  ok &= roundtrip_row(r32, W, 5, 1);

  /* high-noise */
  for (i = 0; i < W; i++) { r32[i] = 0xFF000000u | (uint32_t) (i * 2654435761u);
                            r8[i] = (uint8_t) (i * 37 + 1); }
  ok &= roundtrip_row(r32, W, 5, 1);
  ok &= roundtrip_row(r8,  W, 3, 0);

  /* alternating pairs */
  for (i = 0; i < W; i++) { r32[i] = (i & 1) ? 0xFF111111u : 0xFFEEEEEEu;
                            r8[i] = (i & 1) ? 0x10 : 0xE0; }
  ok &= roundtrip_row(r32, W, 5, 1);
  ok &= roundtrip_row(r8,  W, 3, 0);

  /* single pixel */
  r32[0] = 0xFF010203u; r8[0] = 0x42;
  ok &= roundtrip_row(r32, 1, 5, 1);
  ok &= roundtrip_row(r8,  1, 3, 0);

  /* run lengths straddling the short/long boundary */
  for (i = 0; i < W; i++) { r32[i] = 0xFF777777u; r8[i] = 0x55; }
  ok &= roundtrip_row(r32, 128, 5, 1);   /* exactly LITERAL_LEN_MAX repeats */
  ok &= roundtrip_row(r32, 129, 5, 1);
  ok &= roundtrip_row(r8,  64,  3, 0);   /* REPEAT_LEN_MAX */
  ok &= roundtrip_row(r8,  65,  3, 0);

  /* literal run straddling the boundary: no two adjacent pixels equal */
  for (i = 0; i < W; i++) r8[i] = (uint8_t) ((i * 131 + 7) | 1);
  for (i = 1; i < W; i++) if (r8[i] == r8[i - 1]) r8[i] ^= 2;
  ok &= roundtrip_row(r8, 128, 3, 0);
  ok &= roundtrip_row(r8, 129, 3, 0);

  return ok;
}

/* ----------------------------------------------------------------------- */

/* Reference blit matching the RLE decoder: copy every pixel except a wholly
 * zero one (which the encoder turns into a skip run). */
static void ref_blit(uint32_t       *dst,
                     int             dst_rowwords,
                     const uint32_t *src,
                     int             src_rowwords,
                     int             w,
                     int             h)
{
  int x, y;

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
    {
      uint32_t px = src[y * src_rowwords + x];
      if (px != 0)
        dst[y * dst_rowwords + x] = px;
    }
}

static int test_blit(const char *resources)
{
  result_t    rc;
  bitmap_t    bm;
  bitmap_t    cbm;
  uint32_t   *rawpix;
  const char *filename;
  int         w, h;
  int         rowwords;
  uint32_t   *scr_ref;
  uint32_t   *scr_rle;
  screen_t    s;
  box_t       clip;
  size_t      npix;
  int         ok = 1;

  filename = pathf("%s/resources/composite/A.png", resources);
  rc = bitmap_load_png(&bm, filename);
  if (rc)
  {
    fprintf(stderr, "test_blit: load rc=&%x\n", rc);
    return 0;
  }
  /* bitmap_load_png yields rgba8888 (R,G,B,A byte order). */

  w    = bm.size.w;
  h    = bm.size.h;
  npix = (size_t) w * h;

  /* Keep an untouched copy of the raw pixels. */
  rawpix = malloc(npix * 4);
  memcpy(rawpix, bm.base, npix * 4);

  /* --- round trip --- */
  cbm = bm;
  cbm.base = malloc(npix * 4);
  memcpy(cbm.base, bm.base, npix * 4);
  bitmap_init(&cbm, bm.size, bm.format, bm.rowbytes, NULL, cbm.base);

  rc = bitmap_compress(&cbm);
  if (rc) { fprintf(stderr, "compress rc=&%x\n", rc); ok = 0; goto done; }

  if (!bitmap_is_compressed(&cbm)
      || pixelfmt_base(cbm.format) != bm.format)
  {
    fprintf(stderr, "compress: flag/format wrong\n");
    ok = 0; goto done;
  }
  {
    uint32_t data_len = bitmap__rle_get32((const uint8_t *) cbm.base + 4);
    fprintf(stdout, "rle: A.png %zu raw -> %u encoded (%.2f)\n",
            npix * 4, (unsigned) data_len, (double) data_len / (double) (npix * 4));
    if ((size_t) data_len >= npix * 4)
      fprintf(stderr, "rle: warning: no size win on A.png\n");
  }

  rc = bitmap_decompress(&cbm);
  if (rc) { fprintf(stderr, "decompress rc=&%x\n", rc); ok = 0; goto done; }

  if (cbm.rowbytes != bm.rowbytes || memcmp(cbm.base, rawpix, npix * 4) != 0)
  {
    fprintf(stderr, "round trip: pixels or rowbytes differ\n");
    ok = 0; goto done;
  }

  /* --- blit equivalence, unclipped then clipped --- */
  rowwords = w;
  scr_ref  = malloc(npix * 4);
  scr_rle  = malloc(npix * 4);

  /* recompress for the blit */
  rc = bitmap_compress(&cbm);
  if (rc) { fprintf(stderr, "compress2 rc=&%x\n", rc); ok = 0; goto done2; }

  /* unclipped */
  memset(scr_ref, 0, npix * 4);
  memset(scr_rle, 0, npix * 4);
  ref_blit(scr_ref, rowwords, rawpix, w, w, h);

  screen_init(&s, bm.size, pixelfmt_rgba8888, w * 4, NULL, scr_rle);
  rc = screen_copy_bitmap(&s, 0, 0, &cbm);
  if (rc) { fprintf(stderr, "blit rc=&%x\n", rc); ok = 0; goto done2; }
  if (memcmp(scr_ref, scr_rle, npix * 4) != 0)
  {
    fprintf(stderr, "blit: unclipped mismatch\n");
    ok = 0; goto done2;
  }

  /* clipped: crop top and left */
  memset(scr_ref, 0, npix * 4);
  memset(scr_rle, 0, npix * 4);
  {
    int cx = w / 3, cy = h / 4;
    int y;

    for (y = cy; y < h; y++)
      ref_blit(scr_ref + y * rowwords + cx, rowwords,
               rawpix + y * w + cx, w, w - cx, 1);

    clip.x0 = cx; clip.y0 = cy; clip.x1 = w; clip.y1 = h;
    s.clip = clip;
    rc = screen_copy_bitmap(&s, 0, 0, &cbm);
    if (rc) { fprintf(stderr, "blit clipped rc=&%x\n", rc); ok = 0; goto done2; }
    if (memcmp(scr_ref, scr_rle, npix * 4) != 0)
    {
      fprintf(stderr, "blit: clipped mismatch\n");
      ok = 0; goto done2;
    }
  }

  /* --- 4bpp screen: RLE path must match the raw p4 blit --- */
  {
    colour_t pal[16];
    uint8_t *p4_ref;
    uint8_t *p4_rle;
    int      p4_rowbytes;
    int      i;

    /* A spread of greys is enough for colour_to_pixel's nearest match to
     * exercise more than one entry. */
    for (i = 0; i < 16; i++)
    {
      unsigned int g = (unsigned int) (i * 17);
      pal[i].primary = 0xFF000000u | (g << 16) | (g << 8) | g;
    }

    p4_rowbytes = (w + 1) / 2;
    p4_ref = malloc((size_t) p4_rowbytes * h);
    p4_rle = malloc((size_t) p4_rowbytes * h);
    memset(p4_ref, 0, (size_t) p4_rowbytes * h);
    memset(p4_rle, 0, (size_t) p4_rowbytes * h);

    rc = bitmap_decompress(&cbm);
    if (rc) { fprintf(stderr, "p4 decompress rc=&%x\n", rc); ok = 0; }

    screen_init(&s, bm.size, pixelfmt_p4, p4_rowbytes, pal, p4_ref);
    rc = screen_copy_bitmap(&s, 0, 0, &cbm);
    if (rc) { fprintf(stderr, "p4 raw blit rc=&%x\n", rc); ok = 0; }

    rc = bitmap_compress(&cbm);
    if (rc) { fprintf(stderr, "p4 compress rc=&%x\n", rc); ok = 0; }

    screen_init(&s, bm.size, pixelfmt_p4, p4_rowbytes, pal, p4_rle);
    rc = screen_copy_bitmap(&s, 0, 0, &cbm);
    if (rc) { fprintf(stderr, "p4 rle blit rc=&%x\n", rc); ok = 0; }

    if (ok && memcmp(p4_ref, p4_rle, (size_t) p4_rowbytes * h) != 0)
    {
      fprintf(stderr, "blit: 4bpp RLE vs raw mismatch\n");
      ok = 0;
    }

    free(p4_ref);
    free(p4_rle);
  }

done2:
  free(scr_ref);
  free(scr_rle);
done:
  free(cbm.base);
  free(rawpix);
  free(bm.base);
  return ok;
}

/* ----------------------------------------------------------------------- */

static int test_y8(void)
{
  bitmap_t bm;
  uint8_t *pix;
  uint8_t *copy;
  int      i;
  result_t rc;
  int      ok = 1;
  size_t   n = (size_t) W * H;

  pix  = malloc(n);
  copy = malloc(n);
  for (i = 0; i < (int) n; i++)
    pix[i] = (uint8_t) ((i / 17) ^ (i * 13));
  memcpy(copy, pix, n);

  bitmap_init(&bm, SIZE2D(W, H), pixelfmt_y8, W, NULL, pix);

  rc = bitmap_compress(&bm);
  if (rc) { fprintf(stderr, "y8 compress rc=&%x\n", rc); ok = 0; goto done; }
  rc = bitmap_decompress(&bm);
  if (rc) { fprintf(stderr, "y8 decompress rc=&%x\n", rc); ok = 0; goto done; }

  if (bm.rowbytes != W || memcmp(bm.base, copy, n) != 0)
  {
    fprintf(stderr, "y8: round trip differs\n");
    ok = 0;
  }

  /* Pathological expansion: singleton, pair, singleton, pair, ... forces a
   * literal-of-1 then a repeat-of-2 per three pixels, ~1.33*w bytes/row. The
   * worst-case allocation must cover it rather than returning
   * result_BUFFER_OVERFLOW on this legal input. (bm.base was freed and
   * replaced by the round trip above, so allocate afresh.) */
  {
    uint8_t *p2 = malloc(n);

    for (i = 0; i < (int) n; i++)
      p2[i] = (uint8_t) (i % 3 == 0 ? (i / 3 + 1) : 0x80);
    memcpy(copy, p2, n);
    bitmap_init(&bm, SIZE2D(W, H), pixelfmt_y8, W, NULL, p2);
    rc = bitmap_compress(&bm);
    if (rc) { fprintf(stderr, "y8 pathological compress rc=&%x\n", rc); ok = 0; goto done; }
    rc = bitmap_decompress(&bm);
    if (rc) { fprintf(stderr, "y8 pathological decompress rc=&%x\n", rc); ok = 0; goto done; }
    if (memcmp(bm.base, copy, n) != 0)
    {
      fprintf(stderr, "y8 pathological: round trip differs\n");
      ok = 0;
    }
  }

done:
  free(bm.base);
  free(copy);
  return ok;
}

/* ----------------------------------------------------------------------- */

/* Synthesise a p8 bitmap with a known palette, save it as a palette-type PNG,
 * reload it and check the format, palette (RGB + tRNS alpha) and pixel indices
 * survive the round trip. Then bitmap_convert it to bgrx8888 and spot-check a
 * pixel against its palette entry. */
static int test_p8_png(void)
{
#ifdef DPTLIB_IMAGES_READ_ONLY
  return 1; /* ponytail: save disabled at build time, nothing to round trip */
#else
#define P8W 24
#define P8H 8
  bitmap_t      bm;
  bitmap_t      rt;
  bitmap_t     *deep = NULL;
  colour_t      pal[256];
  colour_t      rtpal[256];
  unsigned char pix[P8W * P8H];
  const char   *fn;
  result_t      rc;
  int           i;
  int           ok = 1;

  fn = pathf("rle-test-p8.png");

  /* palette: greyscale ramp, plus a few non-opaque entries to exercise tRNS */
  for (i = 0; i < 256; i++)
    pal[i] = colour_rgba((unsigned char) i, (unsigned char) i,
                         (unsigned char) i, 0xFF);
  pal[0]   = colour_rgba(0, 0, 0, 0x00);
  pal[1]   = colour_rgba(255, 0, 0, 0x80);
  pal[200] = colour_rgba(10, 20, 30, 0xFF);

  for (i = 0; i < P8W * P8H; i++)
    pix[i] = (unsigned char) ((i * 7 + i / P8W) & 0xFF);

  bitmap_init(&bm, SIZE2D(P8W, P8H), pixelfmt_p8, P8W, pal, pix);

  rc = bitmap_save_png(&bm, fn);
  if (rc) { fprintf(stderr, "p8 png: save rc=&%x\n", rc); return 0; }

  rc = bitmap_load_png(&rt, fn);
  remove(fn);
  if (rc) { fprintf(stderr, "p8 png: load rc=&%x\n", rc); return 0; }

  if (rt.format != pixelfmt_p8)
  {
    fprintf(stderr, "p8 png: reloaded format &%x not p8\n", rt.format);
    ok = 0; goto done;
  }
  if (rt.size.w != P8W || rt.size.h != P8H)
  {
    fprintf(stderr, "p8 png: reloaded size wrong\n");
    ok = 0; goto done;
  }
  if (rt.palette == NULL)
  {
    fprintf(stderr, "p8 png: reloaded bitmap has no palette\n");
    ok = 0; goto done;
  }

  memcpy(rtpal, rt.palette, sizeof rtpal);
  for (i = 0; i < 256; i++)
  {
    if (rtpal[i].primary != pal[i].primary)
    {
      fprintf(stderr, "p8 png: palette entry %d &%08x != &%08x\n",
              i, rtpal[i].primary, pal[i].primary);
      ok = 0; goto done;
    }
  }

  for (i = 0; i < P8W * P8H; i++)
  {
    if (((const unsigned char *) rt.base)[i] != pix[i])
    {
      fprintf(stderr, "p8 png: pixel %d %u != %u\n",
              i, ((const unsigned char *) rt.base)[i], pix[i]);
      ok = 0; goto done;
    }
  }

  rc = bitmap_convert(&rt, pixelfmt_bgrx8888, &deep);
  if (rc) { fprintf(stderr, "p8 png: convert rc=&%x\n", rc); ok = 0; goto done; }

  {
    unsigned int        idx = pix[0];
    pixelfmt_bgrx8888_t  got = ((const pixelfmt_bgrx8888_t *) deep->base)[0];
    pixelfmt_rgba8888_t want = pal[idx].primary;

    if (PIXELFMT_xxRx8888(got) != PIXELFMT_Rxxx8888(want)
     || PIXELFMT_xGxx8888(got) != PIXELFMT_xGxx8888(want)
     || PIXELFMT_Bxxx8888(got) != PIXELFMT_xxBx8888(want))
    {
      fprintf(stderr, "p8 png: converted pixel 0 &%08x != palette &%08x\n",
              got, want);
      ok = 0;
    }
  }

done:
  if (deep != NULL) { free(deep->base); free(deep); }
  free(rt.base);
  return ok;
#undef P8W
#undef P8H
#endif
}

/* ----------------------------------------------------------------------- */

result_t bitmap_rle_test(const char *resources)
{
  int ok = 1;

  ok &= test_synthetic();
  ok &= test_y8();
  ok &= test_p8_png();
  ok &= test_blit(resources);

  return ok ? result_TEST_PASSED : result_TEST_FAILED;
}
