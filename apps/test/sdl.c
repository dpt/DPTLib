#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/bmfont.h"

/* ----------------------------------------------------------------------- */

const int GAMEWIDTH  = 800;
const int GAMEHEIGHT = 600;

/* ----------------------------------------------------------------------- */

// TODO: Solve this absolute path....
#define PATH "/Users/dave/SyncProjects/github/DPTLib/" // ick.

/* ----------------------------------------------------------------------- */

typedef struct sdlstate
{
  SDL_Window   *window;
  SDL_Renderer *renderer;
  SDL_Texture  *texture;
}
sdlstate_t;

void PrintEvent(const SDL_Event * event)
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

/* ----------------------------------------------------------------------- */

typedef struct testfont
{
  const char *filename;
  bmfont_t   *bmfont;
  int         width, height;
}
bmtestfont_t;

/* ----------------------------------------------------------------------- */

static bmtestfont_t fonts[] =
{
  { PATH "tiny-font.png",     NULL },
  { PATH "henry-font.png",    NULL },
  { PATH "tall-font.png",     NULL },
  { PATH "ms-sans-serif.png", NULL },
  { PATH "digits-font.png",   NULL }
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

int main(void)
{
  const int        scr_width    = GAMEWIDTH;
  const int        scr_height   = GAMEHEIGHT;
  const pixelfmt_t scr_fmt      = pixelfmt_p4; // pixelfmt_bgrx8888;
  const int        scr_log2bpp  = 2; /* 4bpp */
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
  sdlstate_t    state;
  unsigned int *pixels;
  bitmap_t      bm;
  int           bm_inited = 0;
  int           font;
  screen_t      scr;
  bool          quit = false;
  int           frame;

  memset(&state, 0, sizeof(state));

  rc = start_sdl(&state);
  if (rc)
    goto failure;

  pixels = malloc(scr_rowbytes * scr_height);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto failure;
  }

  bitmap_init(&bm, scr_width, scr_height, scr_fmt, scr_rowbytes, palette, pixels);
  bm_inited = 1;

  bitmap_clear(&bm, palette[background_colour_index]);

  for (font = 0; font < NELEMS(fonts); font++)
  {
    rc = bmfont_create(fonts[font].filename, &fonts[font].bmfont);
    if (rc)
      goto failure;

    bmfont_info(fonts[font].bmfont, &fonts[font].width, &fonts[font].height);
  }

  screen_for_bitmap(&scr, &bm);

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

    colour_t fg = palette[ 1];
    colour_t bg = transparency ? transparent : palette[15];

    if (!dontclear)
      bitmap_clear(&bm, palette[background_colour_index]);

    box_t overalldirty;
    box_reset(&overalldirty);

    // clipping test
    {
      point_t      origin  = {mx,my};
      const int    height  = fonts[currfont].height;
      const int    rows    = scr_height / height;
      box_t        dirty;
      int          i;

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

    SDL_Delay(1000 / 60); // 60fps

    prevdirty = overalldirty;
  }

  stop_sdl(&state);

  printf("(quit)\n");

  exit(EXIT_SUCCESS);


failure:
  printf("(error rd=%d)\n", rc);

  exit(EXIT_FAILURE);
}

// vim: ts=8 sts=2 sw=2 et
