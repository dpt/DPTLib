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

const int GAMEWIDTH  = 800;
const int GAMEHEIGHT = 600;

/* ----------------------------------------------------------------------- */

#define palette_BLACK       (0)
#define palette_DARK_GREEN  (3)
#define palette_WHITE       (7)
#define palette_GREEN      (11)

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

typedef struct bmtestfont
{
  const char   *filename;
  bmfont_t     *bmfont;
  int           width, height;
}
bmtestfont_t;

typedef struct bmtestline
{
  int           font_index;
  unsigned char fg, bg; /* palette indices */
  point_t       origin;
  const char   *string;
}
bmtestline_t;

/* ----------------------------------------------------------------------- */

#define MAXFONTS 5

static bmtestfont_t bmfonts[MAXFONTS] =
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

static const char *pangrams[] =
{
  "A quart jar of oil mixed with zinc oxide makes a very bright paint.",
  "A quick movement of the enemy will jeopardize six gunboats.",
  "A wizard's job is to vex chumps quickly in fog.",
  "Amazingly few discotheques provide jukeboxes.",
  "Few black taxis drive up major roads on quiet hazy nights.",
  "Jack quietly moved up front and seized the big ball of wax.",
  "Jackdaws love my big sphinx of quartz.",
  "Just keep examining every low bid quoted for zinc etchings.",
  "Just work for improved basic techniques to maximize your typing skill.",
  "My girl wove six dozen plaid jackets before she quit.",
  "Pack my box with five dozen liquor jugs.",
  "Six big devils from Japan quickly forgot how to waltz.",
  "Six boys guzzled cheap raw plum vodka quite joyfully.",
  "Sixty zippers were quickly picked from the woven jute bag.",
  "The five boxing wizards jump quickly.",
  "The public was amazed to view the quickness and dexterity of the juggler.",
  "The quick brown fox jumps over the lazy dog.",
  "We promptly judged antique ivory buckles for the next prize.",
  "Whenever the black fox jumped the squirrel gazed suspiciously."
};

/* ----------------------------------------------------------------------- */

typedef struct bmfontteststate
{
  int         scr_width, scr_height;
  bitmap_t    bm;
  screen_t    scr;
  colour_t    palette[16];
  colour_t    transparent;
  int         background_colour_index;
#ifdef USE_SDL
  sdlstate_t  sdl_state;
#endif
}
bmfontteststate_t;

static result_t bmfont_clipping_test(bmfontteststate_t *state)
{
  static const point_t centres[] =
  {
    {  0            , GAMEHEIGHT * 0 / 4 },
    {  GAMEWIDTH / 2, GAMEHEIGHT * 0 / 4 },
    {  GAMEWIDTH    , GAMEHEIGHT * 0 / 4 },

    {  0            , GAMEHEIGHT * 1 / 4 },
    {  GAMEWIDTH / 2, GAMEHEIGHT * 1 / 4 },
    {  GAMEWIDTH    , GAMEHEIGHT * 1 / 4 },

    {  0            , GAMEHEIGHT * 2 / 4 },
    {  GAMEWIDTH / 2, GAMEHEIGHT * 2 / 4 },
    {  GAMEWIDTH    , GAMEHEIGHT * 2 / 4 },

    {  0            , GAMEHEIGHT * 3 / 4 },
    {  GAMEWIDTH / 2, GAMEHEIGHT * 3 / 4 },
    {  GAMEWIDTH    , GAMEHEIGHT * 3 / 4 },

    {  0            , GAMEHEIGHT * 4 / 4 },
    {  GAMEWIDTH / 2, GAMEHEIGHT * 4 / 4 },
    {  GAMEWIDTH    , GAMEHEIGHT * 4 / 4 },
  };

  result_t rc = result_OK;
  int      font;
  int      transparent;
  char     filename[256];

  for (font = 0; font < MAXFONTS; font++)
  {
    for (transparent = 0; transparent < 2; transparent++)
    {
      int fontwidth, fontheight;
      int i;

      bitmap_clear(&state->bm, state->palette[palette_DARK_GREEN]);

      bmfont_get_info(bmfonts[font].bmfont, &fontwidth, &fontheight);

      for (i = 0; i < NELEMS(centres); i++)
      {
        const int nchars = 26;
        int       stringwidth;
        point_t   pos;
        colour_t  bg;

        rc = bmfont_measure(bmfonts[font].bmfont,
                            lorem_ipsum,
                            nchars,
                            INT_MAX,
                            NULL,
                           &stringwidth);
        if (rc)
          return rc;

        pos.x = centres[i].x - stringwidth / 2 + 1;
        pos.y = centres[i].y - fontheight  / 2 + 1;

        bg = transparent ? state->transparent : state->palette[palette_GREEN];

        if (transparent)
        {
          rc = bmfont_draw(bmfonts[font].bmfont,
                          &state->scr,
                           lorem_ipsum,
                           nchars,
                           state->palette[palette_BLACK],
                           bg,
                          &pos,
                           NULL /*endpos*/);
          if (rc)
            return rc;
        }

        pos.x--;
        pos.y--;

        rc = bmfont_draw(bmfonts[font].bmfont,
                        &state->scr,
                         lorem_ipsum,
                         nchars,
                         state->palette[palette_WHITE],
                         bg,
                        &pos,
                         NULL /*endpos*/);
        if (rc)
          return rc;
      }

      sprintf(filename,
              "bmfont-clipping-font-%d%s-%dbpp.png",
              font,
              transparent ? "-transparent" : "-filled",
              1 << pixelfmt_log2bpp(state->scr.format));
      bitmap_save_png(&state->bm, filename);
    }
  }

  return result_TEST_PASSED;
}

