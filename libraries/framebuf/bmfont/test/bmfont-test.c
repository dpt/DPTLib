/* bmfont-test.c */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/colour.h"

#include "framebuf/bmfont.h"

/* ----------------------------------------------------------------------- */

// TODO: Solve this absolute path....
#define PATH "/Users/dave/SyncProjects/github/DPTLib/" // ick.

/* ----------------------------------------------------------------------- */

typedef struct testfont
{
  const char *filename;
  bmfont_t   *bmfont;
}
bmtestfont_t;

typedef struct testline
{
  int           font_index;
  unsigned char fg,bg; // palette indices
  point_t       origin;
  const char   *string;
}
bmtestline_t;

/* ----------------------------------------------------------------------- */

static const char lorem_ipsum[] =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
"tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, "
"quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo "
"consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse "
"cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non "
"proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

static const bmtestline_t lines[] =
{
  { 0, 0, 16, { 4,   4       }, "HELLO, WORLD!" },
  { 0, 1,  9, { 4,   4+ 6    }, "This is a tiny 4x5 font, so I can write loads and loads in it, even this!" },
  { 0, 2, 10, { 4,   4+ 6* 2 }, lorem_ipsum },
  { 1, 3, 16, { 4, 128+16*-1 }, "Hello Humans!" },
  { 1, 4, 12, { 4, 128+16* 0 }, "This is a massive 15x16 font, so I have to split it up!" },
  { 1, 5, 13, { 4, 128+16* 2 }, "Five boxing wizards vex the quick brown fox." },
  { 1, 6, 14, { 4, 128+16* 4 }, "Thisisjustalonglonglinewithoutanyspacestotestsplittingcornercases." },
};

/* ----------------------------------------------------------------------- */

static bmtestfont_t fonts[] =
{
    { PATH "tiny-font.png",  NULL },
    { PATH "henry-font.png", NULL }
};

/* ----------------------------------------------------------------------- */

result_t bmfont_test(void)
{
  const int scr_width    = 320;
  const int scr_height   = 240;
  const int scr_rowbytes = scr_width * 4;

  const colour_t palette[] = {
    colour_rgb(0x00, 0x00, 0x00),
    colour_rgb(0x1D, 0x2B, 0x53),
    colour_rgb(0x7E, 0x25, 0x53),
    colour_rgb(0x00, 0x87, 0x51),
    colour_rgb(0xAB, 0x52, 0x36),
    colour_rgb(0x5F, 0x57, 0x4F),
    colour_rgb(0xC2, 0xC3, 0xC7),
    colour_rgb(0xFF, 0xF1, 0xE8),
    colour_rgb(0xFF, 0x00, 0x4D),
    colour_rgb(0xFF, 0xA3, 0x00),
    colour_rgb(0xFF, 0xEC, 0x27),
    colour_rgb(0x00, 0xE4, 0x36),
    colour_rgb(0x29, 0xAD, 0xFF),
    colour_rgb(0x83, 0x76, 0x9C),
    colour_rgb(0xFF, 0x77, 0xA8),
    colour_rgb(0xFF, 0xCC, 0xAA),
    colour_rgba(0x00, 0x00, 0x00, 0x00) // transparent
  };


  result_t            rc = result_OK;
  unsigned int       *pixels;
  bitmap_t            bm;
  int                 bm_inited = 0;
  int                 font;
  screen_t            scr;
  const bmtestline_t *line;

  pixels = malloc(scr_rowbytes * scr_height);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }

  bitmap_init(&bm, scr_width, scr_height, pixelfmt_bgra8888, scr_rowbytes, NULL, pixels);
  bm_inited = 1;

  bitmap_clear(&bm, palette[15]);

  for (font = 0; font < NELEMS(fonts); font++)
  {
    rc = bmfont_create(fonts[font].filename, &fonts[font].bmfont);
    if (rc)
      goto Failure;
  }

  screen_for_bitmap(&scr, &bm);

  // clipping test
  {
    const int fonti = 1;
    point_t origin;

    origin.x = 4;
    origin.y = -4;
    for (int i = 0; i < scr_height/16; i++)
    {
      rc = bmfont_draw(fonts[fonti].bmfont,
                       &scr,
                       "farts",
                       5,
                       palette[7],
                       palette[11],
                       &origin,
                       NULL /*endpos*/);
      origin.x -= 1;
      origin.y += 16;
    }
  }

  // general test
  if (0)
  for (line = &lines[0]; line < &lines[0] + NELEMS(lines); line++)
  {
    bmfont_t   *bmfont    = fonts[line->font_index].bmfont;
    int         glyphwidth, glyphheight;
    const char *string    = line->string;
    size_t      stringlen = strlen(line->string);
    point_t     origin    = line->origin;

    bmfont_info(bmfont, &glyphwidth, &glyphheight);

    do
    {
      int            absolute_break;
      int            friendly_break;
      bmfont_width_t width;

      (void) bmfont_measure(bmfont,
                            string,
                            stringlen,
                            scr_width - (line->origin.x * 2),
                           &absolute_break, // if no break returns strlen
                           &width);
      //printf("absolute_break=%d w=%d\n", absolute_break, width);

      friendly_break = absolute_break;
      if (absolute_break < stringlen) // if string did need breaking
      {
        /* Try to split at spaces */
        for (friendly_break = absolute_break - 1; friendly_break >= 0; friendly_break--)
          if (isspace(string[friendly_break]))
            break;

        if (friendly_break < 0)
        {
          /* If no spaces found, break within word */
          friendly_break = absolute_break;
        }
        else
        {
          /* Space found, skip it */
          assert(isspace(string[friendly_break]));
          friendly_break++;
        }
      }

      rc = bmfont_draw(bmfont,
                      &scr,
                       string,
                       friendly_break,
                       palette[line->fg],
                       palette[line->bg],
                      &origin,
                       NULL /*endpos*/);
      if (rc)
        goto Failure;

      origin.y += glyphheight + 1; // 1 => leading

      string    += friendly_break;
      stringlen -= friendly_break;
    }
    while (stringlen > 0);
  }

  bitmap_save_png(&bm, "bmfont-output.png");

  rc = result_TEST_PASSED;

Cleanup:
  for (font = 0; font < NELEMS(fonts); font++)
    bmfont_destroy(fonts[font].bmfont);

  if (bm_inited)
    free(bm.base);

  return rc;


Failure:
  fprintf(stderr, "error: %x\n", rc);
  rc = result_TEST_FAILED;
  goto Cleanup;
}
