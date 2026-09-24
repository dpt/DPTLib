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
#include "tasks.h" /* g_tasks.swap_mouse_buttons */

/* Screen pixel format for the demo, chosen at run time via --depth: 32 =
 * pixelfmt_bgrx8888 (feeds SDL directly, no per-frame conversion); 8 =
 * pixelfmt_p8; 4 (the default) = pixelfmt_p4 paletted (exercises
 * screen_copy_rect's nibble-packed blit path instead); 2 = pixelfmt_p2; 1 =
 * pixelfmt_p1 monochrome. Paletted depths are converted to bgrx8888 per
 * frame via bitmap_convert. Stashed in struct wuss_frontend so present() can
 * branch on it. */

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
  SDL_Window          *window;
  SDL_Renderer        *renderer;
  SDL_Texture         *texture;
  int                  scr_width;
  int                  scr_height;
  int                  scale; /* device pixels per screen pixel; see WUSS_SDL_*_SCALE */
  int                  depth; /* framebuffer bits per pixel: 32 (bgrx8888), 8 (p8), 4 (p4), 2 (p2) or 1 (p1) */
  void                *pixels; /* the private framebuffer handed to the caller */
  bitmap_t             conv; /* scratch bgrx8888 buffer for present()'s paletted
                               * depths, sized scr_width x scr_height and reused
                               * every frame instead of allocating one per present */
  char                 text[64]; /* pending TEXT_INPUT, UTF-8 */
  const char          *text_pos; /* next code point in text to hand out */
  wuss_key_modifiers_t text_mods;
};

/* ----------------------------------------------------------------------- */

static wuss_button_t sdl_button_to_wuss(Uint8 button)
{
  if (g_tasks.swap_mouse_buttons)
  {
    switch (button)
    {
    case SDL_BUTTON_MIDDLE: return wuss_BUTTON_ADJUST;
    case SDL_BUTTON_RIGHT:  return wuss_BUTTON_MENU;
    default:                return wuss_BUTTON_SELECT;
    }
  }

  switch (button)
  {
  case SDL_BUTTON_MIDDLE: return wuss_BUTTON_MENU;
  case SDL_BUTTON_RIGHT:  return wuss_BUTTON_ADJUST;
  default:                return wuss_BUTTON_SELECT;
  }
}

static wuss_key_modifiers_t sdl_mods_to_wuss(SDL_Keymod mod)
{
  wuss_key_modifiers_t mods;

  mods = wuss_KEY_MOD_NONE;
  if (mod & SDL_KMOD_SHIFT)
    mods |= wuss_KEY_MOD_SHIFT;
  if (mod & SDL_KMOD_CTRL)
    mods |= wuss_KEY_MOD_CTRL;
#ifdef __APPLE__
  if (mod & SDL_KMOD_GUI) /* Cmd plays the role of Ctrl */
    mods |= wuss_KEY_MOD_CTRL;
#endif
  if (mod & SDL_KMOD_ALT)
    mods |= wuss_KEY_MOD_ALT;

  return mods;
}

