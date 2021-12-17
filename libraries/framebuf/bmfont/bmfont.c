/* bmfont.c */

/* TODO
 * - Detect colours by inspecting palette rather than assuming specific indices.
 * - Sidebearings
 * - Kerning
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "utils/array.h"
#include "utils/bytesex.h"

/* -------------------------------------------------------------------------- */

/* Configuration */

/* Input font PNGs must have 32 characters per row */
#define CHARS_PER_ROW   (32)

#undef BMFONT_DEBUG

/* -------------------------------------------------------------------------- */

/* Pixel values */
#define PIXEL_BG_IDX    (0) /* background */
#define PIXEL_FG_IDX    (1) /* font */
#define PIXEL_WIDTH_IDX (2) /* advance widths */
#define PIXEL_GRID_IDX  (3) /* grid (not presently interpreted) */

/* -------------------------------------------------------------------------- */

struct bmfont
{
  png_uint_32     gridwidth, gridheight; /* pixels */
  png_uint_32     charwidth, charheight; /* em size in pixels */
  int             totalchars;

  void           *glyphs;
  int             glyphrowbytes;

  bmfont_width_t *adw; /* an array of length totalchars */
  int             adw_used;
  int             adw_allocated;
};

/* -------------------------------------------------------------------------- */

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

/** Builds a table that yields the count of 2bpp pixels of value `idx`. */
static void build_adw_tab(unsigned char tab[256], int idx)
{
  int i;

  for (i = 0; i < 256; i++)
    tab[i] = (((i >> 0) & 3) == idx) +
             (((i >> 2) & 3) == idx) +
             (((i >> 4) & 3) == idx) +
             (((i >> 6) & 3) == idx);
}

/** Counts advance widths. */
static int count_adw(unsigned char tab[256], pixelfmt_x_t adw_px)
{
  return tab[(adw_px >>  0) & 0xFF] +
         tab[(adw_px >>  8) & 0xFF] +
         tab[(adw_px >> 16) & 0xFF] +
         tab[(adw_px >> 24) & 0xFF];
}

/** Verify the font format and build the advance width table. */
static result_t extract_advance_widths(bmfont_t    *bmfont,
                                       void        *voidpixels,
                                       png_uint_32  imgwidth,
                                       png_uint_32  imgheight,
                                       size_t       rowbytes)
{
  result_t      rc = result_OK;
  unsigned char adwtab[256];
  unsigned int *pixels;
  int           bitsperchar;
  unsigned int  mask;
  png_uint_32   y;

  assert(bmfont);
  assert(voidpixels);
  assert(imgwidth  > 0);
  assert(imgheight > 0);
  assert(rowbytes  > 0);

  build_adw_tab(adwtab, PIXEL_WIDTH_IDX);

  pixels      = (unsigned int *) voidpixels;
  bitsperchar = bmfont->charwidth * 2; /* 2 because 2bpp */
  mask        = 0xFFFFFFFFu << (32 - bitsperchar);

  assert(bitsperchar <= 32);

  for (y = 0; y < imgheight; y += bmfont->gridheight)
  {
    unsigned int currbits;
    int          ncurrbits;
    unsigned int nextbits;
    int          nnextbits;
    png_uint_32  x;
    pixelfmt_x_t adw_px;

    /* maintain two words, a current and a pending so we've always got enough bits ready */
    currbits  = rev_l(*pixels++);
    ncurrbits = 32; /* bits available in currbits */
    nextbits  = 0;
    nnextbits = 0;  /* bits available in nextbits */

    for (x = 0; x < imgwidth; x += bmfont->charwidth)
    {
      while (ncurrbits < bitsperchar)
      {
        int bitsused;

        if (nnextbits == 0) /* refill if needed */
        {
          nextbits  = rev_l(*pixels++); // TODO check for end of buffer
          nnextbits = 32;
        }

        /* shuffle new bits into place */
        currbits |= nextbits >> ncurrbits;
        if ((bitsused = MIN(nnextbits, 32 - ncurrbits)) < 32)
          nextbits <<= bitsused; /* discard used bits */
        nnextbits -= bitsused;
        ncurrbits += bitsused;
      }
      assert(ncurrbits >= bitsperchar);

      adw_px = currbits & mask; /* extract high bits */
      currbits <<= bitsperchar; /* discard used bits */
      ncurrbits -= bitsperchar;

      if (array_grow((void **) &bmfont->adw,
                                sizeof(*bmfont->adw),
                                bmfont->adw_used,
                       (int *) &bmfont->adw_allocated,
                                1,
                                8))
        goto oom;

      bmfont->adw[bmfont->adw_used++] = count_adw(adwtab, adw_px);
    }

    pixels += ((bmfont->gridheight - 1) * rowbytes) / sizeof(*pixels);

    assert(pixels >= (unsigned int *) voidpixels);
    assert(pixels <= (unsigned int *) voidpixels + (rowbytes * imgheight / 4));
  }

  return rc;


oom:
  rc = result_OOM;
  return rc;
}

