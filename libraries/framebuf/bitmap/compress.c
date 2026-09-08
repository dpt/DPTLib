/* framebuf/bitmap/compress.c -- RLE compress/decompress a read-only bitmap */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "utils/barith.h"

#include "framebuf/pixelfmt.h"

#include "framebuf/bitmap.h"

#include "rle.h"

/* ----------------------------------------------------------------------- */

void bitmap__rle_put32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t) (v      );
  p[1] = (uint8_t) (v >>  8);
  p[2] = (uint8_t) (v >> 16);
  p[3] = (uint8_t) (v >> 24);
}

uint32_t bitmap__rle_get32(const uint8_t *p)
{
  return (uint32_t) p[0]
       | (uint32_t) p[1] <<  8
       | (uint32_t) p[2] << 16
       | (uint32_t) p[3] << 24;
}

/* ----------------------------------------------------------------------- */

/* Read one pixel from `p` (1 or 4 bytes) as a host value. */
static pixelfmt_any_t rle__load_pixel(const uint8_t *p, int log2bpp)
{
  if (log2bpp == 3)
    return p[0];

  return (pixelfmt_any_t) p[0]
       | (pixelfmt_any_t) p[1] <<  8
       | (pixelfmt_any_t) p[2] << 16
       | (pixelfmt_any_t) p[3] << 24;
}

/* ----------------------------------------------------------------------- */

/*
 * Encoder cursor. `p` walks the output, `end` is one past the buffer;
 * `overflow` latches once a write would pass `end`.
 */
typedef struct rle__enc
{
  uint8_t *p;
  uint8_t *end;
  int      overflow;
}
rle__enc_t;

static void rle__put(rle__enc_t *e, uint8_t b)
{
  if (e->p >= e->end)
  {
    e->overflow = 1;
    return;
  }
  *e->p++ = b;
}

static void rle__put_pixel(rle__enc_t *e, pixelfmt_any_t px, int bpp)
{
  int i;

  for (i = 0; i < bpp; i++)
    rle__put(e, (uint8_t) (px >> (8 * i)));
}

/* ZEROIS2N: a length equal to 2^bits is stored as a zero field. */
static unsigned int rle__lenfield(int length, int bits)
{
  if (length == (1 << bits))
    return 0;

  return (unsigned int) length;
}

/* Emit a literal run: `length` pixels starting at `src` (already at the run). */
static void rle__emit_literal(rle__enc_t    *e,
                              const uint8_t *src,
                              int            length,
                              int            bpp)
{
  int chunk;
  int i;

  while (length > 0)
  {
    if (length <= bitmap__RLE_LITERAL_LEN_MAX)
    {
      chunk = length;
      rle__put(e, (uint8_t) (bitmap__RLE_VAL(bitmap__RLE_LITERAL_ID)
                             | rle__lenfield(chunk, bitmap__RLE_LITERAL_LEN_BITS)));
    }
    else
    {
      unsigned int store;

      chunk = MIN(length, bitmap__RLE_LITERAL_LONG_LEN_MAX);
      store = (unsigned int) (chunk - bitmap__RLE_LITERAL_LONG_LEN_MIN);
      rle__put(e, (uint8_t) (bitmap__RLE_VAL(bitmap__RLE_LITERAL_LONG_ID)
                             | (store >> 8)));
      rle__put(e, (uint8_t) store);
    }

    for (i = 0; i < chunk * bpp; i++)
      rle__put(e, src[i]);

    src    += (size_t) chunk * bpp;
    length -= chunk;
  }
}

/* Emit a repeat run: `length` copies of pixel `px`. */
static void rle__emit_repeat(rle__enc_t    *e,
                             pixelfmt_any_t px,
                             int            length,
                             int            bpp)
{
  int chunk;

  while (length > 0)
  {
    if (length <= bitmap__RLE_REPEAT_LEN_MAX)
    {
      chunk = length;
      rle__put(e, (uint8_t) (bitmap__RLE_VAL(bitmap__RLE_REPEAT_ID)
                             | rle__lenfield(chunk, bitmap__RLE_REPEAT_LEN_BITS)));
    }
    else
    {
      unsigned int store;

      chunk = MIN(length, bitmap__RLE_REPEAT_LONG_LEN_MAX);
      store = (unsigned int) (chunk - bitmap__RLE_REPEAT_LONG_LEN_MIN);
      rle__put(e, (uint8_t) (bitmap__RLE_VAL(bitmap__RLE_REPEAT_LONG_ID)
                             | (store >> 8)));
      rle__put(e, (uint8_t) store);
    }

    rle__put_pixel(e, px, bpp);
    length -= chunk;
  }
}