/* Map a non-printing SDL key to its wuss code, or 0 if it is not one. */
static int sdl_special_key(SDL_Keycode key)
{
  if (key >= SDLK_F1 && key <= SDLK_F12)
    return wuss_KEY_F1 + (int) (key - SDLK_F1);

  switch (key)
  {
  case SDLK_RETURN:    return 13;
  case SDLK_KP_ENTER:  return 13;
  case SDLK_BACKSPACE: return 8;
  case SDLK_TAB:       return 9;
  case SDLK_ESCAPE:    return 27;
  case SDLK_UP:        return wuss_KEY_UP;
  case SDLK_DOWN:      return wuss_KEY_DOWN;
  case SDLK_LEFT:      return wuss_KEY_LEFT;
  case SDLK_RIGHT:     return wuss_KEY_RIGHT;
  case SDLK_HOME:      return wuss_KEY_HOME;
  case SDLK_END:       return wuss_KEY_END;
  case SDLK_PAGEUP:    return wuss_KEY_PAGE_UP;
  case SDLK_PAGEDOWN:  return wuss_KEY_PAGE_DOWN;
  case SDLK_INSERT:    return wuss_KEY_INSERT;
  case SDLK_DELETE:    return wuss_KEY_DELETE;
  default:             return 0;
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

  if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 32)
  {
    fprintf(stderr,
            "Error: unsupported depth %d (want 1, 2, 4, 8 or 32)\n", depth);
    return result_BAD_ARG;
  }

  if (scale <= 0)
    scale = WUSS_SDL_DEFAULT_SCALE;
  scale = CLAMP(scale, WUSS_SDL_MIN_SCALE, WUSS_SDL_MAX_SCALE);

  stride = (width * depth + 7) >> 3;

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

  fe->conv.base = NULL;
  if (depth != 32)
  {
    fe->conv.base = malloc((size_t) width * sizeof(pixelfmt_bgrx8888_t) * height);
    if (fe->conv.base == NULL)
    {
      free(fe->pixels);
      free(fe);
      return result_OOM;
    }
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

#ifndef __EMSCRIPTEN__
  /* Emscripten's keypress-based TEXT_INPUT is unreliable; its KEY_DOWN
   * already carries the composed character (see wuss_frontend_poll) */
  SDL_StartTextInput(fe->window);
#endif

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

  *fmt = (depth == 32) ? pixelfmt_bgrx8888
       : (depth == 8)  ? pixelfmt_p8
       : (depth == 4)  ? pixelfmt_p4
       : (depth == 2)  ? pixelfmt_p2
                       : pixelfmt_p1;

  if (fe->conv.base != NULL)
    bitmap_init(&fe->conv, SIZE2D(width, height), pixelfmt_bgrx8888,
               width * sizeof(pixelfmt_bgrx8888_t), NULL, fe->conv.base);

  *pixels   = fe->pixels;
  *rowbytes = stride;
  *frontend = fe;
  return result_OK;


failure:

  if (fe->texture)  SDL_DestroyTexture(fe->texture);
  if (fe->renderer) SDL_DestroyRenderer(fe->renderer);
  if (fe->window)   SDL_DestroyWindow(fe->window);
  SDL_Quit();
  free(fe->conv.base);
  free(fe->pixels);
  free(fe);
  return result_TEST_FAILED;
}

result_t wuss_frontend_resize(wuss_frontend_t *fe,
                              int              width,
                              int              height,
                              void           **pixels,
                              int             *rowbytes)
{
  int          stride;
  void        *new_pixels;
  void        *new_conv;
  SDL_Texture *new_texture;

  stride = (width * fe->depth + 7) >> 3;

  new_pixels = malloc((size_t) stride * height);
  if (new_pixels == NULL)
    return result_OOM;

  new_conv = NULL;
  if (fe->depth != 32)
  {
    new_conv = malloc((size_t) width * sizeof(pixelfmt_bgrx8888_t) * height);
    if (new_conv == NULL)
    {
      free(new_pixels);
      return result_OOM;
    }
  }

  new_texture = SDL_CreateTexture(fe->renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, width, height);
  if (new_texture == NULL)
  {
    free(new_conv);
    free(new_pixels);
    return result_TEST_FAILED;
  }
  SDL_SetTextureBlendMode(new_texture, SDL_BLENDMODE_NONE);
  SDL_SetTextureScaleMode(new_texture, SDL_SCALEMODE_NEAREST);

  SDL_DestroyTexture(fe->texture);
  free(fe->pixels);
  free(fe->conv.base);

  fe->texture    = new_texture;
  fe->pixels     = new_pixels;
  fe->scr_width  = width;
  fe->scr_height = height;

  fe->conv.base = new_conv;
  if (new_conv != NULL)
    bitmap_init(&fe->conv, SIZE2D(width, height), pixelfmt_bgrx8888,
               width * sizeof(pixelfmt_bgrx8888_t), NULL, new_conv);

  SDL_SetWindowSize(fe->window, width * fe->scale, height * fe->scale);

  *pixels   = fe->pixels;
  *rowbytes = stride;
  return result_OK;
}

bool wuss_frontend_poll(wuss_frontend_t *fe, wuss_input_t *event)
{
  SDL_Event ev;

  for (;;)
  {
    /* hand out a TEXT_INPUT string one code point per call */
    if (fe->text_pos != NULL && *fe->text_pos != '\0')
    {
      event->kind = wuss_INPUT_KEY;
      event->key  = (int) SDL_StepUTF8(&fe->text_pos, NULL);
      event->mods = fe->text_mods;
      return true;
    }

    if (!SDL_PollEvent(&ev))
      return false;

    switch (ev.type)
    {
    case SDL_EVENT_QUIT:
      event->kind = wuss_INPUT_QUIT;
      return true;

    case SDL_EVENT_KEY_DOWN:
      {
        wuss_key_modifiers_t mods;
        int                  key;

        mods = sdl_mods_to_wuss(ev.key.mod);
        key  = sdl_special_key(ev.key.key);
        /* Plain printables arrive via TEXT_INPUT; only take them here when
         * Ctrl or Alt turns them into a command. */
        if (key == 0 &&
            (mods & (wuss_KEY_MOD_CTRL | wuss_KEY_MOD_ALT)) &&
            !(ev.key.key & SDLK_SCANCODE_MASK))
          key = (int) ev.key.key;
#ifdef __EMSCRIPTEN__
        /* no TEXT_INPUT here: SDL keymaps the browser's KeyboardEvent.key
         * under the current modifiers, but ev.key.key is the unshifted key,
         * so look up the shifted/composed one */
        if (key == 0)
        {
          SDL_Keycode k;

          k = SDL_GetKeyFromScancode(ev.key.scancode, ev.key.mod, false);
          if (k >= ' ' && k != 127 && !(k & SDLK_SCANCODE_MASK))
            key = (int) k;
        }
#endif
        if (key == 0)
          continue;

        event->kind = wuss_INPUT_KEY;
        event->key  = key;
        event->mods = mods;
      }
      return true;

    case SDL_EVENT_TEXT_INPUT:
      /* KEY_DOWN already sent Ctrl/Alt combinations (and on macOS Alt would
       * compose a different character here): drop the duplicate. */
      if (sdl_mods_to_wuss(SDL_GetModState()) &
          (wuss_KEY_MOD_CTRL | wuss_KEY_MOD_ALT))
        continue;

      /* ponytail: text past sizeof(fe->text) is dropped; IME bursts that
       * long are not expected in the demo */
      SDL_strlcpy(fe->text, ev.text.text, sizeof(fe->text));
      fe->text_pos  = fe->text;
      fe->text_mods = sdl_mods_to_wuss(SDL_GetModState());
      continue;

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
        event->wheel = g_tasks.reverse_scroll ? -(int) ev.wheel.y : (int) ev.wheel.y;
      }
      return true;

    default:
      continue;
    }
  }
}