/** Builds a table that packs a byte of 2bpp pixels to a nibble of 1bpp. */
static void build_repack_tab(unsigned char tab[256], int idx)
{
  int i;

  for (i = 0; i < 256; i++)
    tab[i] = ((((i >> 0) & 3) == idx) << 0) |
             ((((i >> 2) & 3) == idx) << 1) |
             ((((i >> 4) & 3) == idx) << 2) |
             ((((i >> 6) & 3) == idx) << 3);
}

static result_t extract_glyphs(bmfont_t    *bmfont,
                               void        *voidpixels,
                               png_uint_32  imgwidth,
                               png_uint_32  imgheight,
                               size_t       rowbytes)
{
  result_t       rc = result_OK;
  unsigned char  repacktab[256];
  size_t         glyphbytes;
  unsigned char *glyphs;
  unsigned int  *pixels;
  int            bitsperchar;
  int            shift;
  unsigned int   mask;
  int            sizeofchar;
  int            sizeofrow;
  png_uint_32    y;

  assert(bmfont);
  assert(voidpixels);
  assert(imgwidth > 0);
  assert(imgheight > 0);
  assert(rowbytes > 0);

  build_repack_tab(repacktab, PIXEL_FG_IDX);

  glyphbytes = bmfont->glyphrowbytes * bmfont->charheight * bmfont->totalchars;
  glyphs = malloc(glyphbytes);
  if (glyphs == NULL)
    goto oom;

  bmfont->glyphs = glyphs;

  pixels      = (unsigned int *) voidpixels;
  bitsperchar = bmfont->charwidth * 2; /* 2 because 2bpp */
  shift       = 32 - bitsperchar;
  mask        = 0xFFFFFFFFu << shift;

  sizeofchar  = bmfont->glyphrowbytes * bmfont->charheight;
  sizeofrow   = CHARS_PER_ROW * sizeofchar;

  for (y = 0; y < imgheight; y++)
  {
    unsigned int currbits;
    int          ncurrbits;
    unsigned int nextbits;
    int          nnextbits;
    int          rowoffset;
    int          lineoffset;
    png_uint_32  x;
    pixelfmt_x_t glyph_px;

    /* skip those rows that contain advance widths */
    if ((y % bmfont->gridheight) == 0)
    {
      pixels += imgwidth / (32 / 2);
      continue;
    }

    /* maintain two words, a current and a pending so we've always got enough bits ready */
    currbits  = rev_l(*pixels++);
    ncurrbits = 32; /* bits available in currbits */
    nextbits  = 0;
    nnextbits = 0;  /* bits available in nextbits */

    rowoffset  = (y / bmfont->gridheight) * sizeofrow;
    lineoffset = ((y % bmfont->gridheight) - 1) * bmfont->glyphrowbytes;

    for (x = 0; x < imgwidth; x += bmfont->charwidth)
    {
      unsigned int monopixels;
      int          charoffset;
      int          offset;

      while (ncurrbits < bitsperchar)
      {
        int bitsused;

        if (nnextbits == 0) /* refill if needed */
        {
          nextbits = rev_l(*pixels++); // TODO check for end of buffer
          nnextbits = 32;
        }

        /* shuffle new bits into place */
        currbits |= nextbits >> ncurrbits;
        if ((bitsused = MIN(nnextbits, 32 - ncurrbits)) < 32)
          nextbits <<= bitsused; /* discard used bits */
        nnextbits -= bitsused;
        ncurrbits += bitsused;
      }
      assert(ncurrbits >= bitsperchar);

      glyph_px = (currbits & mask) >> shift; /* extract high bits */
      currbits <<= bitsperchar; /* discard used bits */
      ncurrbits -= bitsperchar;

      /* glyph_px now has up to 16 2bpp pixels */
      /* now extract any (PIXEL_FG_IDX) pixels and repack them as monochrome */

      /* The following does the equivalent of:
       *   for (int i = 0; i < 16; i++)
       *     if (((px_out >> (i * 2)) & 3) == PIXEL_FG_IDX)
       *       monopixels |= (1u << i);
       */
      monopixels = (repacktab[(glyph_px >>  0) & 0xFF] <<  0) |
                   (repacktab[(glyph_px >>  8) & 0xFF] <<  4) |
                   (repacktab[(glyph_px >> 16) & 0xFF] <<  8) |
                   (repacktab[(glyph_px >> 24) & 0xFF] << 12);

      charoffset = (x / bmfont->gridwidth) * sizeofchar;
      offset     = rowoffset + lineoffset + charoffset;

      if (bmfont->glyphrowbytes == 1)
      {
        unsigned char *glyphs_as_uchar;
        glyphs_as_uchar  = (unsigned char *) glyphs + offset / sizeof(*glyphs_as_uchar);
        *glyphs_as_uchar = monopixels;
      }
      else
      {
        unsigned short *glyphs_as_ushort;
        glyphs_as_ushort  = (unsigned short *) glyphs + offset / sizeof(*glyphs_as_ushort);
        *glyphs_as_ushort = monopixels;
      }
    }
  }

  return rc;


oom:
  rc = result_OOM;
  return rc;
}

