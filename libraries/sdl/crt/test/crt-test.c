/* crt-test.c -- check the CRT shaders build and draw */

/* A program of its own rather than part of DPTLibTest, since this library
 * doesn't link DPTLib. It needs a display and a GPU. */

#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "sdl/crt.h"

#define WIDTH  (32)
#define HEIGHT (24)

int main(int argc, char *argv[])
{
  static const crt_params_t params =
  {
    0.025f, 0.5f, 0.025f, 1.1f, 1.1f, 1.0f, 0.75f, 0.3f, 0.6f, 1.0f
  };
  static const crt_format_t formats[] =
  {
    crt_FORMAT_RGBX8888,
    crt_FORMAT_BGRX8888
  };
  static Uint32 pixels[WIDTH * HEIGHT];

  SDL_Window   *window;
  SDL_Rect      viewport;
  int           f;
  crt_t        *crt;
  int           frame;
  int           size;

  (void) argc;
  (void) argv;

  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }

  window = SDL_CreateWindow("crt-test", 128, 96, 0);
  if (window == NULL)
  {
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  viewport.x = 8;
  viewport.y = 8;
  viewport.w = 112;
  viewport.h = 80;

  /* Each format, drawn at two sizes: a size change remakes the texture. */
  for (f = 0; f < 2; f++)
  {
    crt = crt_create(window, formats[f]);
    if (crt == NULL)
    {
      fprintf(stderr, "crt_create failed for format %d\n", f);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    for (frame = 0; frame < 4; frame++)
    {
      size = (frame < 2) ? 1 : 2;
      crt_render(crt, window, pixels, WIDTH / size, HEIGHT / size, &viewport,
                 &params);
    }

    crt_destroy(crt, window);
  }

  crt_destroy(NULL, window);

  SDL_DestroyWindow(window);
  SDL_Quit();

  printf("crt-test passed\n");
  return 0;
}
