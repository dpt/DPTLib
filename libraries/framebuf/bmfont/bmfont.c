/* framebuf/bmfont/bmfont.c */

/* TODOS / IDEAS
 *
 * (1) Instead of relying on hard-coded pixel values for the font input format
 *     we could use the palette to determine the correct values.
 * (2) Support for sidebearings
 * (3) Support for kerning
 * (4) Could try to make the character plotters word-oriented rather than byte.
 */

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/bmfont.h"
#include "utils/array.h"
#include "utils/bytesex.h"

/* -------------------------------------------------------------------------- */

/* Configuration */

/* Input font PNGs must have 32 characters per row */
#define CHARS_PER_ROW   (32)

/* Input advance widths are encoded 1px short of the true glyph advance; the
 * missing pixel is letter spacing, added back in at use, not storage. */
#define LETTER_SPACING  (1)

/* -------------------------------------------------------------------------- */

/* Pixel values */
#define PIXEL_BG_IDX    (0) /* background */
#define PIXEL_FG_IDX    (1) /* font */
#define PIXEL_WIDTH_IDX (2) /* advance widths */
#define PIXEL_GRID_IDX  (3) /* grid: left sidebearing (cosmetic), baseline and
                             * cell-bottom rows (read back for ascent/descent) */

/* -------------------------------------------------------------------------- */

struct bmfont
{
  png_uint_32     gridwidth, gridheight; /* pixels */
  png_uint_32     charwidth, charheight; /* em size in pixels */
  int             ascent;  /* baseline offset from the top of a glyph cell */
  int             descent; /* offset from the baseline to the cell bottom */
  int             totalchars;

  void           *glyphs;
  int             glyphrowbytes;

  bmfont_width_t *adw; /* an array of length totalchars */
  int             adw_used;
  int             adw_allocated;
  bmfont_width_t  maxadw; /* widest advance width, for monospaced mode */

  bmfont_flags_t  flags;
};

/* -------------------------------------------------------------------------- */

/** The advance width to use for glyph \p gid, honouring the monospace flag
 *  and any caller-supplied extra letter/word spacing. Includes the
 *  LETTER_SPACING pixel not stored in the font. */
static bmfont_width_t bmfont_advance_for(const bmfont_t         *bmfont,
                                         int                     gid,
                                         int                     c,
                                         const bmfont_spacing_t *spacing)
{
  bmfont_width_t adw;

  adw = (bmfont->flags & bmfont_FLAG_MONOSPACE) ? bmfont->maxadw :
                                                  bmfont->adw[gid];
  adw += LETTER_SPACING;

  if (spacing)
  {
    adw += spacing->letter_spacing;
    if (c == ' ')
      adw += spacing->word_spacing;
  }

  return adw;
}

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
static int count_adw(unsigned char tab[256], uint64_t adw_px)
{
  return tab[(adw_px >>  0) & 0xFF] +
         tab[(adw_px >>  8) & 0xFF] +
         tab[(adw_px >> 16) & 0xFF] +
         tab[(adw_px >> 24) & 0xFF] +
         tab[(adw_px >> 32) & 0xFF] +
         tab[(adw_px >> 40) & 0xFF] +
         tab[(adw_px >> 48) & 0xFF] +
         tab[(adw_px >> 56) & 0xFF];
}

/* -------------------------------------------------------------------------- */

/** True if row \p y of the 2bpp image holds at least one pixel of \p value.
 *  Pixels are packed four to a byte, most significant pair first. */
static int row_has_pixel(const unsigned char *pixels,
                         size_t               rowbytes,
                         png_uint_32          y,
                         png_uint_32          width,
                         int                  value)
{
  const unsigned char *row = pixels + rowbytes * y;
  png_uint_32          x;

  for (x = 0; x < width; x++)
  {
    int px = (row[x >> 2] >> (6 - 2 * (x & 3))) & 3;
    if (px == value)
      return 1;
  }

  return 0;
}

/**
 * Detect the grid cell height from the decoded pixels.
 *
 * The only unknown in the format is the cell height: 32 cells per row, each
 * an advance-width strip row on top of the glyph rows. A strip row carries
 * PIXEL_WIDTH_IDX pixels and no glyph ink (PIXEL_FG_IDX); the glyph rows
 * carry no PIXEL_WIDTH_IDX. A candidate height is correct iff it divides the
 * image and every row it implies matches its role. Returns 0 if nothing fits.
 */
static int detect_gridheight(const unsigned char *pixels,
                             size_t               rowbytes,
                             png_uint_32          width,
                             png_uint_32          height)
{
  int gridwidth = (int) (width / CHARS_PER_ROW);
  int gh;

  /* a cell is at least two rows (one strip, one glyph) and no taller than
   * a generous multiple of its width */
  for (gh = 2; gh <= gridwidth * 3 && (png_uint_32) gh <= height; gh++)
  {
    png_uint_32 y;
    int         ok = 1;

    if ((height % (png_uint_32) gh) != 0)
      continue;

    for (y = 0; y < height && ok; y++)
    {
      int has_adw = row_has_pixel(pixels, rowbytes, y, width, PIXEL_WIDTH_IDX);
      int has_ink = row_has_pixel(pixels, rowbytes, y, width, PIXEL_FG_IDX);

      if ((y % (png_uint_32) gh) == 0)
        ok = has_adw && !has_ink; /* strip row */
      else
        ok = !has_adw;            /* glyph row */
    }

    if (ok)
      return gh;
  }

  return 0;
}

/** True if row \p y of the cell is full-width PIXEL_GRID_IDX. */
static int row_is_grid(const unsigned char *pixels,
                       size_t               rowbytes,
                       png_uint_32          gridwidth,
                       png_uint_32          y)
{
  const unsigned char *row = pixels + rowbytes * y;
  png_uint_32          x;

  for (x = 0; x < gridwidth; x++)
  {
    int px = (row[x >> 2] >> (6 - 2 * (x & 3))) & 3;
    if (px != PIXEL_GRID_IDX)
      return 0;
  }

  return 1;
}

/**
 * Find the baseline and descent within a glyph cell.
 *
 * The space glyph (grid cell 0, gid 0) carries no ink, so its grid rows --
 * drawn by ttf2bmfont.py as full-width rows of PIXEL_GRID_IDX pixels across
 * the cell, one at the baseline and one at the cell bottom -- are
 * unambiguous. The first such row within the cell body is the baseline
 * (*ascent is its offset from the top of the cell); the next one found at or
 * below it is the cell bottom (*descent is its offset from the baseline).
 * Returns 0 if no baseline row is found, e.g. a font predating this
 * convention -- the caller falls back to treating the whole cell body as
 * ascent, with no descender.
 */
static int detect_baseline_metrics(const unsigned char *pixels,
                                   size_t               rowbytes,
                                   png_uint_32          gridwidth,
                                   png_uint_32          gridheight,
                                   int                 *ascent,
                                   int                 *descent)
{
  png_uint_32 y;
  int         baseline_y = -1;

  for (y = 1; y < gridheight; y++) /* row 0 is the advance-width strip */
  {
    if (!row_is_grid(pixels, rowbytes, gridwidth, y))
      continue;

    if (baseline_y < 0)
    {
      baseline_y = (int) y;
      continue;
    }

    *ascent  = baseline_y - 1; /* -1: ascent excludes the strip row */
    *descent = (int) y - baseline_y;
    return 1;
  }

  return 0;
}