/* -------------------------------------------------------------------------- */

/**
 * Loads a PNG as a font.
 *
 * It should be (em_width*16) pixels wide. <PIXEL_WIDTH_IDX> pixels represent advance widths.
 */
result_t bmfont_create(const char *png, bmfont_t **pbmfont)
{
  result_t       rc;
  bmfont_t      *bmfont       = NULL;
  FILE          *fp;
  png_uint_32    pngwidth, pngheight;
  int            pngbitdepth, pngcolourtype;
  png_structp    png_ptr      = NULL;
  png_infop      info_ptr;
  png_bytep     *row_pointers = NULL;
  unsigned char *pixels       = NULL;

  *pbmfont = NULL;

  fp = fopen(png, "rb");
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
    rc = result_BAD_ARG;
    goto cleanup;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL)
  {
    rc = result_BAD_ARG;
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
#ifdef BMFONT_DEBUG
  fprintf(stderr, "bmfont load png: w=%d h=%d bit_depth=%d colour_type=%d\n",
          (int) width, (int) height, bit_depth, colour_type);
#endif

  /* we need a 2bpp paletted PNG */
  if (pngbitdepth != 2 || pngcolourtype != PNG_COLOR_TYPE_PALETTE)
  {
#ifdef BMFONT_DEBUG
    fprintf(stderr, "Incompatible PNG format: depth=%d, coltype=%d\n",
            bit_depth, colour_type);
#endif
    rc = result_INCOMPATIBLE;
    goto cleanup;
  }

  /* work out the font size */

  int gridwidth  = pngwidth / CHARS_PER_ROW;
  int gridheight = 0; /* to be determined */

  {
    int h;

    /* starting with gridwidth, increment until we find a height which evenly divides */
    for (h = gridwidth; h < gridwidth * 3; h++)
      if ((pngheight % h) == 0)
      {
        gridheight = h;
        break;
      }

    if (gridheight == 0)
    {
#ifdef BMFONT_DEBUG
      fprintf(stderr, "Can't determine font height\n");
#endif
      rc = result_BAD_ARG;
      goto cleanup;
    }
  }

  bmfont = calloc(1, sizeof(*bmfont));
  if (bmfont == NULL)
  {
    rc = result_OOM;
    goto cleanup;
  }

  bmfont->gridwidth     = gridwidth;      /* width of glyphs in grid */
  bmfont->gridheight    = gridheight;     /* height of glyphs in grid */
  bmfont->charwidth     = gridwidth;      /* currently same as gridwidth */
  bmfont->charheight    = gridheight - 1; /* -1 to account for advance width pixel row */
  bmfont->totalchars    = CHARS_PER_ROW * pngheight / gridheight;
  bmfont->glyphrowbytes = (gridwidth + 7) / 8; /* byte width of stored glyphs (1 or 2) */

#ifdef BMFONT_DEBUG
  fprintf(stderr, "bmfont load png: charwidth=%d charheight=%d totalchars=%d glyphrowbytes=%d\n",
          bmfont->charwidth, bmfont->charheight,
          bmfont->totalchars, bmfont->glyphrowbytes);
#endif

  {
    size_t      pngrowbytes;
    png_uint_32 h;

    pngrowbytes = png_get_rowbytes(png_ptr, info_ptr);
    pixels = malloc(pngrowbytes * pngheight);
    if (pixels == NULL)
      goto oom;

    row_pointers = malloc(pngheight * sizeof(*row_pointers));
    if (row_pointers == NULL)
      goto oom;

    for (h = 0; h < pngheight; h++)
      row_pointers[h] = pixels + pngrowbytes * h;

    png_read_image(png_ptr, row_pointers);

    rc = extract_advance_widths(bmfont, pixels, pngwidth, pngheight, pngrowbytes);
    if (rc)
      goto cleanup;

    rc = extract_glyphs(bmfont, pixels, pngwidth, pngheight, pngrowbytes);
    if (rc)
      goto cleanup;
  }

  *pbmfont = bmfont;

  rc = result_OK;

cleanup:
  free(row_pointers);
  free(pixels);
  if (rc)
    free(bmfont);
  if (png_ptr)
    png_destroy_read_struct(&png_ptr, NULL, NULL);
  fclose(fp);
  return rc;

oom:
  rc = result_OOM;
  goto cleanup;
}

