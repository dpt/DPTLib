/* framebuf/pixelmap/test/pixelmap-test.c */

#include <stdio.h>
#include <stdlib.h>

#include "base/result.h"

#include "framebuf/colour.h"
#include "framebuf/palettes.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/pixelmap.h"

#include "test/all-tests.h"

/* compose the packed index for an rgba8888 source pixel */
static unsigned int pm_index(const pixelmap_t *pm, pixelfmt_rgba8888_t px)
{
  unsigned int r, g, b;

  r = (px >> pm->rshift) & 0xFF;
  g = (px >> pm->gshift) & 0xFF;
  b = (px >> pm->bshift) & 0xFF;

  return ((r >> (8 - pm->rbits)) << (pm->gbits + pm->bbits))
       | ((g >> (8 - pm->gbits)) << pm->bbits)
       | ( b >> (8 - pm->bbits));
}

static unsigned int pm_lookup(const pixelmap_t *pm, unsigned int idx)
{
  unsigned int per_byte;
  unsigned int bit;

  if (pm->dest_log2bpp == 3)
    return pm->entries[idx];

  per_byte = 8u >> pm->dest_log2bpp;
  bit      = (idx % per_byte) << pm->dest_log2bpp;

  return (pm->entries[idx / per_byte] >> bit) & ((1u << (1u << pm->dest_log2bpp)) - 1);
}

static int chan_dist(pixelfmt_rgba8888_t a, pixelfmt_rgba8888_t b)
{
  int dr, dg, db;

  dr = (int) PIXELFMT_Rxxx8888(a) - (int) PIXELFMT_Rxxx8888(b);
  dg = (int) PIXELFMT_xGxx8888(a) - (int) PIXELFMT_xGxx8888(b);
  db = (int) PIXELFMT_xxBx8888(a) - (int) PIXELFMT_xxBx8888(b);

  return dr * dr + dg * dg + db * db;
}

