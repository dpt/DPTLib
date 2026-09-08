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
static int roundtrip_row(const void *row, int npix, int log2bpp, int has_alpha)
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

  (void) bitmap__rle_decode_row(enc, log2bpp, dec, 0, npix, 1);

  (void) used;

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
  result_t  rc;
  bitmap_t  bm;
  bitmap_t  cbm;
  uint32_t *rawpix;
  const char *filename;
  int       w, h;
  int       rowwords;
  uint32_t *scr_ref;
  uint32_t *scr_rle;
  screen_t  s;
  box_t     clip;
  size_t    npix;
  int       ok = 1;

  filename = path_join_filename(resources, 3, "resources", "composite", "A.png");
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

done:
  free(bm.base);
  free(copy);
  return ok;
}

/* ----------------------------------------------------------------------- */

result_t bitmap_rle_test(const char *resources)
{
  int ok = 1;

  ok &= test_synthetic();
  ok &= test_y8();
  ok &= test_blit(resources);

  return ok ? result_TEST_PASSED : result_TEST_FAILED;
}
