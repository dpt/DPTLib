/* bmfont-test.c */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/bmfont.h"

/* ----------------------------------------------------------------------- */

/* Configuration */

#define TEST_P4 /* test 4bpp */

const int GAMEWIDTH  = 800;
const int GAMEHEIGHT = 600;

/* ----------------------------------------------------------------------- */

#ifdef USE_SDL

#include <SDL.h>

typedef struct sdlstate
{
  SDL_Window   *window;
  SDL_Renderer *renderer;
  SDL_Texture  *texture;
}
sdlstate_t;

void PrintEvent(const SDL_Event *event)
{
  if (event->type == SDL_WINDOWEVENT) {
    switch (event->window.event) {
      case SDL_WINDOWEVENT_SHOWN:
        SDL_Log("Window %d shown", event->window.windowID);
        break;
      case SDL_WINDOWEVENT_HIDDEN:
        SDL_Log("Window %d hidden", event->window.windowID);
        break;
      case SDL_WINDOWEVENT_EXPOSED:
        SDL_Log("Window %d exposed", event->window.windowID);
        break;
      case SDL_WINDOWEVENT_MOVED:
        SDL_Log("Window %d moved to %d,%d",
            event->window.windowID, event->window.data1,
            event->window.data2);
        break;
      case SDL_WINDOWEVENT_RESIZED:
        SDL_Log("Window %d resized to %dx%d",
            event->window.windowID, event->window.data1,
            event->window.data2);
        break;
      case SDL_WINDOWEVENT_SIZE_CHANGED:
        SDL_Log("Window %d size changed to %dx%d",
            event->window.windowID, event->window.data1,
            event->window.data2);
        break;
      case SDL_WINDOWEVENT_MINIMIZED:
        SDL_Log("Window %d minimized", event->window.windowID);
        break;
      case SDL_WINDOWEVENT_MAXIMIZED:
        SDL_Log("Window %d maximized", event->window.windowID);
        break;
      case SDL_WINDOWEVENT_RESTORED:
        SDL_Log("Window %d restored", event->window.windowID);
        break;
      case SDL_WINDOWEVENT_ENTER:
        SDL_Log("Mouse entered window %d",
            event->window.windowID);
        break;
      case SDL_WINDOWEVENT_LEAVE:
        SDL_Log("Mouse left window %d", event->window.windowID);
        break;
      case SDL_WINDOWEVENT_FOCUS_GAINED:
        SDL_Log("Window %d gained keyboard focus",
            event->window.windowID);
        break;
      case SDL_WINDOWEVENT_FOCUS_LOST:
        SDL_Log("Window %d lost keyboard focus",
            event->window.windowID);
        break;
      case SDL_WINDOWEVENT_CLOSE:
        SDL_Log("Window %d closed", event->window.windowID);
        break;
#if SDL_VERSION_ATLEAST(2, 0, 5)
      case SDL_WINDOWEVENT_TAKE_FOCUS:
        SDL_Log("Window %d is offered a focus", event->window.windowID);
        break;
      case SDL_WINDOWEVENT_HIT_TEST:
        SDL_Log("Window %d has a special hit test", event->window.windowID);
        break;
#endif
      default:
        SDL_Log("Window %d got unknown event %d",
            event->window.windowID, event->window.event);
        break;
    }
  }
}