result_t pixelmap_test(const char *resources)
{
  colour_t          pal[palette_PICO8__LENGTH];
  const pixelmap_t *pm, *pm2, *pm8;
  unsigned int      idx;
  int               i;

  (void) resources;

  define_pico8_palette(pal);

  /* 1. rgba8888 -> p4: known colours land on the expected index */
  pm = pixelmap_get(pixelfmt_rgba8888, pixelfmt_p4, pal, palette_PICO8__LENGTH);
  if (pm == NULL)
  {
    printf("pixelmap: get rgba8888->p4 returned NULL\n");
    return result_TEST_FAILED;
  }

  for (i = 0; i < palette_PICO8__LENGTH; i++)
  {
    unsigned int got;

    idx = pm_index(pm, pal[i].primary);
    got = pm_lookup(pm, idx);

    /* the exact palette colour, quantised to 4:4:4, should match itself or a
     * near neighbour -- assert the resolved colour is close, not that the
     * index is identical */
    if (chan_dist(pal[i].primary, pal[got].primary) > chan_dist(pal[i].primary, pal[i].primary) + 3 * 16 * 16)
    {
      printf("pixelmap: entry %d resolves too far (idx %u -> pal %u)\n", i, idx, got);
      return result_TEST_FAILED;
    }
  }

  /* 2. same args -> same cached pointer */
  pm2 = pixelmap_get(pixelfmt_rgba8888, pixelfmt_p4, pal, palette_PICO8__LENGTH);
  if (pm2 != pm)
  {
    printf("pixelmap: cache miss on identical request\n");
    return result_TEST_FAILED;
  }

  /* 3. unsupported pair -> NULL */
  if (pixelmap_get(pixelfmt_rgba8888, pixelfmt_rgba8888, pal, palette_PICO8__LENGTH) != NULL)
  {
    printf("pixelmap: unsupported destfmt did not return NULL\n");
    return result_TEST_FAILED;
  }

  /* 4. rgba8888 -> p8 (64KB table): every palette entry resolves close */
  pm8 = pixelmap_get(pixelfmt_rgba8888, pixelfmt_p8, pal, palette_PICO8__LENGTH);
  if (pm8 == NULL)
  {
    printf("pixelmap: get rgba8888->p8 returned NULL\n");
    return result_TEST_FAILED;
  }
  if (pm8->nentries != 65536)
  {
    printf("pixelmap: p8 table has %u entries, expected 65536\n", pm8->nentries);
    return result_TEST_FAILED;
  }

  for (i = 0; i < palette_PICO8__LENGTH; i++)
  {
    unsigned int got;

    idx = pm_index(pm8, pal[i].primary);
    got = pm_lookup(pm8, idx);

    /* 5:6:5 quantisation: within ~8 levels per channel */
    if (chan_dist(pal[i].primary, pal[got].primary) > 3 * 8 * 8)
    {
      printf("pixelmap: p8 entry %d resolves too far (pal %u)\n", i, got);
      return result_TEST_FAILED;
    }
  }

  /* 4b. rgba8888 -> p1 (2KB 4:4:4 table): a 2-entry black/white palette
   *     resolves dark colours to index 0 and light ones to index 1 */
  {
    const pixelmap_t *pm1;
    colour_t          bw[2];

    bw[0] = colour_rgb(0x00, 0x00, 0x00);
    bw[1] = colour_rgb(0xFF, 0xFF, 0xFF);

    pm1 = pixelmap_get(pixelfmt_rgba8888, pixelfmt_p1, bw, 2);
    if (pm1 == NULL)
    {
      printf("pixelmap: get rgba8888->p1 returned NULL\n");
      return result_TEST_FAILED;
    }
    if (pm1->dest_log2bpp != 0 || pm1->nentries != 4096)
    {
      printf("pixelmap: p1 layout wrong (dest_log2bpp %d, nentries %u)\n",
             pm1->dest_log2bpp, pm1->nentries);
      return result_TEST_FAILED;
    }

    if (pm_lookup(pm1, pm_index(pm1, colour_rgb(0x10, 0x10, 0x10).primary)) != 0)
    {
      printf("pixelmap: p1 dark colour did not resolve to index 0\n");
      return result_TEST_FAILED;
    }
    if (pm_lookup(pm1, pm_index(pm1, colour_rgb(0xF0, 0xF0, 0xF0).primary)) != 1)
    {
      printf("pixelmap: p1 light colour did not resolve to index 1\n");
      return result_TEST_FAILED;
    }
  }

  /* 4c. rgba8888 -> p2 (4:4:4 table): a 4-entry greyscale palette resolves
   *     each grey band to its nearest index */
  {
    const pixelmap_t *pm2;
    colour_t          grey[4];

    grey[0] = colour_rgb(0x00, 0x00, 0x00);
    grey[1] = colour_rgb(0x55, 0x55, 0x55);
    grey[2] = colour_rgb(0xAA, 0xAA, 0xAA);
    grey[3] = colour_rgb(0xFF, 0xFF, 0xFF);

    pm2 = pixelmap_get(pixelfmt_rgba8888, pixelfmt_p2, grey, 4);
    if (pm2 == NULL)
    {
      printf("pixelmap: get rgba8888->p2 returned NULL\n");
      return result_TEST_FAILED;
    }
    if (pm2->dest_log2bpp != 1 || pm2->nentries != 4096)
    {
      printf("pixelmap: p2 layout wrong (dest_log2bpp %d, nentries %u)\n",
             pm2->dest_log2bpp, pm2->nentries);
      return result_TEST_FAILED;
    }

    if (pm_lookup(pm2, pm_index(pm2, colour_rgb(0x08, 0x08, 0x08).primary)) != 0)
    {
      printf("pixelmap: p2 black did not resolve to index 0\n");
      return result_TEST_FAILED;
    }
    if (pm_lookup(pm2, pm_index(pm2, colour_rgb(0xF8, 0xF8, 0xF8).primary)) != 3)
    {
      printf("pixelmap: p2 white did not resolve to index 3\n");
      return result_TEST_FAILED;
    }
  }

  /* 5. paletted -> deep: p4 -> bgrx8888, one deep pixel per palette index */
  {
    const pixelmap_t          *pmd;
    const pixelfmt_bgrx8888_t *map;

    pmd = pixelmap_get(pixelfmt_p4, pixelfmt_bgrx8888, pal, palette_PICO8__LENGTH);
    if (pmd == NULL)
    {
      printf("pixelmap: get p4->bgrx8888 returned NULL\n");
      return result_TEST_FAILED;
    }
    if (pmd->entry_bytes != 4 || pmd->nentries != palette_PICO8__LENGTH)
    {
      printf("pixelmap: p4->bgrx8888 layout wrong (entry_bytes %u, nentries %u)\n",
             pmd->entry_bytes, pmd->nentries);
      return result_TEST_FAILED;
    }

    map = (const pixelfmt_bgrx8888_t *) pmd->entries;
    for (i = 0; i < palette_PICO8__LENGTH; i++)
    {
      pixelfmt_bgrx8888_t want;

      want = (pixelfmt_bgrx8888_t) colour_to_pixel(pal, palette_PICO8__LENGTH,
                                                   pal[i], pixelfmt_bgrx8888);
      if (map[i] != want)
      {
        printf("pixelmap: p4->bgrx8888 entry %d = 0x%08X, want 0x%08X\n",
               i, map[i], want);
        return result_TEST_FAILED;
      }
    }
  }

  return result_TEST_PASSED;
}