/* Emit a skip run: `length` transparent pixels. */
static void rle__emit_skip(rle__enc_t *e, int length)
{
  int chunk;

  while (length > 0)
  {
    if (length <= bitmap__RLE_SKIP_LEN_MAX)
    {
      chunk = length;
      rle__put(e, (uint8_t) (bitmap__RLE_VAL(bitmap__RLE_SKIP_ID)
                             | rle__lenfield(chunk, bitmap__RLE_SKIP_LEN_BITS)));
    }
    else
    {
      unsigned int store;

      chunk = MIN(length, bitmap__RLE_SKIP_LONG_LEN_MAX);
      store = (unsigned int) (chunk - bitmap__RLE_SKIP_LONG_LEN_MIN);
      rle__put(e, (uint8_t) (bitmap__RLE_VAL(bitmap__RLE_SKIP_LONG_ID)
                             | (store >> 8)));
      rle__put(e, (uint8_t) store);
    }

    length -= chunk;
  }
}

/* ----------------------------------------------------------------------- */

result_t bitmap__rle_encode_row(const void *srcrow,
                                int         npix,
                                int         log2bpp,
                                int         has_alpha,
                                uint8_t    *dst,
                                size_t      dstcap,
                                size_t     *dstused)
{
  rle__enc_t     e;
  const uint8_t *src;
  int            bpp;
  int            i;
  int            runstart;

  assert(srcrow);
  assert(dst);
  assert(log2bpp == 3 || log2bpp == 5);

  e.p        = dst;
  e.end      = dst + dstcap;
  e.overflow = 0;

  src = srcrow;
  bpp = 1 << (log2bpp - 3);

  i = 0;
  while (i < npix)
  {
    pixelfmt_any_t px0;
    int            run;
    int            transparent;

    px0 = rle__load_pixel(src + (size_t) i * bpp, log2bpp);

    /* Maximal identical run from i. */
    run = 1;
    while (i + run < npix
           && rle__load_pixel(src + (size_t) (i + run) * bpp, log2bpp) == px0)
      run++;

    /* A skip run drops the pixel bytes entirely, so it must be reversible:
     * only wholly-zero pixels qualify (the common PNG transparent value).
     * Non-zero-but-alpha-0 pixels go through as literal/repeat. */
    transparent = has_alpha && (px0 == 0);

    if (run >= 2 || transparent)
    {
      if (transparent)
        rle__emit_skip(&e, run);
      else
        rle__emit_repeat(&e, px0, run, bpp);
      i += run;
      continue;
    }

    /* Not worth a repeat: accumulate a literal run until the next repeatable
     * pair (or a transparent pixel for alpha formats). */
    runstart = i;
    i++;
    while (i < npix)
    {
      pixelfmt_any_t a;

      a = rle__load_pixel(src + (size_t) i * bpp, log2bpp);

      if (has_alpha && a == 0)
        break;
      if (i + 1 < npix
          && rle__load_pixel(src + (size_t) (i + 1) * bpp, log2bpp) == a)
        break;
      i++;
    }
    rle__emit_literal(&e, src + (size_t) runstart * bpp, i - runstart, bpp);
  }

  rle__put(&e, bitmap__RLE_EOL_BYTE);

  if (e.overflow)
    return result_BUFFER_OVERFLOW;

  *dstused = (size_t) (e.p - dst);
  return result_OK;
}

/* ----------------------------------------------------------------------- */