void wuss_frontend_present(wuss_frontend_t *fe,
                           const bitmap_t  *bm,
                           const box_t     *dirty)
{
  if (fe->depth == 32)
  {
    SDL_UpdateTexture(fe->texture, NULL, bm->base, bm->rowbytes);
  }
  else
  {
    result_t rc;
    bitmap_t rows, out;
    int      y0, y1;
    SDL_Rect rect;

    /* Sub-byte formats (p1/p2/p4) pack several pixels per byte, so only a
     * whole-row crop is safe without redoing their bit-unpacking maths for an
     * arbitrary x0; a dirty rect just narrows which rows get converted. */
    y0 = (dirty != NULL) ? CLAMP(dirty->y0, 0, bm->size.h) : 0;
    y1 = (dirty != NULL) ? CLAMP(dirty->y1, 0, bm->size.h) : bm->size.h;
    if (y1 <= y0)
      goto present;

    rc = bitmap_init(&rows,
                     SIZE2D(bm->size.w, y1 - y0),
                     bm->format,
                     bm->rowbytes,
                     bm->palette,
                     (unsigned char *) bm->base + y0 * bm->rowbytes);
    if (rc != result_OK)
      goto present;

    /* wuss draws into a paletted bitmap; SDL wants bgrx. bitmap_convert_into
     * reads the palette straight off `bm`, which the caller updates when the
     * palette task's picker menu changes it, so a live palette change just
     * shows up in the next converted frame. `fe->conv`'s buffer is sized for
     * the full screen, so any dirty-row subset fits; only its size/rowbytes
     * need to match this call's row count. */
    bitmap_init(&out, rows.size, pixelfmt_bgrx8888,
               bm->size.w * sizeof(pixelfmt_bgrx8888_t), NULL, fe->conv.base);

    if (bitmap_convert_into(&rows, pixelfmt_bgrx8888, &out) == result_OK)
    {
      rect.x = 0;
      rect.y = y0;
      rect.w = bm->size.w;
      rect.h = y1 - y0;

      SDL_UpdateTexture(fe->texture, &rect, out.base, out.rowbytes);
    }
  }

present:
  SDL_RenderTexture(fe->renderer, fe->texture, NULL, NULL);
  SDL_RenderPresent(fe->renderer);

  SDL_Delay(1000 / 60);
}

void wuss_frontend_zoom(wuss_frontend_t *fe, int delta)
{
  int scale;

  scale = CLAMP(fe->scale + delta, WUSS_SDL_MIN_SCALE, WUSS_SDL_MAX_SCALE);
  if (scale == fe->scale)
    return;

  fe->scale = scale;
  SDL_SetWindowSize(fe->window, fe->scr_width * scale, fe->scr_height * scale);
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

  free(fe->conv.base);
  free(fe->pixels);
  free(fe);
}

#endif /* USE_SDL */
#endif /* WUSS_APP */