/** Verify the font format and build the advance width table. */
static result_t extract_advance_widths(bmfont_t   *bmfont,
                                       void       *voidpixels,
                                       png_uint_32 imgwidth,
                                       png_uint_32 imgheight,
                                       size_t      rowbytes)
{
  result_t      rc = result_OK;
  unsigned char adwtab[256];
  unsigned int *pixels;
  unsigned int *pixels_end;
  int           bitsperchar;
  uint64_t      mask;
  png_uint_32   y;
  int           i;

  assert(bmfont);
  assert(voidpixels);
  assert(imgwidth  > 0);
  assert(imgheight > 0);
  assert(rowbytes  > 0);

  build_adw_tab(adwtab, PIXEL_WIDTH_IDX);

  pixels      = (unsigned int *) voidpixels;
  pixels_end  = pixels + (rowbytes * imgheight) / sizeof(*pixels);
  bitsperchar = bmfont->charwidth * 2; /* 2 because 2bpp */
  mask        = 0xFFFFFFFFFFFFFFFFull << (64 - bitsperchar);

  assert(bitsperchar <= 64);

  for (y = 0; y < imgheight; y += bmfont->gridheight)
  {
    uint64_t    currbits;
    int         ncurrbits;
    uint64_t    nextbits;
    int         nnextbits;
    png_uint_32 x;
    uint64_t    adw_px;

    /* maintain two words, a current and a pending so we've always got enough
     * bits ready */
    if (pixels >= pixels_end)
      return result_PARSE_ERROR; /* malformed grid: row starts past the buffer */
    currbits  = (uint64_t) rev_l(*pixels++) << 32;
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
          if (pixels >= pixels_end)
            return result_PARSE_ERROR; /* malformed grid: row overruns the buffer */
          nextbits  = (uint64_t) rev_l(*pixels++) << 32;
          nnextbits = 32;
        }

        /* shuffle new bits into place */
        currbits |= nextbits >> ncurrbits;
        if ((bitsused = MIN(nnextbits, 64 - ncurrbits)) < 32)
          nextbits <<= bitsused; /* discard used bits */
        nnextbits -= bitsused;
        ncurrbits += bitsused;
      }
      assert(ncurrbits >= bitsperchar);

      adw_px = currbits & mask; /* extract high bits */
      currbits = (bitsperchar == 64) ? 0 : currbits << bitsperchar; /* discard used bits */
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

    /* a malformed grid can walk pixels outside the decoded image; refuse it
     * rather than read past the buffer on the next row */
    if (pixels < (unsigned int *) voidpixels ||
        pixels > (unsigned int *) voidpixels + (rowbytes * imgheight / 4))
      return result_PARSE_ERROR;
  }

  bmfont->maxadw = 0;
  for (i = 0; i < bmfont->adw_used; i++)
    if (bmfont->adw[i] > bmfont->maxadw)
      bmfont->maxadw = bmfont->adw[i];

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

