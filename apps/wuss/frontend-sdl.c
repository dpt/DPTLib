/* wuss/frontend-sdl.c -- SDL backend for the Wuss interactive demo */

#ifdef WUSS_APP
#ifdef USE_SDL

#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/point.h"
#include "wuss/wuss.h"

#include <SDL3/SDL.h>

#include "frontend.h"

/* Screen pixel format for the demo, chosen at run time via -depth: 32 =
 * pixelfmt_bgrx8888 (feeds SDL directly, no per-frame conversion); 4 (the
 * default) = pixelfmt_p4 paletted (exercises screen_copy_rect's nibble-packed
 * blit path instead). No other depth is handled yet. Stashed in struct
 * wuss_frontend so present() can branch on it. */

/* Integer window zoom: the fixed Wuss screen is drawn at this many device
 * pixels per screen pixel. The initial value comes from -scale (0 => use the
 * default here); F2 steps it up, Shift-F2 down, clamped to
 * [WUSS_SDL_MIN_SCALE, WUSS_SDL_MAX_SCALE]. */
#define WUSS_SDL_DEFAULT_SCALE 2
#define WUSS_SDL_MIN_SCALE     1
#define WUSS_SDL_MAX_SCALE     4

/* ----------------------------------------------------------------------- */

struct wuss_frontend
{
  SDL_Window   *window;
  SDL_Renderer *renderer;
  SDL_Texture  *texture;
  int           scr_width;
  int           scr_height;
  int           scale; /* device pixels per screen pixel; see WUSS_SDL_*_SCALE */
  int           depth; /* framebuffer bits per pixel: 32 (bgrx8888) or 4 (p4) */
  void         *pixels; /* the private framebuffer handed to the caller */
};

/* ----------------------------------------------------------------------- */

static wuss_button_t sdl_button_to_wuss(Uint8 button)
{
  switch (button)
  {
  case SDL_BUTTON_MIDDLE: return wuss_BUTTON_MENU;
  case SDL_BUTTON_RIGHT:  return wuss_BUTTON_ADJUST;
  default:                return wuss_BUTTON_SELECT;
  }
}

/* SDL delivers mouse coordinates in window space, which F2 can scale away
 * from the fixed-size Wuss screen; map back down to screen space. */
static void sdl_pos_to_scr(SDL_Window *window,
                           int         scr_width,
                           int         scr_height,
                           float       in_x,
                           float       in_y,
                           int        *out_x,
                           int        *out_y)
{
  int win_w, win_h;

  SDL_GetWindowSize(window, &win_w, &win_h);

  *out_x = (int) (in_x * scr_width  / win_w);
  *out_y = (int) (in_y * scr_height / win_h);
}

/* ----------------------------------------------------------------------- */

result_t wuss_frontend_open(int               width,
                            int               height,
                            const colour_t   *palette,
                            int               npalette,
                            int               depth,
                            int               scale,
                            void            **pixels,
                            int              *rowbytes,
                            pixelfmt_t       *fmt,
                            wuss_frontend_t **frontend)
{
  wuss_frontend_t *fe;
  int              stride;

  NOT_USED(palette);
  NOT_USED(npalette);

  if (depth != 4 && depth != 32)
  {
    fprintf(stderr, "Error: unsupported -depth %d (want 4 or 32)\n", depth);
    return result_BAD_ARG;
  }

  if (scale <= 0)
    scale = WUSS_SDL_DEFAULT_SCALE;
  scale = CLAMP(scale, WUSS_SDL_MIN_SCALE, WUSS_SDL_MAX_SCALE);

  stride = (depth == 32) ? width * 4  /* pixelfmt_bgrx8888: 4 bytes/pixel */
                         : width / 2; /* pixelfmt_p4: 2 pixels/byte */

  fe = calloc(1, sizeof(*fe));
  if (fe == NULL)
    return result_OOM;

  fe->scr_width  = width;
  fe->scr_height = height;
  fe->scale      = scale;
  fe->depth      = depth;

  fe->pixels = malloc((size_t) stride * height);
  if (fe->pixels == NULL)
  {
    free(fe);
    return result_OOM;
  }

  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    fprintf(stderr, "Error: SDL_Init: %s\n", SDL_GetError());
    goto failure;
  }

  fe->window = SDL_CreateWindow("Wuss", width * fe->scale, height * fe->scale,
                                0);
  if (fe->window == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateWindow: %s\n", SDL_GetError());
    goto failure;
  }

  fe->renderer = SDL_CreateRenderer(fe->window, NULL);
  if (fe->renderer == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateRenderer: %s\n", SDL_GetError());
    goto failure;
  }

  fe->texture = SDL_CreateTexture(fe->renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, width, height);
  if (fe->texture == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateTexture: %s\n", SDL_GetError());
    goto failure;
  }

  SDL_SetTextureBlendMode(fe->texture, SDL_BLENDMODE_NONE);
  /* keep pixels crisp when F2 scales the window up */
  SDL_SetTextureScaleMode(fe->texture, SDL_SCALEMODE_NEAREST);

  *fmt = (depth == 32) ? pixelfmt_bgrx8888 : pixelfmt_p4;

  *pixels   = fe->pixels;
  *rowbytes = stride;
  *frontend = fe;
  return result_OK;


