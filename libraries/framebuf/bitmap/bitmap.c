/* bitmap.c */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <strings.h>

#include "framebuf/bitmap.h"

result_t bitmap_init(bitmap_t       *bm,
                     int             width,
                     int             height,
                     pixelfmt_t      fmt,
                     int             rowbytes,
                     const colour_t *palette,
                     void           *base)
{
  int log2bpp;

  assert(bm);

  bm->width    = width;
  bm->height   = height;
  bm->format   = fmt;
  bm->rowbytes = rowbytes;
  bm->palette  = NULL;
  bm->base     = base;

  log2bpp = pixelfmt_log2bpp(fmt);
  if (log2bpp <= 3 && palette)
  {
    int       nentries;
    colour_t *newpal;

    nentries = 1 << (1 << log2bpp);
    newpal = malloc(nentries * sizeof(*newpal));
    if (newpal == NULL)
      return result_OOM;

    memcpy(newpal, palette, nentries * sizeof(*newpal));

    bm->palette = newpal;
  }

  return result_OK;
}

void bitmap_clear(bitmap_t *bm, colour_t colour)
{
  int          log2bpp;
  unsigned int px;
  int          x,y;

  assert(bm);

  log2bpp = pixelfmt_log2bpp(bm->format);
  px = colour_to_pixel(bm->palette, 1 << (1 << log2bpp), colour, bm->format);

  switch (log2bpp)
  {
  case 0: /* 1bpp */
    memset(bm->base, px * 0xFF, bm->rowbytes * bm->height);
    break;
  case 1: /* 2bpp */
    memset(bm->base, px * 0x55, bm->rowbytes * bm->height);
    break;
  case 2: /* 4bpp */
    memset(bm->base, px * 0x11, bm->rowbytes * bm->height);
    break;
  case 3: /* 8bpp */
    memset(bm->base, px * 0x01, bm->rowbytes * bm->height);
    break;

  case 5: /* 32bpp - pixels are ints */
  {
    unsigned int *pixels;

    pixels = bm->base;
    for (y = 0; y < bm->height; y++)
      for (x = 0; x < bm->width; x++)
        *pixels++ = px;
    // pixels += bm->rowbytes / sizeof(*pixels) - bm->width;  cope with gaps etc.
  }
    break;

  default:
    assert(0);
    return; /* not implemented */
  }
}

static result_t bmconv_p4_to_bgrx8888(const bitmap_t *src, bitmap_t **pdst)
{
  result_t             rc;
  bitmap_t            *dst;
  pixelfmt_bgrx8888_t *outpixels;
  pixelfmt_bgrx8888_t  map[16];
  int                  i;
  pixelfmt_p4_t       *inpixels;
  int                  x,y;

  assert(src);
  assert(src->palette);

  for (i = 0; i < 16; i++)
    map[i] = colour_to_pixel(src->palette, 16, src->palette[i], pixelfmt_bgrx8888);

  outpixels = malloc(src->width * sizeof(pixelfmt_bgrx8888_t) * src->height); // rowbytes rounding needed?
  if (outpixels == NULL)
    return result_OOM;

  dst = malloc(sizeof(*dst));
  if (dst == NULL)
  {
    free(outpixels);
    return result_OOM;
  }

  rc = bitmap_init(dst,
                   src->width, src->height,
                   pixelfmt_bgrx8888,
                   src->width * sizeof(pixelfmt_bgrx8888_t),
                   NULL,
                   outpixels);
  if (rc)
    return rc;

  inpixels = src->base;
  for (y = 0; y < src->height; y++)
  {
    for (x = 0; x < src->width / 8; x++)
    {
      pixelfmt_p4_t in = *inpixels++; // fetches 8 pixels
      // 0xABCDEFGH is 8 4bpp pixels shown H,G,F,E,D,C,B,A
      *outpixels++ = map[(in >>  0) & 0xF];
      *outpixels++ = map[(in >>  4) & 0xF];
      *outpixels++ = map[(in >>  8) & 0xF];
      *outpixels++ = map[(in >> 12) & 0xF];
      *outpixels++ = map[(in >> 16) & 0xF];
      *outpixels++ = map[(in >> 20) & 0xF];
      *outpixels++ = map[(in >> 24) & 0xF];
      *outpixels++ = map[(in >> 28) & 0xF];
    }
  }

  *pdst = dst;

  return rc;
}

result_t bitmap_convert(const bitmap_t *src, pixelfmt_t newfmt, bitmap_t **dst)
{
  *dst = NULL;

  switch (src->format)
  {
  case pixelfmt_p4:
    switch (newfmt)
    {
    case pixelfmt_bgrx8888:
      return bmconv_p4_to_bgrx8888(src, dst);

    default:
      return result_NOT_SUPPORTED;
    }
    break;

  default:
    return result_NOT_SUPPORTED;
  }

  return result_OK;
}