static result_t extract_glyphs(bmfont_t   *bmfont,
                               void       *voidpixels,
                               png_uint_32 imgwidth,
                               png_uint_32 imgheight,
                               size_t      rowbytes)
{
  result_t       rc = result_OK;
  unsigned char  repacktab[256];
  size_t         glyphbytes;
  unsigned char *glyphs;
  unsigned int  *pixels;
  int            bitsperchar;
  int            shift;
  uint64_t       mask;
  int            sizeofchar;
  int            sizeofrow;
  png_uint_32    y;

  NOT_USED(rowbytes);

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
  shift       = 64 - bitsperchar;
  mask        = 0xFFFFFFFFFFFFFFFFull << shift;

  assert(bitsperchar <= 64);

  sizeofchar  = bmfont->glyphrowbytes * bmfont->charheight;
  sizeofrow   = CHARS_PER_ROW * sizeofchar;

  for (y = 0; y < imgheight; y++)
  {
    uint64_t    currbits;
    int         ncurrbits;
    uint64_t    nextbits;
    int         nnextbits;
    int         rowoffset;
    int         lineoffset;
    png_uint_32 x;
    uint64_t    glyph_px;

    /* skip those rows that contain advance widths */
    if ((y % bmfont->gridheight) == 0)
    {
      pixels += imgwidth / (32 / 2);
      continue;
    }

    /* maintain two words, a current and a pending so we've always got enough
     * bits ready */
    currbits  = (uint64_t) rev_l(*pixels++) << 32;
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
          nextbits  = (uint64_t) rev_l(*pixels++) << 32; // TODO: Check for end of buffer
          nnextbits = 32;
        }

        /* shuffle new bits into place */
        currbits |= nextbits >> ncurrbits;
        if ((bitsused = MIN(nnextbits, 64 - ncurrbits)) < 32)
          nextbits <<= bitsused; /* discard used bits */
        nnextbits -= bitsused;
        ncurrbits += bitsused;
      }
      assert(ncurrbits >= bitsperchar);

      glyph_px = (currbits & mask) >> shift; /* extract high bits */
      currbits = (bitsperchar == 64) ? 0 : currbits << bitsperchar; /* discard used bits */
      ncurrbits -= bitsperchar;

      /* glyph_px now has up to 32 2bpp pixels */
      /* now extract any (PIXEL_FG_IDX) pixels and repack them as monochrome */

      /* The following does the equivalent of:
       *   for (int i = 0; i < 32; i++)
       *     if (((px_out >> (i * 2)) & 3) == PIXEL_FG_IDX)
       *       monopixels |= (1u << i);
       */
      monopixels = (repacktab[(glyph_px >>  0) & 0xFF] <<  0) |
                   (repacktab[(glyph_px >>  8) & 0xFF] <<  4) |
                   (repacktab[(glyph_px >> 16) & 0xFF] <<  8) |
                   (repacktab[(glyph_px >> 24) & 0xFF] << 12) |
                   (repacktab[(glyph_px >> 32) & 0xFF] << 16) |
                   (repacktab[(glyph_px >> 40) & 0xFF] << 20) |
                   (repacktab[(glyph_px >> 48) & 0xFF] << 24) |
                   (repacktab[(glyph_px >> 56) & 0xFF] << 28);

      charoffset = (x / bmfont->gridwidth) * sizeofchar;
      offset     = rowoffset + lineoffset + charoffset;

      if (bmfont->glyphrowbytes == 1)
      {
        unsigned char *glyphs_as_uchar;
        glyphs_as_uchar  = (unsigned char *) glyphs + offset / sizeof(*glyphs_as_uchar);
        *glyphs_as_uchar = (unsigned char) monopixels;
      }
      else if (bmfont->glyphrowbytes == 2)
      {
        unsigned short *glyphs_as_ushort;
        glyphs_as_ushort  = (unsigned short *) glyphs + offset / sizeof(*glyphs_as_ushort);
        *glyphs_as_ushort = (unsigned short) monopixels;
      }
      else
      {
        unsigned int *glyphs_as_uint;
        glyphs_as_uint  = (unsigned int *) glyphs + offset / sizeof(*glyphs_as_uint);
        *glyphs_as_uint = monopixels;
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
 * It should be (em_width*32) pixels wide, em_width up to 32px. <PIXEL_WIDTH_IDX> pixels represent advance widths.
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
    logf_error("bmfont_create: cannot open \"%s\"", png);
    rc = result_FILE_NOT_FOUND;
    goto cleanup;
  }

  if (!file_is_png(fp))
  {
    logf_error("bmfont_create: \"%s\" is not a PNG", png);
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
    rc = result_OOM;
    goto cleanup;
  }

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    rc = result_OOM;
    goto cleanup;
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, PNGSIGLEN);
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr,
              &pngwidth, &pngheight, &pngbitdepth, &pngcolourtype,
               NULL, NULL, NULL);
  logf_info("bmfont load png: w=%d h=%d bit_depth=%d colour_type=%d",
            (int) pngwidth, (int) pngheight, pngbitdepth, pngcolourtype);

  /* we need a 2bpp paletted PNG */
  if (pngbitdepth != 2 || pngcolourtype != PNG_COLOR_TYPE_PALETTE)
  {
    logf_error("bmfont: incompatible PNG format: depth=%d, coltype=%d",
               pngbitdepth, pngcolourtype);
    rc = result_INCOMPATIBLE;
    goto cleanup;
  }

  /* decode the image; the size detector needs the pixels */

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
  }

  /* work out the font size */

  {
    size_t pngrowbytes = png_get_rowbytes(png_ptr, info_ptr);
    int    gridwidth   = pngwidth / CHARS_PER_ROW;
    int    gridheight  = detect_gridheight(pixels, pngrowbytes,
                                           pngwidth, pngheight);

    if (gridheight == 0)
    {
      logf_error("%s", "bmfont: can't determine font height");
      rc = result_BAD_ARG;
      goto cleanup;
    }

    if (gridwidth > CHARS_PER_ROW)
    {
      logf_error("bmfont: character width %d exceeds the maximum of %d",
                 gridwidth, CHARS_PER_ROW);
      rc = result_BAD_ARG;
      goto cleanup;
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
    bmfont->charheight    = gridheight - 1; /* -1 for the advance width row */
    bmfont->totalchars    = CHARS_PER_ROW * pngheight / gridheight;
    bmfont->glyphrowbytes = gridwidth <=  8 ? 1 :
                             gridwidth <= 16 ? 2 :
                                               4; /* stored glyph row: 1, 2 or 4 */
    if (!detect_baseline_metrics(pixels, pngrowbytes,
                                 (png_uint_32) gridwidth,
                                 (png_uint_32) gridheight,
                                 &bmfont->ascent, &bmfont->descent))
    {
      bmfont->ascent  = bmfont->charheight; /* no grid row: full height, no
                                             * descender */
      bmfont->descent = 0;
    }

    logf_info("bmfont load png: charwidth=%d charheight=%d ascent=%d "
              "descent=%d totalchars=%d glyphrowbytes=%d",
              bmfont->charwidth, bmfont->charheight, bmfont->ascent,
              bmfont->descent, bmfont->totalchars, bmfont->glyphrowbytes);

    rc = extract_advance_widths(bmfont, pixels, pngwidth, pngheight,
                                pngrowbytes);
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

void bmfont_set_flags(bmfont_t *bmfont, bmfont_flags_t flags)
{
  assert(bmfont);

  bmfont->flags = flags;
}

void bmfont_get_info(bmfont_t *bmfont,
                     int      *width,
                     int      *height,
                     int      *ascent,
                     int      *descent)
{
  if (width)
    *width   = bmfont->charwidth;
  if (height)
    *height  = bmfont->charheight;
  if (ascent)
    *ascent  = bmfont->ascent;
  if (descent)
    *descent = bmfont->descent;
}

int bmfont_get_count(bmfont_t *bmfont)
{
  return bmfont->totalchars;
}

result_t bmfont_measure(bmfont_t               *bmfont,
                        const char             *text,
                        int                     textlen,
                        const bmfont_spacing_t *spacing,
                        bmfont_width_t          target_width,
                        int                    *split_point,
                        bmfont_width_t         *actual_width)
{
  bmfont_width_t current_width;
  int            len;
  int            any_drawn;
  int            last_c;

  assert(bmfont);
  assert(text);
  assert(textlen > 0);
  assert(target_width >= 0);
  /* split_point, actual_width may be NULL */

  current_width = 0;
  any_drawn      = 0;
  last_c         = 0;
  for (len = textlen; len; len--)
  {
    int c;
    int gid;
    int advance;
    int next_width;

    if ((c = (unsigned char) *text++) < ' ')
      continue;

    gid = c - ' ';
    advance = (gid < bmfont->totalchars) ?
              bmfont_advance_for(bmfont, gid, c, spacing) : 0;

    next_width = current_width + advance;
    if (next_width > target_width)
      break;

    current_width = next_width;
    any_drawn      = 1;
    last_c         = c;
  }

  if (split_point)
    *split_point  = textlen - len;
  if (actual_width)
  {
    bmfont_width_t trailing_trim = 0;

    if (any_drawn)
    {
      trailing_trim = LETTER_SPACING;
      if (spacing)
      {
        trailing_trim += spacing->letter_spacing;
        if (last_c == ' ')
          trailing_trim += spacing->word_spacing;
      }
    }

    *actual_width = current_width - trailing_trim;
  }

  return result_OK;
}

/* -------------------------------------------------------------------------- */

/* The advance of text[i] for caret purposes: zero for control characters
 * and anything outside the glyph table, as in bmfont_draw. */
static bmfont_width_t bmfont_caret_advance(const bmfont_t         *bmfont,
                                           const char             *text,
                                           int                     i,
                                           const bmfont_spacing_t *spacing)
{
  int c;
  int gid;

  c = (unsigned char) text[i];
  if (c < ' ')
    return 0;

  gid = c - ' ';
  if (gid >= bmfont->totalchars)
    return 0;

  return bmfont_advance_for(bmfont, gid, c, spacing);
}

/* The caret sits in the last pixel of the preceding glyph's advance -- its
 * letter spacing gap -- so it never overlaps ink. */
static bmfont_width_t bmfont_caret_x_for(bmfont_width_t left)
{
  return MAX(0, left - 1);
}

void bmfont_find_caret(bmfont_t               *bmfont,
                       const char             *text,
                       int                     len,
                       const bmfont_spacing_t *spacing,
                       int                     x,
                       int                    *index,
                       bmfont_width_t         *caret_x)
{
  bmfont_width_t left;
  int            i;

  assert(bmfont);
  assert(text || len == 0);
  assert(len >= 0);
  /* index, caret_x may be NULL */

  left = 0;
  for (i = 0; i < len; i++)
  {
    bmfont_width_t advance;

    advance = bmfont_caret_advance(bmfont, text, i, spacing);

    /* stop before this glyph if x is in its left half; an exact midpoint
     * goes to the lower index */
    if (2 * (x - left) <= advance)
      break;

    left += advance;
  }

  if (index)
    *index = i;
  if (caret_x)
    *caret_x = bmfont_caret_x_for(left);
}

bmfont_width_t bmfont_caret_x(bmfont_t               *bmfont,
                              const char             *text,
                              int                     index,
                              const bmfont_spacing_t *spacing)
{
  bmfont_width_t left;
  int            i;

  assert(bmfont);
  assert(text || index == 0);
  assert(index >= 0);

  left = 0;
  for (i = 0; i < index; i++)
    left += bmfont_caret_advance(bmfont, text, i, spacing);

  return bmfont_caret_x_for(left);
}

void bmfont_draw_caret(bmfont_t      *bmfont,
                       screen_t      *scr,
                       colour_t       colour,
                       const point_t *pos)
{
  int top;
  int height;

  assert(bmfont);
  assert(scr);
  assert(pos);

  top    = pos->y - bmfont->ascent;
  height = bmfont->charheight;

  /* I-beam: a full cell height stem with 3px bars on its first and last
   * rows. At the text origin the left bars fall at x-1; clipping copes. */
  screen_fill_rect(scr, pos->x, top, SIZE2D(1, height), colour);
  screen_fill_rect(scr, pos->x - 1, top, SIZE2D(3, 1), colour);
  screen_fill_rect(scr, pos->x - 1, top + height - 1, SIZE2D(3, 1), colour);
}

/* -------------------------------------------------------------------------- */

/* bmfont_drawchar_<pixel format>_<width>w_<o/t>
 * where width is in bytes and o => opaque, t => transparent */
typedef void bmfont_drawchar_t(void          *vscreen,
                               const void    *vglyph,
                               int            top_skip,
                               int            right_skip,
                               int            shift,
                               int            stride,
                               int            charwidth,
                               int            charheight,
                               pixelfmt_any_t fg,
                               pixelfmt_any_t bg);

/* -------------------------------------------------------------------------- */

/* Draw a character, a maximum of one byte wide, to a p4 screen using an opaque
 * background. */
static void bmfont_drawchar_p4_1w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
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
  int stride = rowbytes / sizeof(*scr);

  /* build packed LUT 0xFFBFFBBB for inner loop (F=fg, B=bg) */
  unsigned int tab = (fg * 0x11011000) | (bg * 0x00100111);

  while (charheight--)
  {
    unsigned int row = *gly++; /* 2 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

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

/* Draw a character, a maximum of one byte wide, to a p4 screen using a
 * transparent background. */
static void bmfont_drawchar_p4_1w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;

  NOT_USED(bg);

  assert(shift == 0 || shift == 4);
  assert(charwidth > 0);
  assert(charheight > 0);
  assert((unsigned int) fg < 16);
  assert((unsigned int) bg < 16);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr);

  /* build packed LUTs for inner loop */
  unsigned int fgtab = fg * 0x11011000;
  unsigned int bgtab =      0x00F00FFF;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 2 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

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

/* Draw a character, a maximum of two bytes wide, to a p4 screen using an opaque
 * background. */
static void bmfont_drawchar_p4_2w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
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
  int stride = rowbytes / sizeof(*scr);

  /* build packed LUT 0xFFBFFBBB for inner loop (F=fg, B=bg) */
  unsigned int tab = (fg * 0x11011000) | (bg * 0x00100111);

  while (charheight--)
  {
    unsigned int row = *gly++; /* 2 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

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

/* Draw a character, a maximum of two bytes wide, to a p4 screen using a
 * transparent background. */
static void bmfont_drawchar_p4_2w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;

  NOT_USED(bg);

  assert(shift == 0 || shift == 4);
  assert(charwidth > 0);
  assert(charheight > 0);
  assert((unsigned int) fg < 16);
  assert((unsigned int) bg < 16);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr);

  /* build packed LUTs for inner loop */
  unsigned int fgtab = fg * 0x11011000;
  unsigned int bgtab =      0x00F00FFF;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 2 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

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

/* Draw a character, a maximum of four bytes wide, to a p4 screen using an opaque
 * background. */
static void bmfont_drawchar_p4_4w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char      *scr = vscreen;
  const unsigned int *gly = vglyph;

  assert(shift == 0 || shift == 4);
  assert(charwidth > 0);
  assert(charheight > 0);
  assert((unsigned int) fg < 16);
  assert((unsigned int) bg < 16);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr);

  /* build packed LUT 0xFFBFFBBB for inner loop (F=fg, B=bg) */
  unsigned int tab = (fg * 0x11011000) | (bg * 0x00100111);

  while (charheight--)
  {
    unsigned int row = *gly++; /* 4 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

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
    case 31: *scr2++ = tab >> (((row >> 29) & 3) << 3);
    case 29: *scr2++ = tab >> (((row >> 27) & 3) << 3);
    case 27: *scr2++ = tab >> (((row >> 25) & 3) << 3);
    case 25: *scr2++ = tab >> (((row >> 23) & 3) << 3);
    case 23: *scr2++ = tab >> (((row >> 21) & 3) << 3);
    case 21: *scr2++ = tab >> (((row >> 19) & 3) << 3);
    case 19: *scr2++ = tab >> (((row >> 17) & 3) << 3);
    case 17: *scr2++ = tab >> (((row >> 15) & 3) << 3);
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

    case 32: *scr2++ = tab >> (((row >> 30) & 3) << 3);
    case 30: *scr2++ = tab >> (((row >> 28) & 3) << 3);
    case 28: *scr2++ = tab >> (((row >> 26) & 3) << 3);
    case 26: *scr2++ = tab >> (((row >> 24) & 3) << 3);
    case 24: *scr2++ = tab >> (((row >> 22) & 3) << 3);
    case 22: *scr2++ = tab >> (((row >> 20) & 3) << 3);
    case 20: *scr2++ = tab >> (((row >> 18) & 3) << 3);
    case 18: *scr2++ = tab >> (((row >> 16) & 3) << 3);
    case 16: *scr2++ = tab >> (((row >> 14) & 3) << 3);
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

/* Draw a character, a maximum of four bytes wide, to a p4 screen using a
 * transparent background. */
static void bmfont_drawchar_p4_4w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char      *scr = vscreen;
  const unsigned int *gly = vglyph;

  NOT_USED(bg);

  assert(shift == 0 || shift == 4);
  assert(charwidth > 0);
  assert(charheight > 0);
  assert((unsigned int) fg < 16);
  assert((unsigned int) bg < 16);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr);

  /* build packed LUTs for inner loop */
  unsigned int fgtab = fg * 0x11011000;
  unsigned int bgtab =      0x00F00FFF;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 4 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

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
    case 31: ts = (((row >> 29) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 29: ts = (((row >> 27) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 27: ts = (((row >> 25) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 25: ts = (((row >> 23) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 23: ts = (((row >> 21) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 21: ts = (((row >> 19) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 19: ts = (((row >> 17) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 17: ts = (((row >> 15) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
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

    case 32: ts = (((row >> 30) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 30: ts = (((row >> 28) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 28: ts = (((row >> 26) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 26: ts = (((row >> 24) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 24: ts = (((row >> 22) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 22: ts = (((row >> 20) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 20: ts = (((row >> 18) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 18: ts = (((row >> 16) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
    case 16: ts = (((row >> 14) & 3) << 3); px = (*scr2 & (bgtab >> ts)) | (fgtab >> ts); *scr2++ = px;
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

/* Plot one glyph row onto a p1 (1bpp) screen row. "row" is the glyph bits
 * already shifted down by right_skip, so bit (cw - 1) is the leftmost visible
 * pixel. "scr" points at the byte holding pixel column 0; "shift" (0..7) is
 * that pixel's bit offset from the byte's MSB, matching the MSB-first 1bpp
 * packing used elsewhere. When "opaque" is zero, clear glyph bits are left
 * untouched. */
static void bmfont_p1_plot_row(unsigned char *scr,
                               unsigned int   row,
                               int            cw,
                               int            shift,
                               pixelfmt_any_t fg,
                               pixelfmt_any_t bg,
                               int            opaque)
{
  int i;

  for (i = 0; i < cw; i++)
  {
    int            set;
    int            bitpos;
    unsigned char *p;
    int            b;

    set = (row >> (cw - 1 - i)) & 1;
    if (!set && !opaque)
      continue;

    bitpos = shift + i;
    p      = scr + (bitpos >> 3);
    b      = 7 - (bitpos & 7);

    *p = (unsigned char) ((*p & ~(1 << b))
                        | (((set ? fg : bg) & 1) << b));
  }
}

/* As bmfont_p1_plot_row but for a p2 (2bpp) screen row: two bits per pixel,
 * "shift" is a bit offset (0, 2, 4 or 6) and pixel column i lands at bit-pair
 * (shift + 2 * i), MSB-first within the byte. */
static void bmfont_p2_plot_row(unsigned char *scr,
                               unsigned int   row,
                               int            cw,
                               int            shift,
                               pixelfmt_any_t fg,
                               pixelfmt_any_t bg,
                               int            opaque)
{
  int i;

  for (i = 0; i < cw; i++)
  {
    int            set;
    int            bitpos;
    unsigned char *p;
    int            b;

    set = (row >> (cw - 1 - i)) & 1;
    if (!set && !opaque)
      continue;

    bitpos = shift + (i << 1);
    p      = scr + (bitpos >> 3);
    b      = 6 - (bitpos & 7);

    *p = (unsigned char) ((*p & ~(3 << b))
                        | (((set ? fg : bg) & 3) << b));
  }
}

/* Draw a character to a p1 screen. One helper covers 1- and 2-byte glyph rows
 * and both opaque and transparent backgrounds -- the p1 inner loop is a plain
 * per-pixel bit write, so the LUT-driven width unrolling the p4 path needs
 * buys nothing here. */
static void bmfont_drawchar_p1_1w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;
  int                  stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = (unsigned int) *gly++ >> right_skip;

    bmfont_p1_plot_row(scr, row, charwidth, shift, fg, bg, 1);
    scr += stride;
  }
}

static void bmfont_drawchar_p1_1w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;
  int                  stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = (unsigned int) *gly++ >> right_skip;

    bmfont_p1_plot_row(scr, row, charwidth, shift, fg, bg, 0);
    scr += stride;
  }
}

static void bmfont_drawchar_p1_2w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;
  int                   stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = (unsigned int) *gly++ >> right_skip;

    bmfont_p1_plot_row(scr, row, charwidth, shift, fg, bg, 1);
    scr += stride;
  }
}

static void bmfont_drawchar_p1_2w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;
  int                   stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = (unsigned int) *gly++ >> right_skip;

    bmfont_p1_plot_row(scr, row, charwidth, shift, fg, bg, 0);
    scr += stride;
  }
}

static void bmfont_drawchar_p1_4w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char      *scr = vscreen;
  const unsigned int *gly = vglyph;
  int                 stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = *gly++ >> right_skip;

    bmfont_p1_plot_row(scr, row, charwidth, shift, fg, bg, 1);
    scr += stride;
  }
}

static void bmfont_drawchar_p1_4w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char      *scr = vscreen;
  const unsigned int *gly = vglyph;
  int                 stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = *gly++ >> right_skip;

    bmfont_p1_plot_row(scr, row, charwidth, shift, fg, bg, 0);
    scr += stride;
  }
}

/* -------------------------------------------------------------------------- */

static void bmfont_drawchar_p2_1w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;
  int                  stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = (unsigned int) *gly++ >> right_skip;

    bmfont_p2_plot_row(scr, row, charwidth, shift, fg, bg, 1);
    scr += stride;
  }
}

static void bmfont_drawchar_p2_1w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;
  int                  stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = (unsigned int) *gly++ >> right_skip;

    bmfont_p2_plot_row(scr, row, charwidth, shift, fg, bg, 0);
    scr += stride;
  }
}

static void bmfont_drawchar_p2_2w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;
  int                   stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = (unsigned int) *gly++ >> right_skip;

    bmfont_p2_plot_row(scr, row, charwidth, shift, fg, bg, 1);
    scr += stride;
  }
}

static void bmfont_drawchar_p2_2w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;
  int                   stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = (unsigned int) *gly++ >> right_skip;

    bmfont_p2_plot_row(scr, row, charwidth, shift, fg, bg, 0);
    scr += stride;
  }
}

static void bmfont_drawchar_p2_4w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char      *scr = vscreen;
  const unsigned int *gly = vglyph;
  int                 stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = *gly++ >> right_skip;

    bmfont_p2_plot_row(scr, row, charwidth, shift, fg, bg, 1);
    scr += stride;
  }
}

static void bmfont_drawchar_p2_4w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char      *scr = vscreen;
  const unsigned int *gly = vglyph;
  int                 stride;

  assert(charwidth > 0);
  assert(charheight > 0);

  gly   += top_skip;
  stride = rowbytes;

  while (charheight--)
  {
    unsigned int row = *gly++ >> right_skip;

    bmfont_p2_plot_row(scr, row, charwidth, shift, fg, bg, 0);
    scr += stride;
  }
}

/* -------------------------------------------------------------------------- */

/* Draw a character, a maximum of one byte wide, to any 8888 screen using an
 * opaque background. */
static void bmfont_drawchar_any8888_1w_o(void          *vscreen,
                                         const void    *vglyph,
                                         int            top_skip,
                                         int            right_skip,
                                         int            shift,
                                         int            rowbytes,
                                         int            charwidth,
                                         int            charheight,
                                         pixelfmt_any_t fg,
                                         pixelfmt_any_t bg)
{
  pixelfmt_bgrx8888_t *scr = vscreen;
  const unsigned char *gly = vglyph;

  NOT_USED(shift);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr) - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 1 byte wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 8: *scr++ = (row & (1u << 7)) ? fg : bg; /* fallthrough! */
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

/* Draw a character, a maximum of one byte wide, to any 8888 screen using a
 * transparent background. */
static void bmfont_drawchar_any8888_1w_t(void          *vscreen,
                                         const void    *vglyph,
                                         int            top_skip,
                                         int            right_skip,
                                         int            shift,
                                         int            rowbytes,
                                         int            charwidth,
                                         int            charheight,
                                         pixelfmt_any_t fg,
                                         pixelfmt_any_t bg)
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
    unsigned int row = *gly++; /* 1 byte wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 8: if (row & (1u << 7)) *scr = fg; scr++; /* fallthrough! */
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

/* Draw a character, a maximum of two bytes wide, to any 8888 screen using an
 * opaque background. */
static void bmfont_drawchar_any8888_2w_o(void          *vscreen,
                                         const void    *vglyph,
                                         int            top_skip,
                                         int            right_skip,
                                         int            shift,
                                         int            rowbytes,
                                         int            charwidth,
                                         int            charheight,
                                         pixelfmt_any_t fg,
                                         pixelfmt_any_t bg)
{
  pixelfmt_bgrx8888_t  *scr = vscreen;
  const unsigned short *gly = vglyph;

  NOT_USED(shift);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr) - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 2 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 16: *scr++ = (row & (1u << 15)) ? fg : bg; /* fallthrough! */
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

/* Draw a character, a maximum of two bytes wide, to any 8888 screen using a
 * transparent background. */
static void bmfont_drawchar_any8888_2w_t(void          *vscreen,
                                         const void    *vglyph,
                                         int            top_skip,
                                         int            right_skip,
                                         int            shift,
                                         int            rowbytes,
                                         int            charwidth,
                                         int            charheight,
                                         pixelfmt_any_t fg,
                                         pixelfmt_any_t bg)
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
    unsigned int row = *gly++; /* 2 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 16: if (row & (1u << 15)) *scr = fg; scr++; /* fallthrough! */
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

/* Draw a character, a maximum of four bytes wide, to any 8888 screen using an
 * opaque background. */
static void bmfont_drawchar_any8888_4w_o(void          *vscreen,
                                         const void    *vglyph,
                                         int            top_skip,
                                         int            right_skip,
                                         int            shift,
                                         int            rowbytes,
                                         int            charwidth,
                                         int            charheight,
                                         pixelfmt_any_t fg,
                                         pixelfmt_any_t bg)
{
  pixelfmt_bgrx8888_t *scr = vscreen;
  const unsigned int  *gly = vglyph;

  NOT_USED(shift);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr) - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 4 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 32: *scr++ = (row & (1u << 31)) ? fg : bg; /* fallthrough! */
      case 31: *scr++ = (row & (1u << 30)) ? fg : bg;
      case 30: *scr++ = (row & (1u << 29)) ? fg : bg;
      case 29: *scr++ = (row & (1u << 28)) ? fg : bg;
      case 28: *scr++ = (row & (1u << 27)) ? fg : bg;
      case 27: *scr++ = (row & (1u << 26)) ? fg : bg;
      case 26: *scr++ = (row & (1u << 25)) ? fg : bg;
      case 25: *scr++ = (row & (1u << 24)) ? fg : bg;
      case 24: *scr++ = (row & (1u << 23)) ? fg : bg;
      case 23: *scr++ = (row & (1u << 22)) ? fg : bg;
      case 22: *scr++ = (row & (1u << 21)) ? fg : bg;
      case 21: *scr++ = (row & (1u << 20)) ? fg : bg;
      case 20: *scr++ = (row & (1u << 19)) ? fg : bg;
      case 19: *scr++ = (row & (1u << 18)) ? fg : bg;
      case 18: *scr++ = (row & (1u << 17)) ? fg : bg;
      case 17: *scr++ = (row & (1u << 16)) ? fg : bg;
      case 16: *scr++ = (row & (1u << 15)) ? fg : bg;
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

/* Draw a character, a maximum of four bytes wide, to any 8888 screen using a
 * transparent background. */
static void bmfont_drawchar_any8888_4w_t(void          *vscreen,
                                         const void    *vglyph,
                                         int            top_skip,
                                         int            right_skip,
                                         int            shift,
                                         int            rowbytes,
                                         int            charwidth,
                                         int            charheight,
                                         pixelfmt_any_t fg,
                                         pixelfmt_any_t bg)
{
  pixelfmt_bgrx8888_t *scr = vscreen;
  const unsigned int  *gly = vglyph;

  NOT_USED(shift);
  NOT_USED(bg);

  gly += top_skip;

  /* adjust stride */
  int stride = rowbytes / sizeof(*scr) - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 4 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 32: if (row & (1u << 31)) *scr = fg; scr++; /* fallthrough! */
      case 31: if (row & (1u << 30)) *scr = fg; scr++;
      case 30: if (row & (1u << 29)) *scr = fg; scr++;
      case 29: if (row & (1u << 28)) *scr = fg; scr++;
      case 28: if (row & (1u << 27)) *scr = fg; scr++;
      case 27: if (row & (1u << 26)) *scr = fg; scr++;
      case 26: if (row & (1u << 25)) *scr = fg; scr++;
      case 25: if (row & (1u << 24)) *scr = fg; scr++;
      case 24: if (row & (1u << 23)) *scr = fg; scr++;
      case 23: if (row & (1u << 22)) *scr = fg; scr++;
      case 22: if (row & (1u << 21)) *scr = fg; scr++;
      case 21: if (row & (1u << 20)) *scr = fg; scr++;
      case 20: if (row & (1u << 19)) *scr = fg; scr++;
      case 19: if (row & (1u << 18)) *scr = fg; scr++;
      case 18: if (row & (1u << 17)) *scr = fg; scr++;
      case 17: if (row & (1u << 16)) *scr = fg; scr++;
      case 16: if (row & (1u << 15)) *scr = fg; scr++;
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

/* Draw a character, a maximum of one byte wide, to a p8 screen using an
 * opaque background. */
static void bmfont_drawchar_p8_1w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;
  int                  stride;

  NOT_USED(shift);

  gly    += top_skip;
  stride = rowbytes - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 1 byte wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 8: *scr++ = (row & (1u << 7)) ? fg : bg; /* fallthrough! */
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

/* Draw a character, a maximum of one byte wide, to a p8 screen using a
 * transparent background. */
static void bmfont_drawchar_p8_1w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char       *scr = vscreen;
  const unsigned char *gly = vglyph;
  int                  stride;

  NOT_USED(shift);
  NOT_USED(bg);

  gly    += top_skip;
  stride = rowbytes - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 1 byte wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 8: if (row & (1u << 7)) *scr = fg; scr++; /* fallthrough! */
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

/* Draw a character, a maximum of two bytes wide, to a p8 screen using an
 * opaque background. */
static void bmfont_drawchar_p8_2w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;
  int                   stride;

  NOT_USED(shift);

  gly    += top_skip;
  stride = rowbytes - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 2 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 16: *scr++ = (row & (1u << 15)) ? fg : bg; /* fallthrough! */
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

/* Draw a character, a maximum of two bytes wide, to a p8 screen using a
 * transparent background. */
static void bmfont_drawchar_p8_2w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char        *scr = vscreen;
  const unsigned short *gly = vglyph;
  int                   stride;

  NOT_USED(shift);
  NOT_USED(bg);

  gly    += top_skip;
  stride = rowbytes - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 2 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 16: if (row & (1u << 15)) *scr = fg; scr++; /* fallthrough! */
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

/* Draw a character, a maximum of four bytes wide, to a p8 screen using an
 * opaque background. */
static void bmfont_drawchar_p8_4w_o(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char      *scr = vscreen;
  const unsigned int *gly = vglyph;
  int                 stride;

  NOT_USED(shift);

  gly    += top_skip;
  stride = rowbytes - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 4 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 32: *scr++ = (row & (1u << 31)) ? fg : bg; /* fallthrough! */
      case 31: *scr++ = (row & (1u << 30)) ? fg : bg;
      case 30: *scr++ = (row & (1u << 29)) ? fg : bg;
      case 29: *scr++ = (row & (1u << 28)) ? fg : bg;
      case 28: *scr++ = (row & (1u << 27)) ? fg : bg;
      case 27: *scr++ = (row & (1u << 26)) ? fg : bg;
      case 26: *scr++ = (row & (1u << 25)) ? fg : bg;
      case 25: *scr++ = (row & (1u << 24)) ? fg : bg;
      case 24: *scr++ = (row & (1u << 23)) ? fg : bg;
      case 23: *scr++ = (row & (1u << 22)) ? fg : bg;
      case 22: *scr++ = (row & (1u << 21)) ? fg : bg;
      case 21: *scr++ = (row & (1u << 20)) ? fg : bg;
      case 20: *scr++ = (row & (1u << 19)) ? fg : bg;
      case 19: *scr++ = (row & (1u << 18)) ? fg : bg;
      case 18: *scr++ = (row & (1u << 17)) ? fg : bg;
      case 17: *scr++ = (row & (1u << 16)) ? fg : bg;
      case 16: *scr++ = (row & (1u << 15)) ? fg : bg;
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

/* Draw a character, a maximum of four bytes wide, to a p8 screen using a
 * transparent background. */
static void bmfont_drawchar_p8_4w_t(void          *vscreen,
                                    const void    *vglyph,
                                    int            top_skip,
                                    int            right_skip,
                                    int            shift,
                                    int            rowbytes,
                                    int            charwidth,
                                    int            charheight,
                                    pixelfmt_any_t fg,
                                    pixelfmt_any_t bg)
{
  unsigned char      *scr = vscreen;
  const unsigned int *gly = vglyph;
  int                 stride;

  NOT_USED(shift);
  NOT_USED(bg);

  gly    += top_skip;
  stride = rowbytes - charwidth;

  while (charheight--)
  {
    unsigned int row = *gly++; /* 4 bytes wide font data */
    row >>= right_skip; /* compensate when right hand clipping */

    switch (charwidth) /* jump table test */
    {
      case 32: if (row & (1u << 31)) *scr = fg; scr++; /* fallthrough! */
      case 31: if (row & (1u << 30)) *scr = fg; scr++;
      case 30: if (row & (1u << 29)) *scr = fg; scr++;
      case 29: if (row & (1u << 28)) *scr = fg; scr++;
      case 28: if (row & (1u << 27)) *scr = fg; scr++;
      case 27: if (row & (1u << 26)) *scr = fg; scr++;
      case 26: if (row & (1u << 25)) *scr = fg; scr++;
      case 25: if (row & (1u << 24)) *scr = fg; scr++;
      case 24: if (row & (1u << 23)) *scr = fg; scr++;
      case 23: if (row & (1u << 22)) *scr = fg; scr++;
      case 22: if (row & (1u << 21)) *scr = fg; scr++;
      case 21: if (row & (1u << 20)) *scr = fg; scr++;
      case 20: if (row & (1u << 19)) *scr = fg; scr++;
      case 19: if (row & (1u << 18)) *scr = fg; scr++;
      case 18: if (row & (1u << 17)) *scr = fg; scr++;
      case 17: if (row & (1u << 16)) *scr = fg; scr++;
      case 16: if (row & (1u << 15)) *scr = fg; scr++;
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

result_t bmfont_draw(bmfont_t               *bmfont,
                     screen_t               *scr,
                     const char             *text,
                     int                     len,
                     colour_t                fg,
                     colour_t                bg,
                     const bmfont_spacing_t *spacing,
                     const point_t          *pos,
                     point_t                *end_pos)
{
  bmfont_drawchar_t *drawfn;
  box_t              scrclip;
  box_t              drawbox;
  box_t              drawclip;
  point_t            top;

  unsigned int bgalpha = colour_get_alpha(&bg);

  /* pos is the baseline; glyph cells are drawn from their top, ascent above
   * the baseline */
  top = POINT(pos->x, pos->y - bmfont->ascent);

  switch (scr->format)
  {
  case pixelfmt_p1:
    switch (bmfont->glyphrowbytes)
    {
    case 1: drawfn = (bgalpha < 255) ? bmfont_drawchar_p1_1w_t : bmfont_drawchar_p1_1w_o; break;
    case 2: drawfn = (bgalpha < 255) ? bmfont_drawchar_p1_2w_t : bmfont_drawchar_p1_2w_o; break;
    case 4: drawfn = (bgalpha < 255) ? bmfont_drawchar_p1_4w_t : bmfont_drawchar_p1_4w_o; break;
    default: assert(0); return result_NOT_SUPPORTED;
    }
    break;

  case pixelfmt_p2:
    switch (bmfont->glyphrowbytes)
    {
    case 1: drawfn = (bgalpha < 255) ? bmfont_drawchar_p2_1w_t : bmfont_drawchar_p2_1w_o; break;
    case 2: drawfn = (bgalpha < 255) ? bmfont_drawchar_p2_2w_t : bmfont_drawchar_p2_2w_o; break;
    case 4: drawfn = (bgalpha < 255) ? bmfont_drawchar_p2_4w_t : bmfont_drawchar_p2_4w_o; break;
    default: assert(0); return result_NOT_SUPPORTED;
    }
    break;

  case pixelfmt_p4:
    switch (bmfont->glyphrowbytes)
    {
    case 1: drawfn = (bgalpha < 255) ? bmfont_drawchar_p4_1w_t : bmfont_drawchar_p4_1w_o; break;
    case 2: drawfn = (bgalpha < 255) ? bmfont_drawchar_p4_2w_t : bmfont_drawchar_p4_2w_o; break;
    case 4: drawfn = (bgalpha < 255) ? bmfont_drawchar_p4_4w_t : bmfont_drawchar_p4_4w_o; break;
    default: assert(0); return result_NOT_SUPPORTED;
    }
    break;

  case pixelfmt_p8:
    switch (bmfont->glyphrowbytes)
    {
    case 1: drawfn = (bgalpha < 255) ? bmfont_drawchar_p8_1w_t : bmfont_drawchar_p8_1w_o; break;
    case 2: drawfn = (bgalpha < 255) ? bmfont_drawchar_p8_2w_t : bmfont_drawchar_p8_2w_o; break;
    case 4: drawfn = (bgalpha < 255) ? bmfont_drawchar_p8_4w_t : bmfont_drawchar_p8_4w_o; break;
    default: assert(0); return result_NOT_SUPPORTED;
    }
    break;

  case pixelfmt_bgra8888:
  case pixelfmt_bgrx8888:
    switch (bmfont->glyphrowbytes)
    {
    case 1: drawfn = (bgalpha < 255) ? bmfont_drawchar_any8888_1w_t : bmfont_drawchar_any8888_1w_o; break;
    case 2: drawfn = (bgalpha < 255) ? bmfont_drawchar_any8888_2w_t : bmfont_drawchar_any8888_2w_o; break;
    case 4: drawfn = (bgalpha < 255) ? bmfont_drawchar_any8888_4w_t : bmfont_drawchar_any8888_4w_o; break;
    default: assert(0); return result_NOT_SUPPORTED;
    }
    break;

  default:
    assert(0); return result_NOT_SUPPORTED;
  }

  if (screen_get_clip(scr, &scrclip))
    return result_OK; /* invalid clipped screen */

  {
    int extra_per_char;

    extra_per_char = spacing ?
      MAX(spacing->letter_spacing + spacing->word_spacing, 0) : 0;

    drawbox.x0 = top.x;
    drawbox.y0 = top.y;
    drawbox.x1 = top.x + (bmfont->charwidth + extra_per_char) * len; /* worst-case estimate */
    drawbox.y1 = top.y + bmfont->charheight;
  }

  if (!box_intersects(&scrclip, &drawbox))
  {
    /* nothing to paint, but callers that chain draws by feeding this
     * call's end_pos in as the next call's pos (e.g. drawing a line as
     * several separately-coloured runs) still need the correct advance,
     * or the run after this one starts from a stale x and every
     * following run on the line shifts left by this run's width */
    if (end_pos)
    {
      int x;

      x = pos->x;
      while (len--)
      {
        int c;
        int gid;

        c = *text++;
        if ((unsigned char) c < ' ')
          continue;
        gid = (unsigned char) c - ' ';
        if (gid >= bmfont->totalchars)
          continue;

        x += bmfont_advance_for(bmfont, gid, c, spacing);
      }
      *end_pos = POINT(x, pos->y);
    }

    return result_OK; /* not visible */
  }

  box_clipped(&scrclip, &drawbox, &drawclip); /* note: drawclip isn't a proper box */

  int            left_skip         = drawclip.x0;
  int            top_skip          = drawclip.y0;
  /* note: drawclip.x1 doesn't get used */
  int            bottom_skip       = drawclip.y1;

  /* ensure that any computed positions are inside the clip box */
  unsigned int   clamped_pos_x     = CLAMP(top.x, scrclip.x0, scrclip.x1 - 1);
  unsigned int   clamped_pos_y     = CLAMP(top.y, scrclip.y0, scrclip.y1 - 1);

  int            log2bpp           = pixelfmt_log2bpp(scr->format);
  unsigned char *screen            = (unsigned char *) scr->base + clamped_pos_y * scr->rowbytes + ((clamped_pos_x << log2bpp) >> 3);
  int            log2pixperb       = MAX(0, 3 - log2bpp); /* log2 pixels per byte */
  int            shift             = (clamped_pos_x & ((1 << log2pixperb) - 1)) << log2bpp;

  int            rowbytes          = scr->rowbytes;
  int            charwidth         = bmfont->charwidth;
  int            charheight        = bmfont->charheight;
  int            clippedcharheight = charheight - top_skip - bottom_skip;
  int            glyphbytes        = bmfont->glyphrowbytes * charheight;
  pixelfmt_any_t nativefg          = colour_to_pixel(scr->palette, scr->palette ? 1 << (1 << log2bpp) : 0, fg, scr->format);
  pixelfmt_any_t nativebg          = colour_to_pixel(scr->palette, scr->palette ? 1 << (1 << log2bpp) : 0, bg, scr->format);
  int            remaining         = scrclip.x1 - clamped_pos_x; /* in pixels */

  int            x                 = pos->x;

  /* box_intersects said something is visible, but a glyph can still be fully
   * clipped vertically at a boundary. drawfn's `while (charheight--)` would
   * then run ~INT_MAX times, so bail rather than trust the assert alone. */
  assert(clippedcharheight > 0);
  if (clippedcharheight <= 0)
  {
    if (end_pos)
      *end_pos = POINT(pos->x, pos->y);
    return result_OK;
  }

  while (len--)
  {
    int         c;
    int         gid;
    int         advance;
    const void *glyph;

    c       = *text++;

    /* control characters and anything outside the glyph table draw nothing
     * and advance nothing -- matching bmfont_measure. */
    if ((unsigned char) c < ' ')
      continue;
    gid = (unsigned char) c - ' ';
    if (gid >= bmfont->totalchars)
      continue;

    advance = bmfont_advance_for(bmfont, gid, c, spacing);

    x += advance;

    /* skip characters until we can plot */
    if (left_skip > advance)
    {
      left_skip -= advance;
      continue;
    }

    /* draw: the glyph cell itself, then any leftover advance (letter
     * spacing, or monospace padding) as a second, glyph-less segment of
     * the same width the drawfn already knows how to paint as pure
     * background (opaque) or leave untouched (transparent) -- a row of
     * zero bits reads as "no ink" either way. */
    {
      static const unsigned char zero_glyph[128] = { 0 };

      int cellwidth;
      int cellpad;
      int clippedcellwidth;

      glyph     = (unsigned char *) bmfont->glyphs + gid * glyphbytes;
      cellwidth = MIN(advance, charwidth);
      cellpad   = advance - cellwidth;

      clippedcellwidth = MIN(remaining, cellwidth - left_skip); /* in pixels */

      if (clippedcellwidth > 0)
      {
        int right_skip = charwidth - left_skip - clippedcellwidth;

        drawfn(screen, glyph,
               top_skip, right_skip,
               shift, rowbytes,
               clippedcellwidth, clippedcharheight,
               nativefg, nativebg);

        if ((remaining -= clippedcellwidth) <= 0)
          break; /* stop drawing */

        {
          int pixelsused_bits = clippedcellwidth << log2bpp;
          int nbits           = shift + pixelsused_bits;
          screen = (unsigned char *) screen + (nbits >> 3);
          shift  = nbits & 7;
        }

        left_skip = 0;
      }
      else
      {
        /* still entirely left-clipped: the visible clip is narrower than
         * this character's remaining left-clip amount. Skip it exactly
         * like the whole-character skip above, without touching
         * "remaining" or the screen pointer/shift, since nothing of it
         * was drawn. */
        left_skip -= cellwidth;
      }

      if (cellpad > 0 && left_skip < cellpad)
      {
        int clippedspacing = MIN(remaining, cellpad - left_skip);

        if (clippedspacing > 0)
        {
          assert((size_t) clippedcharheight * bmfont->glyphrowbytes <=
                 sizeof(zero_glyph));

          drawfn(screen, zero_glyph,
                 top_skip, 0,
                 shift, rowbytes,
                 clippedspacing, clippedcharheight,
                 nativefg, nativebg);

          if ((remaining -= clippedspacing) <= 0)
            break; /* stop drawing */

          {
            int pixelsused_bits = clippedspacing << log2bpp;
            int nbits           = shift + pixelsused_bits;
            screen = (unsigned char *) screen + (nbits >> 3);
            shift  = nbits & 7;
          }
        }

        left_skip = 0;
      }
      else if (cellpad > 0)
      {
        left_skip -= cellpad;
      }
    }
  }

  if (end_pos)
  {
    /* calculate final x */
    if (len > 0)
    {
      while (len--)
      {
        int c;
        int gid;
        int advance;

        c = *text++;
        if ((unsigned char) c < ' ')
          continue;
        gid = (unsigned char) c - ' ';
        if (gid >= bmfont->totalchars)
          continue;

        advance = bmfont_advance_for(bmfont, gid, c, spacing);

        x += advance;
      }
    }

    *end_pos = POINT(x, pos->y);
  }

  return result_OK;
}

/* -------------------------------------------------------------------------- */

result_t bmfont_draw_relief(bmfont_t               *bmfont,
                            screen_t               *scr,
                            const char             *text,
                            int                     len,
                            colour_t                fg,
                            colour_t                shadow,
                            const bmfont_spacing_t *spacing,
                            const point_t          *pos,
                            const point_t          *offset,
                            point_t                *end_pos)
{
  result_t rc;
  colour_t transparent;
  point_t  shadowpos;

  transparent = colour_rgba(0, 0, 0, 0);
  shadowpos   = POINT(pos->x + offset->x, pos->y + offset->y);

  rc = bmfont_draw(bmfont, scr, text, len, shadow, transparent, spacing,
                  &shadowpos, NULL);
  if (rc)
    return rc;

  return bmfont_draw(bmfont, scr, text, len, fg, transparent, spacing, pos,
                     end_pos);
}

/* -------------------------------------------------------------------------- */

/* vim: set ts=8 sts=2 sw=2 et: */
