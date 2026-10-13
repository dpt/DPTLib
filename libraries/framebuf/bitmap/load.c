/* framebuf/bitmap/load.c */

#include <assert.h>
#include <stdlib.h>

#include <png.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

static int file_is_png(FILE *fp)
{
#define PNGSIGLEN (8)
  unsigned char buf[PNGSIGLEN];

  if (fseek(fp, 0, SEEK_SET) < 0)
    return 0;

  if (fread(buf, 1, PNGSIGLEN, fp) != PNGSIGLEN)
    return 0;

  return png_sig_cmp(&buf[0], 0, PNGSIGLEN) == 0;
}

/* Smallest paletted format holding a PNG's 1, 2, 4 or 8 bits per pixel. PNG
 * packs the leftmost pixel in the high bits, as p1 and p2 do but p4 (low
 * nibble first) does not, so a 4-bit PNG needs png_set_packswap. */
static pixelfmt_t paletted_format(png_structp png_ptr, int bitdepth)
{
  switch (bitdepth)
  {
  case 1:
    return pixelfmt_p1;
  case 2:
    return pixelfmt_p2;
  case 4:
    png_set_packswap(png_ptr);
    return pixelfmt_p4;
  default:
    return pixelfmt_p8;
  }
}

result_t bitmap_load_png(bitmap_t *bm, const char *filename)
{
  result_t       rc;
  FILE          *fp;
  png_structp    png_ptr      = NULL;
  png_infop      info_ptr;
  png_uint_32    pngwidth, pngheight;
  int            pngbitdepth, pngcolourtype;
  unsigned char *pixels       = NULL;
  png_bytep     *row_pointers = NULL;
  pixelfmt_t     bm_fmt;
  int            bm_rowbytes;
  colour_t       plte[256];
  colour_t      *bm_palette   = NULL;
  png_uint_32    h;

  fp = fopen(filename, "rb");
  if (fp == NULL)
  {
    rc = result_FILE_NOT_FOUND;
    goto cleanup;
  }

  if (!file_is_png(fp))
  {
    rc = result_BAD_ARG;
    goto cleanup;
  }

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL)
  {
    rc = result_OOM;
    goto cleanup;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL)
  {
    rc = result_OOM;
    goto cleanup;
  }

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    rc = result_BAD_ARG;
    goto cleanup;
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, PNGSIGLEN);
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr,
              &pngwidth, &pngheight, &pngbitdepth, &pngcolourtype,
               NULL, NULL, NULL);

  /* paletted and greyscale PNGs may be 1/2/4/8 bpp (greyscale also 16, which
   * is stripped to 8). every other colour type must already be 8-bit. */
  if (pngbitdepth != 8 && pngcolourtype != PNG_COLOR_TYPE_PALETTE &&
      pngcolourtype != PNG_COLOR_TYPE_GRAY)
  {
    rc = result_INCOMPATIBLE;
    goto cleanup;
  }

  switch (pngcolourtype)
  {
  case PNG_COLOR_TYPE_PALETTE:
    /* keep a native paletted PNG paletted, at its own depth, carrying its
     * PLTE across as the bitmap's palette, rather than expanding to RGB */
    {
      png_colorp png_palette;
      int        nplte;
      png_bytep  trns       = NULL;
      int        ntrns      = 0;
      int        i;

      if (png_get_PLTE(png_ptr, info_ptr, &png_palette, &nplte) == 0)
      {
        rc = result_INCOMPATIBLE;
        goto cleanup;
      }
      if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_get_tRNS(png_ptr, info_ptr, &trns, &ntrns, NULL);

      for (i = 0; i < nplte; i++)
      {
        int a;

        a = (i < ntrns) ? trns[i] : 0xFF;
        plte[i] = colour_rgba(png_palette[i].red,
                              png_palette[i].green,
                              png_palette[i].blue,
                              a);
      }
      for (; i < 256; i++)
        plte[i] = colour_rgba(0, 0, 0, 0xFF);

      bm_fmt     = paletted_format(png_ptr, pngbitdepth);
      bm_palette = plte;
    }
    break;
  case PNG_COLOR_TYPE_GRAY:
    /* as a paletted PNG, with a ramp of greys as the palette */
    {
      int i;
      int ngrey;
      int grey;
      int was16;

      was16 = (pngbitdepth == 16);
      if (was16)
      {
        png_set_strip_16(png_ptr);
        pngbitdepth = 8;
      }

      bm_fmt = paletted_format(png_ptr, pngbitdepth);

      ngrey = 1 << pngbitdepth;
      for (i = 0; i < ngrey; i++)
      {
        grey    = i * 255 / (ngrey - 1);
        plte[i] = colour_rgba(grey, grey, grey, 0xFF);
      }

      /* a colour-key tRNS makes the keyed grey transparent. A 16-bit key
       * was stripped to 8 bits above, so it no longer names a palette
       * entry; leave those opaque */
      if (!was16 && png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      {
        png_color_16p key = NULL;

        png_get_tRNS(png_ptr, info_ptr, NULL, NULL, &key);
        if (key != NULL && key->gray < ngrey)
        {
          grey            = key->gray * 255 / (ngrey - 1);
          plte[key->gray] = colour_rgba(grey, grey, grey, 0);
        }
      }
      bm_palette = plte;
    }
    break;
  case PNG_COLOR_TYPE_RGB:
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    {
      /* a colour-key tRNS: the keyed colour becomes alpha 0 */
      png_set_tRNS_to_alpha(png_ptr);
      bm_fmt = pixelfmt_rgba8888;
    }
    else
    {
      bm_fmt = pixelfmt_rgbx8888;
      png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
    break;
  case PNG_COLOR_TYPE_RGBA:
    bm_fmt = pixelfmt_rgba8888;
    break;
  case PNG_COLOR_TYPE_GRAY_ALPHA:
    png_set_gray_to_rgb(png_ptr);
    bm_fmt = pixelfmt_rgba8888;
    break;
  default:
    rc = result_INCOMPATIBLE;
    goto cleanup;
  }

  /* rounded up: a sub-byte row ends part way through its last byte */
  bm_rowbytes = ((pngwidth << pixelfmt_log2bpp(bm_fmt)) + 7) >> 3;

  pixels = malloc(bm_rowbytes * pngheight);
  if (pixels == NULL)
    goto oom;

  row_pointers = malloc(pngheight * sizeof(*row_pointers));
  if (row_pointers == NULL)
    goto oom;

  for (h = 0; h < pngheight; h++)
    row_pointers[h] = pixels + bm_rowbytes * h;

  png_read_image(png_ptr, row_pointers);

  rc = bitmap_init(bm, SIZE2D(pngwidth, pngheight),
                   bm_fmt,
                   bm_rowbytes,
                   bm_palette, /* p8: PLTE copied in; NULL for deep formats */
                   pixels);
  if (rc != result_OK)
    goto cleanup;

  pixels = NULL; /* belongs to bitmap now */

  goto cleanup;

oom:
  rc = result_OOM;
  goto cleanup;

cleanup:
  free(row_pointers);
  free(pixels);
  if (png_ptr)
    png_destroy_read_struct(&png_ptr, NULL, NULL);
  if (fp)
    fclose(fp);
  return rc;
}

/* vim: set ts=8 sts=2 sw=2 et: */