void bmfont_destroy(bmfont_t *bmfont)
{
  free(bmfont);
}

/* -------------------------------------------------------------------------- */

void bmfont_info(bmfont_t *bmfont, int *width, int *height)
{
  *width  = bmfont->charwidth;
  *height = bmfont->charheight;
}

result_t bmfont_measure(bmfont_t       *bmfont,
                        const char     *text,
                        int             textlen,
                        bmfont_width_t  target_width,
                        int            *split_point,
                        bmfont_width_t *actual_width)
{
  bmfont_width_t current_width;
  int            len;

  assert(bmfont);
  assert(text);
  assert(textlen > 0);
  assert(target_width >= 0);
  assert(split_point);
  assert(actual_width);

  current_width = 0;
  for (len = textlen; len; len--)
  {
    int c;
    int gid;
    int advance;
    int next_width;

    if ((c = *text++) < ' ')
      continue;

    gid = c - ' ';
    advance = (gid < bmfont->totalchars) ? bmfont->adw[gid] : 0;

    next_width = current_width + advance;
    if (next_width > target_width)
      break;

    current_width = next_width;
  }

  if (split_point)
    *split_point  = textlen - len;
  if (actual_width)
    *actual_width = current_width;

  return result_OK;
}

/* -------------------------------------------------------------------------- */

/* bmfont_drawchar_<pixel format>_<width>w_<o/t> */

typedef void bmfont_drawchar_t(void        *vscreen,
                               const void  *vglyph,
                               int          top_skip,
                               int          right_skip,
                               int          shift,
                               int          stride,
                               int          charwidth,
                               int          charheight,
                               pixelfmt_x_t fg,
                               pixelfmt_x_t bg);

/* -------------------------------------------------------------------------- */

static void bmfont_drawchar_p4_1w_o(void        *vscreen,
                                    const void  *vglyph,
                                    int          top_skip,
                                    int          right_skip,
                                    int          shift,
                                    int          rowbytes,
                                    int          charwidth,
                                    int          charheight,
                                    pixelfmt_x_t fg,
                                    pixelfmt_x_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;

  assert(shift == 0 || shift == 4);
  assert(charwidth > 0);
  assert(charheight > 0);
  assert((unsigned int) fg < 16);
  assert((unsigned int) bg < 16);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr); // NOP

  /* build packed LUT 0xFFBFFBBB for inner loop (F=fg, B=bg) */
  unsigned int tab = (fg * 0x11011000) | (bg * 0x00100111);

  while (charheight--)
  {
    unsigned int row = *gly++; // 2 bytes wide font data
    row >>= right_skip; // compensate when right hand clipping

    unsigned char *scr2 = scr;
    int            cw   = charwidth;

    /* leading case */
    if (shift)
    {
      unsigned int px;

      px = (*scr2 & 0x0F) | ((row & (1u << (cw - 1))) ? fg : bg) << shift;
      *scr2++ = px;
      cw--;
    }

    /* whole bytes */
    switch (cw)
    {
    case  7: *scr2++ = tab >> (((row >>  5) & 3) << 3);
    case  5: *scr2++ = tab >> (((row >>  3) & 3) << 3);
    case  3: *scr2++ = tab >> (((row >>  1) & 3) << 3);
    /* trailing case */
    case  1: *scr2 = (((row & (1u << 0)) ? fg : bg) << 0) | (*scr2 & 0xF0);
      break;

    case  8: *scr2++ = tab >> (((row >>  6) & 3) << 3);
    case  6: *scr2++ = tab >> (((row >>  4) & 3) << 3);
    case  4: *scr2++ = tab >> (((row >>  2) & 3) << 3);
    case  2: *scr2++ = tab >> (((row >>  0) & 3) << 3);
      break;
    }

    scr += stride;
  }
}