static result_t bmfont_layout_test(bmfontteststate_t *state)
{
  result_t rc = result_OK;
  int      font;
  int      transparent;

  for (font = 0; font < MAXFONTS; font++)
  {
    for (transparent = 0; transparent < 2; transparent++)
    {
      bmfont_t   *bmfont    = bmfonts[font].bmfont;
      int         glyphwidth, glyphheight;
      const char *string;
      size_t      stringlen;
      point_t     origin    = {0,0};
      char        filename[256];

      bitmap_clear(&state->bm, state->palette[palette_DARK_GREEN]);

      bmfont_get_info(bmfont, &glyphwidth, &glyphheight);

      for (;;)
      {
        string    = lorem_ipsum;
        stringlen = strlen(lorem_ipsum);

        do
        {
          int            absolute_break;
          int            friendly_break;
          bmfont_width_t width;
          point_t        endpos;
          colour_t       bg;
          int newline = 0;

          (void) bmfont_measure(bmfont,
                                string,
                                stringlen,
                                state->scr_width - origin.x,
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

            newline = 1;
          }

          bg = transparent ? state->transparent : state->palette[palette_GREEN];

          rc = bmfont_draw(bmfont,
                          &state->scr,
                           string,
                           friendly_break,
                           state->palette[palette_WHITE],
                           bg,
                          &origin,
                          &endpos);
          if (rc)
            return rc;

          if (newline)
          {
            origin.x = 0;
            origin.y += glyphheight + 1; /* 1 => leading */
            if (origin.y >= GAMEHEIGHT)
              goto stop;
          }
          else
          {
            origin.x = endpos.x;
          }

          string    += friendly_break;
          stringlen -= friendly_break;
        }
        while (stringlen > 0);
      }

    stop:
      sprintf(filename,
              "bmfont-layout-font-%d%s-%dbpp.png",
              font,
              transparent ? "-transparent" : "-filled",
              1 << pixelfmt_log2bpp(state->scr.format));
      bitmap_save_png(&state->bm, filename);
    }
  }

  return result_TEST_PASSED;
}

static result_t bmfont_interactive_test(bmfontteststate_t *state)
{
  bool  quit = false;
  int   frame;

  // test screen clipping
  box_t scrclip = screen_get_clip(&state->scr);
  box_grow(&scrclip, -37);
  state->scr.clip = scrclip;

  int   mx = 0;
  int   my = 0;
  int   dontclear = 0;
  int   animate = 0;
  int   firstdraw = 1;
  int   cycling = 1;
  int   currfont = 1;
  int   transparency = 0;
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
          case SDLK_f: currfont = (currfont + 1) % NELEMS(bmfonts); break;
          case SDLK_t: transparency = !transparency; break;
          case SDLK_q: quit = true; break;
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

    colour_t fg = state->palette[1];
    colour_t bg = transparency ? state->transparent : state->palette[15];

    if (!dontclear)
      bitmap_clear(&state->bm, state->palette[state->background_colour_index]);

    box_t overalldirty;
    box_reset(&overalldirty);

    {
      point_t   origin  = {mx,my};
      const int height  = bmfonts[currfont].height;
      const int rows    = state->scr_height / height;
      box_t     dirty;
      int       i;

      for (i = 0; i < rows; i++)
      {
        const char  *message = pangrams[i % NELEMS(pangrams)];
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
            fg = state->palette[( 0 + i + frame / 2) % 16];
            bg = transparency ? state->transparent : state->palette[(15 + i + frame / 3) % 16];
          }
        }
        dirty.x0 = origin.x;
        dirty.y0 = origin.y;
        dirty.x1 = origin.x + msglen * 15; // HACK
        dirty.y1 = origin.y + height;
        box_intersection(&dirty, &scrclip, &dirty); /* clamp to screen bounds */
        box_union(&overalldirty, &dirty, &overalldirty);
        (void) bmfont_draw(bmfonts[currfont].bmfont,
                          &state->scr,
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

      if (state->scr.format != pixelfmt_bgrx8888)
        bitmap_convert((const bitmap_t *) &state->scr, pixelfmt_bgrx8888, &scr_bgrx8888);
      else
        scr_bgrx8888 = (bitmap_t *) &state->scr;

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
      SDL_UpdateTexture(state->sdl_state.texture,
                       &texturearea,
                        (char *) scr_bgrx8888->base + texturearea.y * scr_bgrx8888->rowbytes + texturearea.x * 4,
                        scr_bgrx8888->rowbytes);
    }

//    /* Clear screen */
//    SDL_SetRenderDrawColor(state.renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE); /* opaque white */
//    SDL_RenderClear(state.renderer);
    /* Render texture */
    SDL_RenderCopy(state->sdl_state.renderer, state->sdl_state.texture, NULL, NULL);
    SDL_RenderPresent(state->sdl_state.renderer);

    SDL_Delay(1000 / 60); /* 60fps */
#endif

    prevdirty = overalldirty;
  }