const uint8_t *bitmap__rle_decode_row(const uint8_t *p,
                                      int            log2bpp,
                                      void          *dstrow,
                                      int            skip,
                                      int            plot,
                                      int            zero_skip)
{
  uint8_t *dst;
  int      bpp;

  assert(p);
  assert(log2bpp == 3 || log2bpp == 5);

  dst = dstrow;
  bpp = 1 << (log2bpp - 3);

  for (;;)
  {
    uint8_t op;
    int     id;
    int     length;
    int     take;      /* pixels of this run actually written */
    int     drop;      /* pixels of this run swallowed by `skip` */

    op = *p++;
    id = clz8(op);

    if (id == bitmap__RLE_EOL_ID)
      break;

    /* Decode opcode + length; `srcpx` (literal) / `px` (repeat) set below. */
    if (id == bitmap__RLE_LITERAL_ID)
    {
      length = op & (bitmap__RLE_LITERAL_LEN_MAX - 1);
      if (length == 0)
        length = bitmap__RLE_LITERAL_LEN_MAX;
    }
    else if (id == bitmap__RLE_REPEAT_ID)
    {
      length = op & (bitmap__RLE_REPEAT_LEN_MAX - 1);
      if (length == 0)
        length = bitmap__RLE_REPEAT_LEN_MAX;
    }
    else if (id == bitmap__RLE_SKIP_ID)
    {
      length = op & (bitmap__RLE_SKIP_LEN_MAX - 1);
      if (length == 0)
        length = bitmap__RLE_SKIP_LEN_MAX;
    }
    else if (id == bitmap__RLE_LITERAL_LONG_ID)
    {
      length = ((op & (bitmap__RLE_VAL(bitmap__RLE_LITERAL_LONG_ID) - 1)) << 8)
             | *p++;
      length += bitmap__RLE_LITERAL_LONG_LEN_MIN;
    }
    else if (id == bitmap__RLE_REPEAT_LONG_ID)
    {
      length = ((op & (bitmap__RLE_VAL(bitmap__RLE_REPEAT_LONG_ID) - 1)) << 8)
             | *p++;
      length += bitmap__RLE_REPEAT_LONG_LEN_MIN;
    }
    else if (id == bitmap__RLE_SKIP_LONG_ID)
    {
      length = ((op & (bitmap__RLE_VAL(bitmap__RLE_SKIP_LONG_ID) - 1)) << 8)
             | *p++;
      length += bitmap__RLE_SKIP_LONG_LEN_MIN;
    }
    else
    {
      assert(!"bitmap__rle_decode_row: reserved opcode");
      break;
    }

    /* Clip this run against the remaining left-skip and plot budget. The
     * stream is still walked to EOL after `plot` hits 0 (take clamps to 0),
     * so the returned cursor always lands on the next row. */
    drop = MIN(skip, length);
    skip -= drop;
    take  = length - drop;
    if (take > plot)
      take = plot;
    if (take < 0)
      take = 0;

    switch (id)
    {
    case bitmap__RLE_LITERAL_ID:
    case bitmap__RLE_LITERAL_LONG_ID:
      if (take > 0)
        memcpy(dst, p + (size_t) drop * bpp, (size_t) take * bpp);
      p    += (size_t) length * bpp;
      dst  += (size_t) take * bpp;
      plot -= take;
      break;

    case bitmap__RLE_REPEAT_ID:
    case bitmap__RLE_REPEAT_LONG_ID:
      if (bpp == 1)
      {
        if (take > 0)
          memset(dst, p[0], (size_t) take);
      }
      else
      {
        int k;

        for (k = 0; k < take; k++)
          memcpy(dst + (size_t) k * bpp, p, (size_t) bpp);
      }
      p    += bpp;
      dst  += (size_t) take * bpp;
      plot -= take;
      break;

    default: /* skip */
      if (zero_skip && take > 0)
        memset(dst, 0, (size_t) take * bpp);
      dst  += (size_t) take * bpp;
      plot -= take;
      break;
    }
  }

  return p;
}

/* ----------------------------------------------------------------------- */

const uint8_t *bitmap__rle_skip_row(const uint8_t *p,
                                    const uint8_t *end,
                                    int            log2bpp)
{
  int bpp;

  assert(log2bpp == 3 || log2bpp == 5);
  bpp = 1 << (log2bpp - 3);

  while (p < end)
  {
    uint8_t op;
    int     id;
    int     length;

    op = *p++;
    id = clz8(op);

    if (id == bitmap__RLE_EOL_ID)
      break;

    switch (id)
    {
    case bitmap__RLE_LITERAL_ID:
      length = op & (bitmap__RLE_LITERAL_LEN_MAX - 1);
      if (length == 0)
        length = bitmap__RLE_LITERAL_LEN_MAX;
      p += (size_t) length * bpp;
      break;

    case bitmap__RLE_REPEAT_ID:
      p += bpp;
      break;

    case bitmap__RLE_SKIP_ID:
      break;

    case bitmap__RLE_LITERAL_LONG_ID:
      length = ((op & (bitmap__RLE_VAL(bitmap__RLE_LITERAL_LONG_ID) - 1)) << 8)
             | *p++;
      length += bitmap__RLE_LITERAL_LONG_LEN_MIN;
      p += (size_t) length * bpp;
      break;

    case bitmap__RLE_REPEAT_LONG_ID:
      p += 1;         /* low length byte */
      p += bpp;       /* inline pixel */
      break;

    case bitmap__RLE_SKIP_LONG_ID:
      p += 1;         /* low length byte */
      break;

    default:
      assert(!"bitmap__rle_skip_row: reserved opcode");
      return end;
    }
  }

  return p;
}

/* ----------------------------------------------------------------------- */