static void bmfont_drawchar_p4_1w_t(void        *vscreen,
                                    const void  *vglyph,
                                    int          top_skip,
                                    int          right_skip,
                                    int          shift,
                                    int          rowbytes,
                                    int          charwidth,
                                    int          charheight,
                                    pixelfmt_x_t fg,
                                    pixelfmt_x_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;

  assert(shift == 0 || shift == 4);
  assert(charwidth > 0);
  assert(charheight > 0);
  assert((unsigned int) fg < 16);
  assert((unsigned int) bg < 16);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr); // NOP

  /* build packed LUTs for inner loop */
  unsigned int fgtab = fg * 0x11011000;
  unsigned int bgtab =      0x00F00FFF;

  while (charheight--)
  {
    unsigned int row = *gly++; // 2 bytes wide font data
    row >>= right_skip; // compensate when right hand clipping

    unsigned char *scr2 = scr;
    int            cw   = charwidth;
    unsigned int   ts; /* tab shift */
    unsigned int   px;

    /* leading case */
    if (shift)
    {
      if (row & (1u << (cw - 1)))
        *scr2 = (*scr2 & 0x0F) | (fg << shift);
      scr2++;
      cw--;
    }

    /* whole bytes */
    switch (cw)
    {
    case  7: ts = (((row >>  5) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  5: ts = (((row >>  3) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  3: ts = (((row >>  1) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    /* trailing case */
    case  1:
      if (row & (1u << 0))
        *scr2 = (fg << 0) | (*scr2 & 0xF0);
      break;

    case  8: ts = (((row >>  6) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  6: ts = (((row >>  4) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  4: ts = (((row >>  2) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  2: ts = (((row >>  0) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
      break;
    }

    scr += stride;
  }
}

static void bmfont_drawchar_p4_2w_o(void        *vscreen,
                                    const void  *vglyph,
                                    int          top_skip,
                                    int          right_skip,
                                    int          shift,
                                    int          rowbytes,
                                    int          charwidth,
                                    int          charheight,
                                    pixelfmt_x_t fg,
                                    pixelfmt_x_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;

  assert(shift == 0 || shift == 4);
  assert(charwidth > 0);
  assert(charheight > 0);
  assert((unsigned int) fg < 16);
  assert((unsigned int) bg < 16);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr); // NOP

  /* build packed LUT 0xFFBFFBBB for inner loop (F=fg, B=bg) */
  unsigned int tab = (fg * 0x11011000) | (bg * 0x00100111);

  while (charheight--)
  {
    unsigned int row = *gly++; // 2 bytes wide font data
    row >>= right_skip; // compensate when right hand clipping

    unsigned char *scr2 = scr;
    int            cw   = charwidth;

    /* leading case */
    if (shift)
    {
      unsigned int px;

      px = (*scr2 & 0x0F) | ((row & (1u << (cw - 1))) ? fg : bg) << shift;
      *scr2++ = px;
      cw--;
    }

    /* whole bytes */
    switch (cw)
    {
    case 15: *scr2++ = tab >> (((row >> 13) & 3) << 3);
    case 13: *scr2++ = tab >> (((row >> 11) & 3) << 3);
    case 11: *scr2++ = tab >> (((row >>  9) & 3) << 3);
    case  9: *scr2++ = tab >> (((row >>  7) & 3) << 3);
    case  7: *scr2++ = tab >> (((row >>  5) & 3) << 3);
    case  5: *scr2++ = tab >> (((row >>  3) & 3) << 3);
    case  3: *scr2++ = tab >> (((row >>  1) & 3) << 3);
    /* trailing case */
    case  1: *scr2 = (((row & (1u << 0)) ? fg : bg) << 0) | (*scr2 & 0xF0);
      break;

    case 14: *scr2++ = tab >> (((row >> 12) & 3) << 3);
    case 12: *scr2++ = tab >> (((row >> 10) & 3) << 3);
    case 10: *scr2++ = tab >> (((row >>  8) & 3) << 3);
    case  8: *scr2++ = tab >> (((row >>  6) & 3) << 3);
    case  6: *scr2++ = tab >> (((row >>  4) & 3) << 3);
    case  4: *scr2++ = tab >> (((row >>  2) & 3) << 3);
    case  2: *scr2++ = tab >> (((row >>  0) & 3) << 3);
      break;
    }

    scr += stride;
  }
}

static void bmfont_drawchar_p4_2w_t(void        *vscreen,
                                    const void  *vglyph,
                                    int          top_skip,
                                    int          right_skip,
                                    int          shift,
                                    int          rowbytes,
                                    int          charwidth,
                                    int          charheight,
                                    pixelfmt_x_t fg,
                                    pixelfmt_x_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;

  assert(shift == 0 || shift == 4);
  assert(charwidth > 0);
  assert(charheight > 0);
  assert((unsigned int) fg < 16);
  assert((unsigned int) bg < 16);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr); // NOP

  /* build packed LUTs for inner loop */
  unsigned int fgtab = fg * 0x11011000;
  unsigned int bgtab =      0x00F00FFF;

  while (charheight--)
  {
    unsigned int row = *gly++; // 2 bytes wide font data
    row >>= right_skip; // compensate when right hand clipping

    unsigned char *scr2 = scr;
    int            cw   = charwidth;
    unsigned int   ts; /* tab shift */
    unsigned int   px;

    /* leading case */
    if (shift)
    {
      if (row & (1u << (cw - 1)))
        *scr2 = (*scr2 & 0x0F) | (fg << shift);
      scr2++;
      cw--;
    }

    /* whole bytes */
    switch (cw)
    {
    case 15: ts = (((row >> 13) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 13: ts = (((row >> 11) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 11: ts = (((row >>  9) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  9: ts = (((row >>  7) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  7: ts = (((row >>  5) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  5: ts = (((row >>  3) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  3: ts = (((row >>  1) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    /* trailing case */
    case  1:
      if (row & (1u << 0))
        *scr2 = (fg << 0) | (*scr2 & 0xF0);
      break;

    case 14: ts = (((row >> 12) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 12: ts = (((row >> 10) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 10: ts = (((row >>  8) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  8: ts = (((row >>  6) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  6: ts = (((row >>  4) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  4: ts = (((row >>  2) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case  2: ts = (((row >>  0) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
      break;
    }

    scr += stride;
  }
}

/* -------------------------------------------------------------------------- */

/* draw a character up to byte wide, opaque */
static void bmfont_drawchar_any8888_1w_o(void        *vscreen,
                                         const void  *vglyph,
                                         int          top_skip,
                                         int          right_skip,
                                         int          shift,
                                         int          rowbytes,
                                         int          charwidth,
                                         int          charheight,
                                         pixelfmt_x_t fg,
                                         pixelfmt_x_t bg)
{
  pixelfmt_bgrx8888_t *scr = vscreen;
  const unsigned char *gly = vglyph;

  NOT_USED(shift);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr) - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; // 1 byte wide font data
    row >>= right_skip; // compensate when right hand clipping

    switch (charwidth) { // jump table test
      case 8: *scr++ = (row & (1u << 7)) ? fg : bg; // fallthrough!
      case 7: *scr++ = (row & (1u << 6)) ? fg : bg;
      case 6: *scr++ = (row & (1u << 5)) ? fg : bg;
      case 5: *scr++ = (row & (1u << 4)) ? fg : bg;
      case 4: *scr++ = (row & (1u << 3)) ? fg : bg;
      case 3: *scr++ = (row & (1u << 2)) ? fg : bg;
      case 2: *scr++ = (row & (1u << 1)) ? fg : bg;
      case 1: *scr++ = (row & (1u << 0)) ? fg : bg;
    }

    scr += stride;
  }
}

/* draw a character up to byte wide, transparent */
static void bmfont_drawchar_any8888_1w_t(void        *vscreen,
                                         const void  *vglyph,
                                         int          top_skip,
                                         int          right_skip,
                                         int          shift,
                                         int          rowbytes,
                                         int          charwidth,
                                         int          charheight,
                                         pixelfmt_x_t fg,
                                         pixelfmt_x_t bg)
{
  pixelfmt_bgrx8888_t *scr = vscreen;
  const unsigned char *gly = vglyph;

  NOT_USED(shift);
  NOT_USED(bg);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr) - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; // 1 byte wide font data
    row >>= right_skip; // compensate when right hand clipping

    switch (charwidth) { // jump table test
      case 8: if (row & (1u << 7)) *scr = fg; scr++; // fallthrough!
      case 7: if (row & (1u << 6)) *scr = fg; scr++;
      case 6: if (row & (1u << 5)) *scr = fg; scr++;
      case 5: if (row & (1u << 4)) *scr = fg; scr++;
      case 4: if (row & (1u << 3)) *scr = fg; scr++;
      case 3: if (row & (1u << 2)) *scr = fg; scr++;
      case 2: if (row & (1u << 1)) *scr = fg; scr++;
      case 1: if (row & (1u << 0)) *scr = fg; scr++;
    }

    scr += stride;
  }
}

/* draw a character up to two bytes wide, opaque */
static void bmfont_drawchar_any8888_2w_o(void        *vscreen,
                                         const void  *vglyph,
                                         int          top_skip,
                                         int          right_skip,
                                         int          shift,
                                         int          rowbytes,
                                         int          charwidth,
                                         int          charheight,
                                         pixelfmt_x_t fg,
                                         pixelfmt_x_t bg)
{
  pixelfmt_bgrx8888_t  *scr = vscreen;
  const unsigned short *gly = vglyph;

  NOT_USED(shift);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr) - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; // 2 bytes wide font data
    row >>= right_skip; // compensate when right hand clipping

    switch (charwidth) { // jump table test
      case 16: *scr++ = (row & (1u << 15)) ? fg : bg; // fallthrough!
      case 15: *scr++ = (row & (1u << 14)) ? fg : bg;
      case 14: *scr++ = (row & (1u << 13)) ? fg : bg;
      case 13: *scr++ = (row & (1u << 12)) ? fg : bg;
      case 12: *scr++ = (row & (1u << 11)) ? fg : bg;
      case 11: *scr++ = (row & (1u << 10)) ? fg : bg;
      case 10: *scr++ = (row & (1u <<  9)) ? fg : bg;
      case  9: *scr++ = (row & (1u <<  8)) ? fg : bg;
      case  8: *scr++ = (row & (1u <<  7)) ? fg : bg;
      case  7: *scr++ = (row & (1u <<  6)) ? fg : bg;
      case  6: *scr++ = (row & (1u <<  5)) ? fg : bg;
      case  5: *scr++ = (row & (1u <<  4)) ? fg : bg;
      case  4: *scr++ = (row & (1u <<  3)) ? fg : bg;
      case  3: *scr++ = (row & (1u <<  2)) ? fg : bg;
      case  2: *scr++ = (row & (1u <<  1)) ? fg : bg;
      case  1: *scr++ = (row & (1u <<  0)) ? fg : bg;
    }

    scr += stride;
  }
}

/* draw a character up to two bytes wide, transparent */
static void bmfont_drawchar_any8888_2w_t(void        *vscreen,
                                         const void  *vglyph,
                                         int          top_skip,
                                         int          right_skip,
                                         int          shift,
                                         int          rowbytes,
                                         int          charwidth,
                                         int          charheight,
                                         pixelfmt_x_t fg,
                                         pixelfmt_x_t bg)
{
  pixelfmt_bgrx8888_t  *scr = vscreen;
  const unsigned short *gly = vglyph;

  NOT_USED(shift);
  NOT_USED(bg);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr) - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; // 2 bytes wide font data
    row >>= right_skip; // compensate when right hand clipping

    switch (charwidth) { // jump table test
      case 16: if (row & (1u << 15)) *scr = fg; scr++; // fallthrough!
      case 15: if (row & (1u << 14)) *scr = fg; scr++;
      case 14: if (row & (1u << 13)) *scr = fg; scr++;
      case 13: if (row & (1u << 12)) *scr = fg; scr++;
      case 12: if (row & (1u << 11)) *scr = fg; scr++;
      case 11: if (row & (1u << 10)) *scr = fg; scr++;
      case 10: if (row & (1u <<  9)) *scr = fg; scr++;
      case  9: if (row & (1u <<  8)) *scr = fg; scr++;
      case  8: if (row & (1u <<  7)) *scr = fg; scr++;
      case  7: if (row & (1u <<  6)) *scr = fg; scr++;
      case  6: if (row & (1u <<  5)) *scr = fg; scr++;
      case  5: if (row & (1u <<  4)) *scr = fg; scr++;
      case  4: if (row & (1u <<  3)) *scr = fg; scr++;
      case  3: if (row & (1u <<  2)) *scr = fg; scr++;
      case  2: if (row & (1u <<  1)) *scr = fg; scr++;
      case  1: if (row & (1u <<  0)) *scr = fg; scr++;
    }

    scr += stride;
  }
}

result_t bmfont_draw(bmfont_t      *bmfont,
                     screen_t      *scr,
                     const char    *text,
                     int            len,
                     colour_t       fg,
                     colour_t       bg,
                     const point_t *pos,
                     point_t       *end_pos)
{
  bmfont_drawchar_t *drawfn;
  box_t              scrclip;
  box_t              drawbox;
  box_t              drawclip;

  unsigned int bgalpha = colour_get_alpha(&bg);

  switch (scr->format)
  {
  case pixelfmt_p4:
    switch (bmfont->glyphrowbytes)
    {
    case 1: drawfn = (bgalpha < 255) ? bmfont_drawchar_p4_1w_t : bmfont_drawchar_p4_1w_o; break;
    case 2: drawfn = (bgalpha < 255) ? bmfont_drawchar_p4_2w_t : bmfont_drawchar_p4_2w_o; break;
    default: assert(0); return result_BAD_ARG;
    }
    break;
  case pixelfmt_bgra8888:
  case pixelfmt_bgrx8888:
    switch (bmfont->glyphrowbytes)
    {
    case 1: drawfn = (bgalpha < 255) ? bmfont_drawchar_any8888_1w_t : bmfont_drawchar_any8888_1w_o; break;
    case 2: drawfn = (bgalpha < 255) ? bmfont_drawchar_any8888_2w_t : bmfont_drawchar_any8888_2w_o; break;
    default: assert(0); return result_BAD_ARG;
    }
    break;
  default:
    assert(0); return result_BAD_ARG;
  }

  scrclip = screen_get_clip(scr);

  drawbox.x0 = pos->x;
  drawbox.y0 = pos->y;
  drawbox.x1 = pos->x + bmfont->charwidth * len; /* worst-case estimate */
  drawbox.y1 = pos->y + bmfont->charheight;

  if (!box_intersects(&scrclip, &drawbox))
    return result_OK; /* not visible */

  box_clipped(&scrclip, &drawbox, &drawclip);

  int            left_skip         = drawclip.x0;
  int            top_skip          = drawclip.y0;
  /* note: drawclip.x1 doesn't get used */
  int            bottom_skip       = drawclip.y1;

  /* ensure that any computed positions are inside the clip box */
  unsigned int   clamped_pos_x     = CLAMP(pos->x, scrclip.x0, scrclip.x1 - 1);
  unsigned int   clamped_pos_y     = CLAMP(pos->y, scrclip.y0, scrclip.y1 - 1);

  int            log2bpp           = pixelfmt_log2bpp(scr->format);
  unsigned char *screen            = (unsigned char *) scr->base + clamped_pos_y * scr->rowbytes + ((clamped_pos_x << log2bpp) >> 3);
  int            log2pixperb       = MAX(0, 3 - log2bpp); /* log2 pixels per byte */
  int            shift              = (clamped_pos_x & ((1 << log2pixperb) - 1)) << log2bpp;

  int            rowbytes          = scr->rowbytes;
  int            charwidth         = bmfont->charwidth;
  int            charheight        = bmfont->charheight;
  int            clippedcharheight = charheight - top_skip - bottom_skip;
  int            glyphbytes        = bmfont->glyphrowbytes * charheight;
  pixelfmt_x_t   nativefg          = colour_to_pixel(scr->palette, 1 << (1 << log2bpp), fg, scr->format);
  pixelfmt_x_t   nativebg          = colour_to_pixel(scr->palette, 1 << (1 << log2bpp), bg, scr->format);
  int            remaining         = scrclip.x1 - clamped_pos_x; /* in pixels */

  const int      tracking         = 0; // +ve can break stuff!

  assert(clippedcharheight > 0);

  while (len--)
  {
    int         c;
    int         gid;
    int         advance;
    const void *glyph;

    c       = *text++;
    gid     = c - ' ';
    advance = bmfont->adw[gid] + tracking;

    /* skip characters until we can plot */
    if (left_skip > advance)
    {
      left_skip -= advance;
      continue;
    }

    /* draw */
    {
      glyph = (unsigned char *) bmfont->glyphs + gid * glyphbytes;

      int remainingcharwidth = MIN(remaining, advance); /* in pixels */
      int right_skip         = charwidth - remainingcharwidth;
      int clippedcharwidth   = remainingcharwidth - left_skip;

      if (clippedcharwidth > 0)
        drawfn(screen, glyph,
               top_skip, right_skip,
               shift, rowbytes,
               clippedcharwidth, clippedcharheight,
               nativefg, nativebg);

      int pixelsused_bits = (advance - left_skip) << log2bpp;
      int nbits           = shift + pixelsused_bits;
      screen = (unsigned char *) screen + (nbits >> 3);
      shift  = nbits & 7;

      remaining -= advance;
      if (remainingcharwidth < advance)
        len = 0; /* stop drawing */
    }

    left_skip = 0;
  }

  if (end_pos)
    *end_pos = (point_t) { 42, pos->y }; // TODO: screen -> coord and clamp?

  return result_OK;
}

/* -------------------------------------------------------------------------- */

/* vim: set ts=8 sts=2 sw=2 et: */