#ifdef USE_SDL
  stop_sdl(&state->sdl_state);
#endif

  return result_TEST_PASSED;
}

result_t bmfont_test_one_format(const char *resources,
                                pixelfmt_t  scr_fmt,
                                int         scr_log2bpp)
{
  bmfontteststate_t state;

  state.scr_width  = GAMEWIDTH;
  state.scr_height = GAMEHEIGHT;

  const int scr_rowbytes = (state.scr_width << scr_log2bpp) / 8;

  state.palette[ 0] = colour_rgb(0x00, 0x00, 0x00);
  state.palette[ 1] = colour_rgb(0x1D, 0x2B, 0x53);
  state.palette[ 2] = colour_rgb(0x7E, 0x25, 0x53);
  state.palette[ 3] = colour_rgb(0x00, 0x87, 0x51);
  state.palette[ 4] = colour_rgb(0xAB, 0x52, 0x36);
  state.palette[ 5] = colour_rgb(0x5F, 0x57, 0x4F);
  state.palette[ 6] = colour_rgb(0xC2, 0xC3, 0xC7);
  state.palette[ 7] = colour_rgb(0xFF, 0xF1, 0xE8);
  state.palette[ 8] = colour_rgb(0xFF, 0x00, 0x4D);
  state.palette[ 9] = colour_rgb(0xFF, 0xA3, 0x00);
  state.palette[10] = colour_rgb(0xFF, 0xEC, 0x27);
  state.palette[11] = colour_rgb(0x00, 0xE4, 0x36);
  state.palette[12] = colour_rgb(0x29, 0xAD, 0xFF);
  state.palette[13] = colour_rgb(0x83, 0x76, 0x9C);
  state.palette[14] = colour_rgb(0xFF, 0x77, 0xA8);
  state.palette[15] = colour_rgb(0xFF, 0xCC, 0xAA);

  state.transparent = colour_rgba(0x00, 0x00, 0x00, 0x00);

  state.background_colour_index = 7; /* near white */

  result_t      rc = result_OK;
  unsigned int *pixels;
  int           bm_inited = 0;
  int           font;

#ifdef USE_SDL
  memset(&state.sdl_state, 0, sizeof(state.sdl_state));

  rc = start_sdl(&state.sdl_state);
  if (rc)
    goto Failure;
#endif

  pixels = malloc(scr_rowbytes * state.scr_height);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }

  bitmap_init(&state.bm,
               state.scr_width,
               state.scr_height,
               scr_fmt,
               scr_rowbytes,
               state.palette,
               pixels);
  bm_inited = 1;

  bitmap_clear(&state.bm, state.palette[state.background_colour_index]);

  screen_for_bitmap(&state.scr, &state.bm);

  for (font = 0; font < NELEMS(bmfonts); font++)
  {
    char filename[PATH_MAX];

    strcpy(filename, resources);
    strcat(filename, "/resources/bmfonts/");
    strcat(filename, bmfonts[font].filename);
    rc = bmfont_create(filename, &bmfonts[font].bmfont);
    if (rc)
      goto Failure;

    bmfont_get_info(bmfonts[font].bmfont, &bmfonts[font].width, &bmfonts[font].height);
  }

  /* ------------------------------------------------------------------------ */

  typedef result_t (*bmfonttestfn_t)(bmfontteststate_t *);

  static const bmfonttestfn_t tests[] =
  {
    bmfont_clipping_test,
    bmfont_layout_test,
    bmfont_interactive_test
  };

  int test;

  for (test = 0; test < NELEMS(tests); test++)
  {
    rc = tests[test](&state);
    if (rc != result_TEST_PASSED)
      goto Failure;
  }

  /* ------------------------------------------------------------------------ */

Cleanup:
  for (int f = 0; f < NELEMS(bmfonts); f++)
    bmfont_destroy(bmfonts[f].bmfont);

  if (bm_inited)
    free(state.bm.base);

  return rc;


Failure:
  fprintf(stderr, "error: %x\n", rc);
  rc = result_TEST_FAILED;
  goto Cleanup;
}

result_t bmfont_test(const char *resources)
{
  static const struct
  {
    pixelfmt_t fmt;
    int        log2bpp;
  }
  tab[] =
  {
    { pixelfmt_p4,       2 },
    { pixelfmt_bgrx8888, 5 }
  };

  result_t rc;
  int      i;

  for (i = 0; i < NELEMS(tab); i++)
  {
    rc = bmfont_test_one_format(resources, tab[i].fmt, tab[i].log2bpp);
    if (rc != result_TEST_PASSED)
      return rc;
  }

  return rc;
}

/* vim: set ts=8 sts=2 sw=2 et: */
