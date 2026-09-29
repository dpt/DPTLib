/* framebuf/pixelmap/get.c -- pixelmap lookup and cache */

#include <stddef.h>
#include <stdlib.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/pixelmap.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

static int pixelmap__is_deep(pixelfmt_t fmt)
{
  switch (fmt)
  {
  case pixelfmt_bgrx8888:
  case pixelfmt_rgbx8888:
  case pixelfmt_xbgr8888:
  case pixelfmt_xrgb8888:
  case pixelfmt_bgra8888:
  case pixelfmt_rgba8888:
  case pixelfmt_abgr8888:
  case pixelfmt_argb8888:
  case pixelfmt_rgb565:
  case pixelfmt_rgbx5551:
    return 1;
  default:
    return 0;
  }
}

/* deep formats whose destination pixel is 2 bytes wide, not 4 */
static int pixelmap__is_deep16(pixelfmt_t fmt)
{
  return fmt == pixelfmt_rgb565 || fmt == pixelfmt_rgbx5551;
}

static int pixelmap__paletted_log2bpp(pixelfmt_t fmt)
{
  switch (fmt)
  {
  case pixelfmt_p1: return 0;
  case pixelfmt_p2: return 1;
  case pixelfmt_p4: return 2;
  case pixelfmt_p8: return 3;
  default:          return -1;
  }
}

/* deep -> paletted: fill the channel quantisation layout. p1/p2/p4 quantise to
 * 4:4:4 (2KB max); p8 to 5:6:5 to keep 256-colour output faithful. */
static int pixelmap__layout_deep_to_paletted(pixelfmt_t  srcfmt,
                                             int         dest_log2bpp,
                                             pixelmap_t *out)
{
  out->entry_bytes = 0;

  if (dest_log2bpp == 3)
  {
    out->rbits = 5; out->gbits = 6; out->bbits = 5;
  }
  else
  {
    out->rbits = 4; out->gbits = 4; out->bbits = 4;
  }

  /* channel extract from the source pixel: shift, pre-shift mask and source
   * channel width. 8888 formats are 8-bit-per-channel, byte-aligned; 565/5551
   * pack narrower fields that need rescaling up to 8 bits after masking. */
  switch (srcfmt)
  {
  case pixelfmt_bgrx8888:
  case pixelfmt_bgra8888:
    out->rshift = 16; out->gshift = 8; out->bshift = 0;
    out->rmask = out->gmask = out->bmask = 0xFF;
    out->rsrcbits = out->gsrcbits = out->bsrcbits = 8;
    break;
  case pixelfmt_rgbx8888:
  case pixelfmt_rgba8888:
    out->rshift = 0; out->gshift = 8; out->bshift = 16;
    out->rmask = out->gmask = out->bmask = 0xFF;
    out->rsrcbits = out->gsrcbits = out->bsrcbits = 8;
    break;
  case pixelfmt_xbgr8888:
  case pixelfmt_abgr8888:
    out->rshift = 24; out->gshift = 16; out->bshift = 8;
    out->rmask = out->gmask = out->bmask = 0xFF;
    out->rsrcbits = out->gsrcbits = out->bsrcbits = 8;
    break;
  case pixelfmt_rgb565:
    out->rshift = PIXELFMT_Rxx565_SHIFT;
    out->gshift = PIXELFMT_xGx565_SHIFT;
    out->bshift = PIXELFMT_xxB565_SHIFT;
    out->rmask = 0x1F; out->gmask = 0x3F; out->bmask = 0x1F;
    out->rsrcbits = 5; out->gsrcbits = 6; out->bsrcbits = 5;
    break;
  case pixelfmt_rgbx5551:
    out->rshift = PIXELFMT_Rxxx5551_SHIFT;
    out->gshift = PIXELFMT_xGxx5551_SHIFT;
    out->bshift = PIXELFMT_xxBx5551_SHIFT;
    out->rmask = out->gmask = out->bmask = 0x1F;
    out->rsrcbits = out->gsrcbits = out->bsrcbits = 5;
    break;
  default: /* xrgb8888, argb8888 */
    out->rshift = 8; out->gshift = 16; out->bshift = 24;
    out->rmask = out->gmask = out->bmask = 0xFF;
    out->rsrcbits = out->gsrcbits = out->bsrcbits = 8;
    break;
  }

  out->nentries = 1u << (out->rbits + out->gbits + out->bbits);

  return 0;
}

/* paletted -> deep: the index is the raw palette index, one deep pixel per
 * entry. nentries comes from the palette, filled in by the caller. */
static int pixelmap__layout_paletted_to_deep(pixelfmt_t  destfmt,
                                             int         nentries,
                                             pixelmap_t *out)
{
  out->entry_bytes = pixelmap__is_deep16(destfmt) ? 2 : 4;
  out->rbits = out->gbits = out->bbits = 0;
  out->rshift = out->gshift = out->bshift = 0;
  out->rmask = out->gmask = out->bmask = 0;
  out->rsrcbits = out->gsrcbits = out->bsrcbits = 0;
  out->nentries = (unsigned int) nentries;

  return 0;
}