failure:

  if (fe->texture)  SDL_DestroyTexture(fe->texture);
  if (fe->renderer) SDL_DestroyRenderer(fe->renderer);
  if (fe->window)   SDL_DestroyWindow(fe->window);
  SDL_Quit();
  free(fe->pixels);
  free(fe);
  return result_TEST_FAILED;
}

bool wuss_frontend_poll(wuss_frontend_t *fe, wuss_input_t *event)
{
  SDL_Event ev;

  for (;;)
  {
    if (!SDL_PollEvent(&ev))
      return false;

    switch (ev.type)
    {
    case SDL_EVENT_QUIT:
      event->kind = wuss_INPUT_QUIT;
      return true;

    case SDL_EVENT_KEY_UP:
      if (ev.key.key == SDLK_F4)
        event->kind = wuss_INPUT_QUIT;
      else if (ev.key.key == SDLK_F1 && (ev.key.mod & SDL_KMOD_SHIFT))
        event->kind = wuss_INPUT_GARBAGE;
      else if (ev.key.key == SDLK_F1)
        event->kind = wuss_INPUT_REDRAW_ALL;
      else if (ev.key.key == SDLK_F3)
        event->kind = wuss_INPUT_PIXEL_STRESS;
      else if (ev.key.key == SDLK_F2)
      {
        int scale;

        /* F2 steps the SDL window zoom up, Shift-F2 down, clamped to
         * [WUSS_SDL_MIN_SCALE, WUSS_SDL_MAX_SCALE]: a backend-local zoom the
         * demo loop never sees. */
        scale = fe->scale + ((ev.key.mod & SDL_KMOD_SHIFT) ? -1 : 1);
        if (scale < WUSS_SDL_MIN_SCALE)
          scale = WUSS_SDL_MIN_SCALE;
        else if (scale > WUSS_SDL_MAX_SCALE)
          scale = WUSS_SDL_MAX_SCALE;
        if (scale != fe->scale)
        {
          fe->scale = scale;
          SDL_SetWindowSize(fe->window, fe->scr_width * scale,
                            fe->scr_height * scale);
        }
        continue;
      }
      else
        continue;
      return true;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      {
        int x, y;

        sdl_pos_to_scr(fe->window, fe->scr_width, fe->scr_height,
                       ev.button.x, ev.button.y, &x, &y);
        event->kind   = wuss_INPUT_MOUSE_DOWN;
        event->pos    = POINT(x, y);
        event->button = sdl_button_to_wuss(ev.button.button);
      }
      return true;

    case SDL_EVENT_MOUSE_BUTTON_UP:
      {
        int x, y;

        sdl_pos_to_scr(fe->window, fe->scr_width, fe->scr_height,
                       ev.button.x, ev.button.y, &x, &y);
        event->kind   = wuss_INPUT_MOUSE_UP;
        event->pos    = POINT(x, y);
        event->button = sdl_button_to_wuss(ev.button.button);
      }
      return true;

    case SDL_EVENT_MOUSE_MOTION:
      {
        int x, y;

        sdl_pos_to_scr(fe->window, fe->scr_width, fe->scr_height,
                       ev.motion.x, ev.motion.y, &x, &y);
        event->kind = wuss_INPUT_MOUSE_MOVE;
        event->pos  = POINT(x, y);
      }
      return true;

    case SDL_EVENT_MOUSE_WHEEL:
      {
        int x, y;

        sdl_pos_to_scr(fe->window, fe->scr_width, fe->scr_height,
                       ev.wheel.mouse_x, ev.wheel.mouse_y, &x, &y);
        event->kind  = wuss_INPUT_WHEEL;
        event->pos   = POINT(x, y);
        event->wheel = (int) ev.wheel.y;
      }
      return true;

    default:
      continue;
    }
  }
}

void wuss_frontend_present(wuss_frontend_t *fe, const bitmap_t *bm)
{
  if (fe->depth == 32)
  {
    SDL_UpdateTexture(fe->texture, NULL, bm->base, bm->rowbytes);
  }
  else
  {
    bitmap_t *disp;

    /* wuss draws into a paletted bitmap; SDL wants bgrx. bitmap_convert reads
     * the palette straight off `bm`, which the caller updates when the palette
     * task's picker menu changes it, so a live palette change just shows up in
     * the next converted frame. */
    if (bitmap_convert(bm, pixelfmt_bgrx8888, &disp) == result_OK)
    {
      SDL_UpdateTexture(fe->texture, NULL, disp->base, disp->rowbytes);
      free(disp->base);
      free(disp);
    }
  }

  SDL_RenderTexture(fe->renderer, fe->texture, NULL, NULL);
  SDL_RenderPresent(fe->renderer);

  SDL_Delay(1000 / 60);
}

void wuss_frontend_set_palette(wuss_frontend_t *fe,
                               const colour_t  *palette,
                               int              npalette)
{
  NOT_USED(fe);
  NOT_USED(palette);
  NOT_USED(npalette);
  /* SDL has no physical palette: the p4 -> bgrx conversion in present() reads
   * the palette straight off the caller's bitmap_t, which it has already
   * updated. Nothing to do here. */
}

void wuss_frontend_close(wuss_frontend_t *fe)
{
  if (fe == NULL)
    return;

  SDL_DestroyTexture(fe->texture);
  SDL_DestroyRenderer(fe->renderer);
  SDL_DestroyWindow(fe->window);
  SDL_Quit();

  free(fe->pixels);
  free(fe);
}

#endif /* USE_SDL */
#endif /* WUSS_APP */