static result_t start_sdl(sdlstate_t *state)
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    fprintf(stderr, "Error: SDL_Init: %s\n", SDL_GetError());
    goto failure;
  }

  state->window = SDL_CreateWindow("DPTLib SDL Test",
                                   SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                   GAMEWIDTH, GAMEHEIGHT,
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (state->window == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateWindow: %s\n", SDL_GetError());
    goto failure;
  }

  state->renderer = SDL_CreateRenderer(state->window,
                                       -1,
                                       SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (state->renderer == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateRenderer: %s\n", SDL_GetError());
    goto failure;
  }

  state->texture = SDL_CreateTexture(state->renderer,
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     GAMEWIDTH, GAMEHEIGHT);
  if (state->texture == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateTexture: %s\n", SDL_GetError());
    goto failure;
  }

  if (SDL_SetTextureBlendMode(state->texture, SDL_BLENDMODE_NONE) < 0)
  {
    fprintf(stderr, "Error: SDL_SetTextureBlendMode: %s\n", SDL_GetError());
    goto failure;
  }

  return result_OK;


failure:
  return result_TOO_BIG; // incorrect
}

static void stop_sdl(sdlstate_t *state)
{
  if (state == NULL)
    return;

  SDL_DestroyTexture(state->texture);
  SDL_DestroyRenderer(state->renderer);
  SDL_DestroyWindow(state->window);

  SDL_Quit();
}

#endif /* SDL */

/* ----------------------------------------------------------------------- */

typedef struct testfont
{
  const char *filename;
  bmfont_t   *bmfont;
  int         width, height;
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

static bmtestfont_t fonts[] =
{
  { "tiny-font.png",     NULL },
  { "henry-font.png",    NULL },
  { "tall-font.png",     NULL },
  { "ms-sans-serif.png", NULL },
  { "digits-font.png",   NULL }
};

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

static const char *strings[] =
{
  "DPTLib bmfont test!",
  "The five boxing wizards jump quickly.",
  "Jackdaws love my big sphinx of quartz.",
  "Pack my box with five dozen liquor jugs.",
  "A horse that was too yellow moaned devilish odes",
  "The sick person in pyjamas quickly trusted the swarthy driver",
};

/* ----------------------------------------------------------------------- */

result_t bmfont_test(const char *resources)
{
#if 1
  const int        scr_width    = GAMEWIDTH;
  const int        scr_height   = GAMEHEIGHT;
#ifdef TEST_P4
  const pixelfmt_t scr_fmt      = pixelfmt_p4;
  const int        scr_log2bpp  = 2; /* 4bpp */
#else
  const pixelfmt_t scr_fmt      = pixelfmt_bgrx8888;
  const int        scr_log2bpp  = 5; /* 32bpp */
#endif
  const int        scr_rowbytes = (scr_width << scr_log2bpp) / 8;
  const int        scr_bytes    = scr_rowbytes * scr_height;

  const colour_t palette[16] = {
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
  };
  const int background_colour_index = 7; // near white

  const colour_t transparent = colour_rgba(0x00, 0x00, 0x00, 0x00);

  result_t      rc = result_OK;
#ifdef USE_SDL
  sdlstate_t    state;
#endif
  unsigned int *pixels;
  bitmap_t      bm;
  int           bm_inited = 0;
  int           font;
  screen_t      scr;
  bool          quit = false;
  int           frame;

#ifdef USE_SDL
  memset(&state, 0, sizeof(state));

  rc = start_sdl(&state);
  if (rc)
    goto Failure;
#endif

  pixels = malloc(scr_rowbytes * scr_height);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }

  bitmap_init(&bm, scr_width, scr_height, scr_fmt, scr_rowbytes, palette, pixels);
  bm_inited = 1;

  bitmap_clear(&bm, palette[background_colour_index]);

  screen_for_bitmap(&scr, &bm);

  for (font = 0; font < NELEMS(fonts); font++)
  {
    char filename[PATH_MAX];

    strcpy(filename, resources);
    strcat(filename, "/resources/bmfonts/");
    strcat(filename, fonts[font].filename);
    rc = bmfont_create(filename, &fonts[font].bmfont);
    if (rc)
      goto Failure;

    bmfont_get_info(fonts[font].bmfont, &fonts[font].width, &fonts[font].height);
  }

  // test screen clipping
  box_t scrclip = screen_get_clip(&scr);
  box_grow(&scrclip, -37);
  scr.clip = scrclip;

  int mx = 0;
  int my = 0;
  int dontclear = 0;
  int animate = 0;
  int firstdraw = 1;
  int cycling = 1;
  int currfont = 1;
  int transparency = 0;
  box_t prevdirty;

  for (frame = 0; !quit; frame++)
  {
#ifdef USE_SDL
    {
      SDL_Event event;

      // Consume all pending events
      while (SDL_PollEvent(&event))
      {
        switch (event.type)
        {
        case SDL_QUIT:
          quit = 1;
          SDL_Log("Quitting after %i ticks", event.quit.timestamp);
          break;

        case SDL_WINDOWEVENT:
          PrintEvent(&event);
          break;

        case SDL_KEYDOWN:
          break;

        case SDL_KEYUP:
          switch (event.key.keysym.sym)
          {
          case SDLK_c: cycling = !cycling; break;
          case SDLK_f: currfont = (currfont + 1) % NELEMS(fonts); break;
          case SDLK_t: transparency = !transparency; break;
          }
          break;

        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:
          break;

        case SDL_MOUSEMOTION:
          mx = event.motion.x - 32;
          my = event.motion.y - 8;
          break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          {
            switch (event.button.button)
            {
            case SDL_BUTTON_LEFT:
              animate = (event.type == SDL_MOUSEBUTTONDOWN);
              break;
            case SDL_BUTTON_RIGHT:
              dontclear = (event.type == SDL_MOUSEBUTTONDOWN);
              break;
            }
          }
          break;

        case SDL_MOUSEWHEEL:
          break;

        default:
          printf("Unhandled event code {%d}\n", event.type);
          break;
        }
      }
    }
#else
    if (frame > 1000)
      quit = 1;
#endif

    colour_t fg = palette[1];
    colour_t bg = transparency ? transparent : palette[15];

    if (!dontclear)
      bitmap_clear(&bm, palette[background_colour_index]);

    box_t overalldirty;
    box_reset(&overalldirty);

    // clipping test
    {
      point_t   origin  = {mx,my};
      const int height  = fonts[currfont].height;
      const int rows    = scr_height / height;
      box_t     dirty;
      int       i;

      for (i = 0; i < rows; i++)
      {
        const char  *message = strings[i % NELEMS(strings)];
        const size_t msglen  = strlen(message);

        if (animate)
        {
          const double movement_speed = 8.0;

          int    j  = i - (rows / 2);
          double t  = (frame + j) / movement_speed;
          double sx = mx - GAMEWIDTH  / 2.0;
          double sy = my - GAMEHEIGHT / 2.0;
          origin.x  = mx +              sin(t) * sx;
          origin.y  = my + j * height + cos(t) * sy;
          if (cycling)
          {
            fg = palette[( 0 + i + frame / 2) % 16];
            bg = transparency ? transparent : palette[(15 + i + frame / 3) % 16];
          }
        }
        dirty.x0 = origin.x;
        dirty.y0 = origin.y;
        dirty.x1 = origin.x + msglen * 15; // HACK
        dirty.y1 = origin.y + height;
        box_intersection(&dirty, &scrclip, &dirty); /* clamp to screen bounds */
        box_union(&overalldirty, &dirty, &overalldirty);
        (void) bmfont_draw(fonts[currfont].bmfont,
                          &scr,
                           message,
                           msglen,
                           fg, bg,
                          &origin,
                           NULL /*endpos*/);
        if (!animate)
          break;
      }
    }

    /* Update the texture and render it */

#ifdef USE_SDL
    if (!box_is_empty(&overalldirty))
    {
      bitmap_t *scr_bgrx8888;
      SDL_Rect  texturearea;

      if (scr_fmt != pixelfmt_bgrx8888)
        bitmap_convert((const bitmap_t *) &scr, pixelfmt_bgrx8888, &scr_bgrx8888);
      else
        scr_bgrx8888 = (bitmap_t *) &scr;

      if (firstdraw)
      {
        texturearea.x = 0;
        texturearea.y = 0;
        texturearea.w = GAMEWIDTH;
        texturearea.h = GAMEHEIGHT;
        firstdraw = 0;
      }
      else
      {
        box_t uniondirty;

        box_union(&overalldirty, &prevdirty, &uniondirty);

        texturearea.x = uniondirty.x0;
        texturearea.y = uniondirty.y0;
        texturearea.w = uniondirty.x1 - uniondirty.x0;
        texturearea.h = uniondirty.y1 - uniondirty.y0;
      }
      // the rect given here describes where to draw in the texture
      SDL_UpdateTexture(state.texture,
                       &texturearea,
                        (char *) scr_bgrx8888->base + texturearea.y * scr_bgrx8888->rowbytes + texturearea.x * 4,
                        scr_bgrx8888->rowbytes);
    }

    /* Clear screen */
    SDL_SetRenderDrawColor(state.renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE); /* opaque white */
    SDL_RenderClear(state.renderer);
    /* Render texture */
    SDL_RenderCopy(state.renderer, state.texture, NULL, NULL);
    SDL_RenderPresent(state.renderer);

    SDL_Delay(1000 / 60); /* 60fps */
#endif

    prevdirty = overalldirty;
  }

//  bitmap_save_png(&bm, "bmfont-test-979.png");

#ifdef USE_SDL
  stop_sdl(&state);
#endif

#else

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
    char filename[PATH_MAX];

    strcpy(filename, resources);
    strcat(filename, "/resources/bmfonts/");
    strcat(filename, fonts[font].filename);
    rc = bmfont_create(filename, &fonts[font].bmfont);
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
                       "Lorem ipsum dolor sit amet, consectetuer",
                       40,
                       palette[7],
                       palette[11],
                       &origin,
                       NULL /*endpos*/);
      origin.x -= 1;
      origin.y += 16;
    }
  }

  bitmap_save_png(&bm, "bmfont-clipping-test.png");

  // general test
  if (0)
  for (line = &lines[0]; line < &lines[0] + NELEMS(lines); line++)
  {
    bmfont_t   *bmfont    = fonts[line->font_index].bmfont;
    int         glyphwidth, glyphheight;
    const char *string    = line->string;
    size_t      stringlen = strlen(line->string);
    point_t     origin    = line->origin;

    bmfont_get_info(bmfont, &glyphwidth, &glyphheight);

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

#endif

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

// vim: ts=8 sts=2 sw=2 et