int pixelmap__layout_for(pixelfmt_t  srcfmt,
                         pixelfmt_t  destfmt,
                         pixelmap_t *out)
{
  int src_deep;
  int dst_deep;
  int dest_log2bpp;

  src_deep = pixelmap__is_deep(srcfmt);
  dst_deep = pixelmap__is_deep(destfmt);

  out->srcfmt       = srcfmt;
  out->destfmt      = destfmt;
  out->dest_log2bpp = -1;
  out->entries      = NULL;

  if (src_deep && !dst_deep)
  {
    dest_log2bpp = pixelmap__paletted_log2bpp(destfmt);
    if (dest_log2bpp < 0)
      return -1;
    out->dest_log2bpp = dest_log2bpp;
    return pixelmap__layout_deep_to_paletted(srcfmt, dest_log2bpp, out);
  }

  if (!src_deep && dst_deep)
  {
    if (pixelmap__paletted_log2bpp(srcfmt) < 0)
      return -1;
    /* nentries is set from the palette count by the caller-side layout call */
    return pixelmap__layout_paletted_to_deep(destfmt, 0, out);
  }

  return -1; /* deep<->deep and paletted<->paletted are not pixelmap's job */
}

void pixelmap_extract_rgb(const pixelmap_t *pm,
                          unsigned int      pixel,
                          unsigned int     *r,
                          unsigned int     *g,
                          unsigned int     *b)
{
  unsigned int v;

  v  = (pixel >> pm->rshift) & pm->rmask;
  *r = (pm->rsrcbits == 8) ? v : (v << (8 - pm->rsrcbits));

  v  = (pixel >> pm->gshift) & pm->gmask;
  *g = (pm->gsrcbits == 8) ? v : (v << (8 - pm->gsrcbits));

  v  = (pixel >> pm->bshift) & pm->bmask;
  *b = (pm->bsrcbits == 8) ? v : (v << (8 - pm->bsrcbits));
}

/* ----------------------------------------------------------------------- */

/* FNV-1a over the palette bytes. */
static unsigned int pixelmap__hash(const colour_t *palette, int nentries)
{
  const unsigned char *p;
  size_t               n;
  unsigned int         h;
  size_t               i;

  p = (const unsigned char *) palette;
  n = (size_t) nentries * sizeof(*palette);
  h = 2166136261u;

  for (i = 0; i < n; i++)
  {
    h ^= p[i];
    h *= 16777619u;
  }

  return h;
}

/* ----------------------------------------------------------------------- */

#define PIXELMAP_CACHE_SIZE 4

struct pixelmap__cacheent
{
  int          used;
  pixelfmt_t   srcfmt;
  pixelfmt_t   destfmt;
  unsigned int palhash;
  unsigned int lru;      /* higher = more recently used */
  pixelmap_t   map;
};

const pixelmap_t *pixelmap_get(pixelfmt_t      srcfmt,
                               pixelfmt_t      destfmt,
                               const colour_t *palette,
                               int             nentries)
{
  static struct pixelmap__cacheent cache[PIXELMAP_CACHE_SIZE];
  static unsigned int clock;

  unsigned int        palhash;
  int                 victim;
  unsigned int        oldest;
  int                 i;

  if (palette == NULL)
    return NULL;

  palhash = pixelmap__hash(palette, nentries);

  for (i = 0; i < PIXELMAP_CACHE_SIZE; i++)
    if (cache[i].used &&
        cache[i].srcfmt  == srcfmt &&
        cache[i].destfmt == destfmt &&
        cache[i].palhash == palhash)
    {
      cache[i].lru = ++clock;
      return &cache[i].map;
    }

  /* miss: pick a victim (unused, else least-recently-used) */
  victim = 0;
  oldest = cache[0].used ? cache[0].lru : 0;
  for (i = 0; i < PIXELMAP_CACHE_SIZE; i++)
  {
    if (!cache[i].used)
    {
      victim = i;
      break;
    }
    if (cache[i].lru < oldest)
    {
      oldest = cache[i].lru;
      victim = i;
    }
  }

  if (cache[victim].used)
  {
    free((void *) cache[victim].map.entries);
    cache[victim].used = 0;
  }

  if (pixelmap__layout_for(srcfmt, destfmt, &cache[victim].map) != 0)
    return NULL;

  /* paletted -> deep: the entry count is the palette size, not known to
   * pixelmap__layout_for */
  if (cache[victim].map.entry_bytes != 0)
    cache[victim].map.nentries = (unsigned int) nentries;

  if (pixelmap__build_table(&cache[victim].map, palette, nentries) != 0)
    return NULL;

  cache[victim].used    = 1;
  cache[victim].srcfmt  = srcfmt;
  cache[victim].destfmt = destfmt;
  cache[victim].palhash = palhash;
  cache[victim].lru     = ++clock;

  return &cache[victim].map;
}