/* Is `fmt` a format the RLE codec accepts? Sets *has_alpha for the alpha
 * formats whose alpha lives in the top byte (bgra/rgba), which are the ones
 * that get `skip` runs.
 * ponytail: abgr8888/argb8888 carry alpha in the low byte; they still
 * compress but with no skip runs. Add a per-format alpha shift if sparse
 * abgr/argb art shows up. */
static int rle__format_ok(pixelfmt_t fmt, int *has_alpha)
{
  *has_alpha = 0;

  switch (fmt)
  {
  case pixelfmt_bgra8888:
  case pixelfmt_rgba8888:
    *has_alpha = 1;
    return 1;

  case pixelfmt_y8:
  case pixelfmt_bgrx8888:
  case pixelfmt_rgbx8888:
  case pixelfmt_xbgr8888:
  case pixelfmt_xrgb8888:
  case pixelfmt_abgr8888:
  case pixelfmt_argb8888:
    return 1;

  default:
    return 0;
  }
}

/* ----------------------------------------------------------------------- */

result_t bitmap_compress(bitmap_t *bm)
{
  result_t  rc;
  pixelfmt_t base;
  int       has_alpha;
  int       log2bpp;
  int       bpp;
  int       w, h;
  size_t    worst;
  size_t    rowworst;
  uint8_t  *blob;
  uint8_t  *cursor;
  const uint8_t *srcrow;
  uint32_t  data_len;
  uint8_t  *shrunk;
  int       y;

  assert(bm);

  if (pixelfmt_is_rle(bm->format))
    return result_OK; /* already compressed */

  base = pixelfmt_base(bm->format);
  if (!rle__format_ok(base, &has_alpha))
    return result_NOT_SUPPORTED;

  log2bpp = pixelfmt_log2bpp(base);
  bpp     = 1 << (log2bpp - 3);
  w       = bm->size.w;
  h       = bm->size.h;

  /* Worst case per row: every pixel literal, one control byte per 128 px,
   * plus an EOL. */
  rowworst = (size_t) w * bpp
           + ((size_t) w / bitmap__RLE_LITERAL_LEN_MAX + 1) * 2
           + 1;
  worst = bitmap__RLE_HEADER_SIZE + rowworst * (size_t) h;

  blob = malloc(worst);
  if (blob == NULL)
    return result_OOM;

  cursor = blob + bitmap__RLE_HEADER_SIZE;
  srcrow = bm->base;
  for (y = 0; y < h; y++)
  {
    size_t used;

    rc = bitmap__rle_encode_row(srcrow, w, log2bpp, has_alpha,
                                cursor, worst - (size_t) (cursor - blob),
                                &used);
    if (rc != result_OK)
    {
      free(blob);
      return rc;
    }

    cursor += used;
    srcrow += bm->rowbytes;
  }

  data_len = (uint32_t) (cursor - blob - bitmap__RLE_HEADER_SIZE);
  bitmap__rle_put32(blob + 0, (uint32_t) bm->rowbytes);
  bitmap__rle_put32(blob + 4, data_len);

  shrunk = realloc(blob, bitmap__RLE_HEADER_SIZE + data_len);
  if (shrunk != NULL)
    blob = shrunk;

  free(bm->base);
  bm->base     = blob;
  bm->format   = base | pixelfmt_RLE_FLAG;
  bm->rowbytes = 0;
  /* bm->span and bm->palette stay valid for the decoded (base) format. */

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t bitmap_decompress(bitmap_t *bm)
{
  pixelfmt_t     base;
  int            log2bpp;
  uint32_t       rowbytes;
  const uint8_t *p;
  uint8_t       *raw;
  uint8_t       *dstrow;
  int            y;

  assert(bm);

  if (!pixelfmt_is_rle(bm->format))
    return result_OK; /* not compressed */

  assert(bm->base != NULL);

  base    = pixelfmt_base(bm->format);
  log2bpp = pixelfmt_log2bpp(base);

  rowbytes = bitmap__rle_get32((const uint8_t *) bm->base + 0);
  p        = (const uint8_t *) bm->base + bitmap__RLE_HEADER_SIZE;

  raw = malloc((size_t) rowbytes * bm->size.h);
  if (raw == NULL)
    return result_OOM;

  dstrow = raw;
  for (y = 0; y < bm->size.h; y++)
  {
    p = bitmap__rle_decode_row(p, log2bpp, dstrow, 0, bm->size.w, 1);
    dstrow += rowbytes;
  }

  free(bm->base);
  bm->base     = raw;
  bm->format   = base;
  bm->rowbytes = (int) rowbytes;

  return result_OK;
}
