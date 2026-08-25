/* wuss-test.c -- wuss - minimal window manager */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "framebuf/palettes.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/window.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

#ifdef USE_SDL

#include <SDL3/SDL.h>

#include "clients/ball.h"
#include "clients/blank.h"
#include "clients/image.h"
#include "clients/palette.h"
#include "clients/text.h"

/* Screen pixel format for the interactive test: 1 = 32bpp pixelfmt_bgrx8888
 * (feeds SDL directly, no per-frame conversion); 0 = pixelfmt_p4 paletted
 * (exercises screen_copy_rect's nibble-packed blit path instead). */
#define WUSS_TEST_32BPP 1

typedef enum test_window
{
  WIN_BALL,
  WIN_TEXT,
  WIN_BLANK,
  WIN_PALETTE,
  WIN_IMAGE,
  WIN__COUNT
}
test_window_t;

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
 * from the fixed-size wuss screen; map back down to screen space */
static void sdl_pos_to_scr(SDL_Window *window, int scr_width, int scr_height, float in_x, float in_y, int *out_x, int *out_y)
{
  int win_w, win_h;

  SDL_GetWindowSize(window, &win_w, &win_h);

  *out_x = (int) (in_x * scr_width  / win_w);
  *out_y = (int) (in_y * scr_height / win_h);
}

/* click windows to bring to front, drag titlebars to move, resize the
 * SDL window to see the wuss screen scale; F2 doubles the SDL window size;
 * Q or close to quit */
static result_t wuss_interactive_test(const char *resources)
{
  const int        scr_width  = 640;
  const int        scr_height = 480;
#if WUSS_TEST_32BPP
  const int        rowbytes   = scr_width * 4; /* pixelfmt_bgrx8888: 4 bytes/pixel */
#else
  const int        rowbytes   = scr_width / 2; /* pixelfmt_p4: 2 pixels/byte */
#endif

  result_t         rc;
  const char      *leafname;
  const char      *filename;
  bmfont_t        *font;
  bmfont_t        *daydream_font;
  void            *pixels;
  bitmap_t         bm;
#if !WUSS_TEST_32BPP
  bitmap_t        *disp;
#endif
  screen_t         scr;
  colour_t         palette[16];
  wuss_t          *wuss;
  ball_client_t    ic_a;
  text_client_t    ic_b;
  blank_client_t   ic_c;
  palette_client_t ic_d;
  image_client_t   ic_e;
  wuss_client_t    clients[WIN__COUNT];
  box_t            boxes[WIN__COUNT];
  wuss_window_t   *windows[WIN__COUNT];
  SDL_Window      *window;
  SDL_Renderer    *renderer;
  SDL_Texture     *texture;
  bool             quit;
  bool             garbage_pending;

  define_pico8_palette(palette);

  leafname = path_join_leafname("ms-sans-serif", "png");
  filename = path_join_filename(resources, 3, "resources", "bmfonts", leafname);
  rc = bmfont_create(filename, &font);
  if (rc != result_OK)
    goto Failure;

  leafname = path_join_leafname("daydream-font", "png");
  filename = path_join_filename(resources, 3, "resources", "bmfonts", leafname);
  rc = bmfont_create(filename, &daydream_font);
  if (rc != result_OK)
    goto Failure;

  leafname = path_join_leafname("jessica", "png");
  filename = path_join_filename(resources, 3, "resources", "images", leafname);
  rc = bitmap_load_png(&ic_e.bitmap, filename);
  if (rc != result_OK)
    goto Failure;

  pixels = malloc(rowbytes * scr_height);
  if (pixels == NULL)
    goto Failure;

#if WUSS_TEST_32BPP
  rc = bitmap_init(&bm, scr_width, scr_height, pixelfmt_bgrx8888, rowbytes, palette, pixels);
#else
  rc = bitmap_init(&bm, scr_width, scr_height, pixelfmt_p4, rowbytes, palette, pixels);
#endif
  if (rc != result_OK)
    goto Failure;

  bitmap_clear(&bm, palette[palette_PICO8_WHITE]);

  screen_for_bitmap(&scr, &bm);

  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    fprintf(stderr, "Error: SDL_Init: %s\n", SDL_GetError());
    goto Failure;
  }

  window = SDL_CreateWindow("DPTLib wuss Test", scr_width, scr_height, 0);
  if (window == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateWindow: %s\n", SDL_GetError());
    goto Failure;
  }

  renderer = SDL_CreateRenderer(window, NULL);
  if (renderer == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateRenderer: %s\n", SDL_GetError());
    goto Failure;
  }

  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               scr_width, scr_height);
  if (texture == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateTexture: %s\n", SDL_GetError());
    goto Failure;
  }

  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST); /* keep pixels crisp when F2 scales the window up */

  {
    wuss_config_t config;

    config.titlebar_height = 0;
    config.titlebar_bg     = palette_PICO8_DARK_BLUE;
    config.titlebar_fg     = palette_PICO8_WHITE;

    rc = wuss_create(&scr, font, palette, NELEMS(palette), &config, &wuss);
    if (rc != result_OK)
      goto Failure;
  }

  ic_a.bg     = palette[palette_PICO8_RED];
  ic_a.ball   = palette[palette_PICO8_WHITE];
  ic_a.x      = 50;
  ic_a.y      = 50;
  ic_a.dx     = 3;
  ic_a.dy     = 2;
  ic_a.radius = 8;
  clients[WIN_BALL].redraw      = ball_redraw;
  clients[WIN_BALL].mouse       = NULL;
  clients[WIN_BALL].client_data = &ic_a;
  clients[WIN_BALL].bg          = wuss_NO_BACKGROUND; /* ball_redraw paints its own background every frame */
  boxes[WIN_BALL].x0 = 20;  boxes[WIN_BALL].y0 = 20;
  boxes[WIN_BALL].x1 = 220; boxes[WIN_BALL].y1 = 180;
  rc = wuss_window_create(wuss, &boxes[WIN_BALL], "Bouncing Ball", wuss_WINDOW_NONE, &clients[WIN_BALL], &windows[WIN_BALL]);
  if (rc != result_OK)
    goto Failure;

  ic_b.font        = daydream_font;
  ic_b.bg          = palette[palette_PICO8_BLUE]; /* matches client_b.bg, for bmfont_draw's glyph blending */
  ic_b.fg          = palette[palette_PICO8_WHITE];
  ic_b.frame_count = 0;
  clients[WIN_TEXT].redraw      = text_redraw;
  clients[WIN_TEXT].mouse       = NULL;
  clients[WIN_TEXT].client_data = &ic_b;
  clients[WIN_TEXT].bg          = palette_PICO8_BLUE;
  boxes[WIN_TEXT].x0 = 120; boxes[WIN_TEXT].y0 = 100;
  boxes[WIN_TEXT].x1 = 340; boxes[WIN_TEXT].y1 = 280;
  ic_b.base_width = boxes[WIN_TEXT].x1 - boxes[WIN_TEXT].x0;
  rc = wuss_window_create(wuss, &boxes[WIN_TEXT], "Lorem Ipsum", wuss_WINDOW_NONE, &clients[WIN_TEXT], &windows[WIN_TEXT]);
  if (rc != result_OK)
    goto Failure;

  ic_c.npalette   = NELEMS(palette);
  ic_c.index      = palette_PICO8_GREEN;
  ic_c.frame_count = 0;
  clients[WIN_BLANK].redraw      = NULL;
  clients[WIN_BLANK].mouse       = NULL;
  clients[WIN_BLANK].client_data = &ic_c;
  clients[WIN_BLANK].bg          = palette_PICO8_GREEN; /* wuss fills the content area itself */
  boxes[WIN_BLANK].x0 = 260; boxes[WIN_BLANK].y0 = 60;
  boxes[WIN_BLANK].x1 = 460; boxes[WIN_BLANK].y1 = 220;
  rc = wuss_window_create(wuss, &boxes[WIN_BLANK], NULL, wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE, &clients[WIN_BLANK], &windows[WIN_BLANK]);
  if (rc != result_OK)
    goto Failure;

  ic_d.palette  = palette;
  ic_d.npalette = NELEMS(palette);
  clients[WIN_PALETTE].redraw      = palette_redraw;
  clients[WIN_PALETTE].mouse       = NULL;
  clients[WIN_PALETTE].client_data = &ic_d;
  clients[WIN_PALETTE].bg          = palette_PICO8_BLACK; /* backdrop for any rounding gap around the grid */
  boxes[WIN_PALETTE].x0 = 380; boxes[WIN_PALETTE].y0 = 260;
  boxes[WIN_PALETTE].x1 = boxes[WIN_PALETTE].x0 + 100; boxes[WIN_PALETTE].y1 = boxes[WIN_PALETTE].y0 + 100;
  rc = wuss_window_create(wuss, &boxes[WIN_PALETTE], "Palette", wuss_WINDOW_NONE, &clients[WIN_PALETTE], &windows[WIN_PALETTE]);
  if (rc != result_OK)
    goto Failure;

  clients[WIN_IMAGE].redraw      = image_redraw;
  clients[WIN_IMAGE].mouse       = NULL;
  clients[WIN_IMAGE].client_data = &ic_e;
  clients[WIN_IMAGE].bg          = palette_PICO8_BLACK; /* shows through the image's transparent pixels */
  boxes[WIN_IMAGE].x0 = 370; boxes[WIN_IMAGE].y0 = 10;
  boxes[WIN_IMAGE].x1 = boxes[WIN_IMAGE].x0 + ic_e.bitmap.width; boxes[WIN_IMAGE].y1 = boxes[WIN_IMAGE].y0 + ic_e.bitmap.height;
  rc = wuss_window_create(wuss, &boxes[WIN_IMAGE], "Image", wuss_WINDOW_NONE, &clients[WIN_IMAGE], &windows[WIN_IMAGE]);
  if (rc != result_OK)
    goto Failure;

  quit            = false;
  garbage_pending = false;

  wuss_redraw(wuss);

  while (!quit)
  {
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
      case SDL_EVENT_QUIT:
        quit = true;
        break;

      case SDL_EVENT_KEY_UP:
        if (event.key.key == SDLK_Q)
          quit = true;
        else if (event.key.key == SDLK_F1)
          garbage_pending = true;
        else if (event.key.key == SDLK_F2)
        {
          int w, h;

          SDL_GetWindowSize(window, &w, &h);
          SDL_SetWindowSize(window, w * 2, h * 2);
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.button.x, event.button.y, &x, &y);
          wuss_mouse_down(wuss, x, y, sdl_button_to_wuss(event.button.button), NULL);
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.button.x, event.button.y, &x, &y);
          wuss_mouse_up(wuss, x, y, sdl_button_to_wuss(event.button.button), NULL);
        }
        break;

      case SDL_EVENT_MOUSE_MOTION:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.motion.x, event.motion.y, &x, &y);
          wuss_mouse_move(wuss, x, y, NULL);
        }
        break;

      default:
        break;
      }
    }

    ball_step(windows[WIN_BALL], &ic_a);
    text_step(windows[WIN_TEXT], &ic_b);
    blank_step(windows[WIN_BLANK], &ic_c);

    if (garbage_pending)
    {
      /* full-screen corruption, drawn over whatever's already on screen
       * (windows included) and left untouched: nothing here is invalidated,
       * so it stays put until something actually redraws over it, e.g. the
       * ball's own small per-frame dirty rect eating a trail through it, or
       * a window being dragged across it */
      unsigned char *p;
      size_t         n;
      size_t         i;

      p = pixels;
      n = (size_t) rowbytes * scr_height;
      for (i = 0; i < n; i++)
        p[i] = (unsigned char) rand();

      garbage_pending = false;
    }
    else
    {
      int ndirty, i;

      ndirty = wuss_get_dirty_count(wuss);
      for (i = 0; i < ndirty; i++)
      {
        box_t dirty;

        wuss_get_dirty(wuss, i, &dirty);
        scr.clip = dirty;
        screen_draw_rect(&scr, dirty.x0, dirty.y0,
                          dirty.x1 - dirty.x0, dirty.y1 - dirty.y0,
                          palette[palette_PICO8_WHITE]);
      }

      /* narrowed above per dirty rect for the flash; screen_copy_rect (used
       * for window-drag blitting) reads this clip too, so it must not leak
       * into the next frame narrower than the whole screen */
      box_reset(&scr.clip);

      wuss_redraw_dirty(wuss);
    }

#if WUSS_TEST_32BPP
    SDL_UpdateTexture(texture, NULL, bm.base, bm.rowbytes); /* bm is already bgrx8888: no conversion needed */
#else
    rc = bitmap_convert(&bm, pixelfmt_bgrx8888, &disp);
    if (rc == result_OK)
    {
      SDL_UpdateTexture(texture, NULL, disp->base, disp->rowbytes);
      free(disp->base);
      free(disp);
    }
#endif

    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_Delay(1000 / 60);
  }

  wuss_destroy(wuss);
  bmfont_destroy(font);
  bmfont_destroy(daydream_font);

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  free(pixels);
  free(ic_e.bitmap.base);

  return result_TEST_PASSED;


Failure:

  printf("wuss_interactive_test: failed (rc=0x%X)\n", rc);

  return result_TEST_FAILED;
}

#endif /* USE_SDL */

/* ----------------------------------------------------------------------- */

typedef struct test_client
{
  int                 redraw_count;
  int                 mouse_count;
  wuss_mouse_action_t last_action;
  int                 last_x, last_y;
  wuss_button_t       last_button;
}
test_client_t;

static result_t test_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  test_client_t *tc;

  NOT_USED(window);
  NOT_USED(scr);
  NOT_USED(content);

  tc = client_data;
  tc->redraw_count++;

  return result_OK;
}

static result_t test_mouse(wuss_window_t *window, wuss_mouse_action_t action, int x, int y, wuss_button_t button, void *client_data)
{
  test_client_t *tc;

  NOT_USED(window);

  tc = client_data;
  tc->mouse_count++;
  tc->last_action = action;
  tc->last_x      = x;
  tc->last_y      = y;
  tc->last_button = button;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t wuss_test(const char *resources)
{
  result_t       rc;
  int            rowbytes;
  void          *pixels;
  bitmap_t       bm;
  screen_t       scr;
  wuss_t        *wuss;
  wuss_config_t  bad_config;
  wuss_t        *bad_wuss;
  test_client_t  tc_a, tc_b, tc_c, tc_d;
  wuss_client_t  client_a, client_b, client_c, client_d;
  box_t          box_a, box_b, box_c, box_d;
  wuss_window_t *win_a, *win_b, *win_c, *win_d;
  wuss_window_t *hit;
  box_t          visible, content;
  int            before_a, before_b;
  int            width, height;
  const colour_t custom_palette[2] = { 0, 0 };

  NOT_USED(resources);

  rowbytes = 200 * 4;
  pixels = malloc(rowbytes * 200);
  if (pixels == NULL)
    goto Failure;

  rc = bitmap_init(&bm, 200, 200, pixelfmt_bgrx8888, rowbytes, NULL, pixels);
  if (rc != result_OK)
    goto Failure;

  screen_for_bitmap(&scr, &bm);

  printf("test: wuss_create with bad titlebar colour index\n");

  bad_config.titlebar_height = 0;
  bad_config.titlebar_bg     = 999;
  bad_config.titlebar_fg     = 0;
  rc = wuss_create(&scr, NULL, NULL, 0, &bad_config, &bad_wuss);
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  printf("test: wuss_create with custom palette, no config\n");

  {
    wuss_t *custom_wuss;

    rc = wuss_create(&scr, NULL, custom_palette, 2, NULL, &custom_wuss);
    if (rc != result_OK)
      goto Failure;
    wuss_destroy(custom_wuss);
  }

  printf("test: wuss_create with default palette\n");

  rc = wuss_create(&scr, NULL, NULL, 0, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create too small\n");

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 0; /* zero-height content is invalid regardless of furniture */
  rc = wuss_window_create(wuss, &box_a, "toosmall", wuss_WINDOW_NONE, NULL, &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  tc_a.redraw_count = 0;
  tc_a.mouse_count  = 0;
  client_a.redraw      = test_redraw;
  client_a.mouse       = test_mouse;
  client_a.client_data = &tc_a;
  client_a.bg          = wuss_NO_BACKGROUND;

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 100;
  rc = wuss_window_create(wuss, &box_a, "A", wuss_WINDOW_NONE, &client_a, &win_a);
  if (rc != result_OK)
    goto Failure;

  tc_b.redraw_count = 0;
  tc_b.mouse_count  = 0;
  client_b.redraw      = test_redraw;
  client_b.mouse       = test_mouse;
  client_b.client_data = &tc_b;
  client_b.bg          = wuss_NO_BACKGROUND;

  box_b.x0 = 50;
  box_b.y0 = 50;
  box_b.x1 = 150;
  box_b.y1 = 150;
  rc = wuss_window_create(wuss, &box_b, "B", wuss_WINDOW_NONE, &client_b, &win_b);
  if (rc != result_OK)
    goto Failure;

  printf("test: redraw\n");

  rc = wuss_redraw(wuss);
  if (rc != result_OK)
    goto Failure;
  if (tc_a.redraw_count != 1 || tc_b.redraw_count != 1)
    goto Failure;

  printf("test: invalidating an area of A fully covered by topmost B is discarded\n");

  {
    box_t local;

    local.x0 = 60;
    local.y0 = 60;
    local.x1 = 90;
    local.y1 = 90; /* well within B's (50,50)-(150,150)+furniture visible footprint */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    local.x0 = 0;
    local.y0 = 0;
    local.x1 = 20;
    local.y1 = 20; /* outside B's footprint entirely: nothing to clip away */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
  }

  printf("test: a piece straddling B's edge redraws A but not B\n");

  {
    box_t local;

    before_a = tc_a.redraw_count;
    before_b = tc_b.redraw_count;

    local.x0 = 30;
    local.y0 = 60;
    local.x1 = 70;
    local.y1 = 90; /* straddles B's left edge (x=49): only x:30-49 survives clipping */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_a.redraw_count != before_a + 1)
      goto Failure;
    if (tc_b.redraw_count != before_b)
      goto Failure; /* B not touched by the surviving piece: must not be redrawn */
  }

  printf("test: z-order hit test and local coordinate translation (B on top)\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 75, 75, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;
  if (tc_b.last_action != wuss_MOUSE_DOWN || tc_b.last_x != 25 || tc_b.last_y != 25)
    goto Failure;

  rc = wuss_mouse_up(wuss, 75, 75, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: click-to-front changes subsequent overlap hits\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 10, -10, wuss_BUTTON_SELECT, &hit); /* A's titlebar, above its content */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_up(wuss, 10, -10, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click does not change z-order\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 120, 120, wuss_BUTTON_SELECT, &hit); /* B's content, only within B */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1)
    goto Failure;

  rc = wuss_mouse_up(wuss, 120, 120, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;

  tc_a.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 75, 75, wuss_BUTTON_SELECT, &hit); /* A still topmost: B's content click above didn't bring it to front */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1 || tc_a.last_x != 75 || tc_a.last_y != 75)
    goto Failure;

  rc = wuss_mouse_up(wuss, 75, 75, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: titlebar click starts a drag, not delivered as content\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 10, -10, wuss_BUTTON_SELECT, &hit); /* A's titlebar, A already topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 0)
    goto Failure;

  printf("test: drag-move updates visible bounds and invalidates the affected region\n");

  rc = wuss_redraw_dirty(wuss); /* flush the click-to-front's leftover dirty region first */
  if (rc != result_OK)
    goto Failure;

  before_a = tc_a.redraw_count;
  before_b = tc_b.redraw_count;
  rc = wuss_mouse_move(wuss, 30, 15, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.redraw_count != before_a || tc_b.redraw_count != before_b)
    goto Failure; /* invalidated, not yet redrawn */

  if (wuss_get_dirty_count(wuss) == 0)
    goto Failure;

  rc = wuss_redraw_dirty(wuss);
  if (rc != result_OK)
    goto Failure;
  if (tc_a.redraw_count != before_a || tc_b.redraw_count != before_b)
    goto Failure; /* blitted, not redrawn: A's own pixels moved without a client
                    * callback, and the vacated sliver behind its old position
                    * exposes only background, not B */

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 19 || visible.y0 != 4)
    goto Failure;

  printf("test: mouse-up ends the drag\n");

  rc = wuss_mouse_up(wuss, 30, 15, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  printf("test: Adjust-drag moves a window without bringing it to front\n");

  rc = wuss_mouse_down(wuss, 140, 35, wuss_BUTTON_ADJUST, &hit); /* B's titlebar, clear of A */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_move(wuss, 145, 60, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  wuss_window_get_visible_bounds(win_b, &visible);
  if (visible.x0 != 54 || visible.y0 != 54)
    goto Failure;

  rc = wuss_mouse_up(wuss, 145, 60, wuss_BUTTON_ADJUST, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_down(wuss, 75, 75, wuss_BUTTON_SELECT, &hit); /* within both A and B; A still topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_up(wuss, 75, 75, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_move(wuss, 200, 200, &hit); /* off all windows, drag must have ended */
  if (rc != result_OK)
    goto Failure;
  if (hit != NULL)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 19 || visible.y0 != 4)
    goto Failure;

  printf("test: window_resize valid and too-small cases\n");

  rc = wuss_window_resize(win_a, 50, 0); /* zero-height content is invalid */
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  rc = wuss_window_resize(win_a, 50, 50);
  if (rc != result_OK)
    goto Failure;

  /* content ends up exactly the requested size... */
  wuss_window_get_content_bounds(win_a, &content);
  width  = content.x1 - content.x0;
  height = content.y1 - content.y0;
  if (width != 50 || height != 50)
    goto Failure;

  /* ...with the titlebar/outline furniture added on top of that */
  wuss_window_get_visible_bounds(win_a, &visible);
  width  = visible.x1 - visible.x0;
  height = visible.y1 - visible.y0;
  if (width != 52 || height != 72)
    goto Failure;

  printf("test: title-less, no-outline window has no furniture, so visible == content\n");

  tc_d.redraw_count = 0;
  tc_d.mouse_count  = 0;
  client_d.redraw      = test_redraw;
  client_d.mouse       = test_mouse;
  client_d.client_data = &tc_d;
  client_d.bg          = wuss_NO_BACKGROUND;

  box_d.x0 = 0;  box_d.y0 = 160;
  box_d.x1 = 30; box_d.y1 = 175; /* shorter than the 20px titlebar_height, still valid: no titlebar to fit */
  rc = wuss_window_create(wuss, &box_d, "ignored", wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE, &client_d, &win_d);
  if (rc != result_OK)
    goto Failure;

  wuss_window_get_visible_bounds(win_d, &visible);
  if (visible.x0 != box_d.x0 || visible.y0 != box_d.y0 ||
      visible.x1 != box_d.x1 || visible.y1 != box_d.y1)
    goto Failure;

  printf("test: click within a title-less window's top edge is delivered as content, not a drag\n");

  rc = wuss_mouse_down(wuss, 5, 165, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_d)
    goto Failure;
  if (tc_d.mouse_count != 1 || tc_d.last_action != wuss_MOUSE_DOWN || tc_d.last_x != 5 || tc_d.last_y != 5)
    goto Failure;

  rc = wuss_mouse_up(wuss, 5, 165, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click on a title-less window brings it to front\n");

  {
    test_client_t  tc_e, tc_f;
    wuss_client_t  client_e, client_f;
    box_t          box_e, box_f;
    wuss_window_t *win_e, *win_f;

    tc_e.redraw_count = 0;
    tc_e.mouse_count  = 0;
    client_e.redraw      = test_redraw;
    client_e.mouse       = test_mouse;
    client_e.client_data = &tc_e;
    client_e.bg          = wuss_NO_BACKGROUND;

    box_e.x0 = 100; box_e.y0 = 0;
    box_e.x1 = 150; box_e.y1 = 50;
    rc = wuss_window_create(wuss, &box_e, NULL, wuss_WINDOW_NO_TITLEBAR, &client_e, &win_e);
    if (rc != result_OK)
      goto Failure;

    tc_f.redraw_count = 0;
    tc_f.mouse_count  = 0;
    client_f.redraw      = test_redraw;
    client_f.mouse       = test_mouse;
    client_f.client_data = &tc_f;
    client_f.bg          = wuss_NO_BACKGROUND;

    box_f.x0 = 130; box_f.y0 = 20;
    box_f.x1 = 180; box_f.y1 = 70;
    rc = wuss_window_create(wuss, &box_f, NULL, wuss_WINDOW_NO_TITLEBAR, &client_f, &win_f);
    if (rc != result_OK)
      goto Failure;

    /* F was created after E, so F is topmost; click E's exposed content
     * (outside the overlap) to bring E back to front */
    rc = wuss_mouse_down(wuss, 110, 10, wuss_BUTTON_SELECT, &hit); /* within E only */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_e)
      goto Failure;

    rc = wuss_mouse_up(wuss, 110, 10, wuss_BUTTON_SELECT, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_down(wuss, 135, 25, wuss_BUTTON_SELECT, &hit); /* overlap: E now on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_e)
      goto Failure;

    rc = wuss_mouse_up(wuss, 135, 25, wuss_BUTTON_SELECT, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_destroy(win_e);
    wuss_window_destroy(win_f);
  }

  printf("test: wuss_window_set_background\n");

  rc = wuss_window_set_background(win_d, 999);
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  rc = wuss_window_set_background(win_d, palette_PICO8_ORANGE);
  if (rc != result_OK)
    goto Failure;

  wuss_window_destroy(win_d);

  printf("test: moving/resizing a window entirely behind an occluder has no visible effect\n");

  {
    test_client_t  tc_h, tc_g;
    wuss_client_t  client_h, client_g;
    box_t          box_h, box_g;
    wuss_window_t *win_h, *win_g;
    int             before_h, before_g;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    client_h.redraw      = test_redraw;
    client_h.mouse       = test_mouse;
    client_h.client_data = &tc_h;
    client_h.bg          = wuss_NO_BACKGROUND;

    box_h.x0 = 10; box_h.y0 = 10;
    box_h.x1 = 30; box_h.y1 = 30;
    rc = wuss_window_create(wuss, &box_h, NULL, wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE, &client_h, &win_h);
    if (rc != result_OK)
      goto Failure;

    tc_g.redraw_count = 0;
    tc_g.mouse_count  = 0;
    client_g.redraw      = test_redraw;
    client_g.mouse       = test_mouse;
    client_g.client_data = &tc_g;
    client_g.bg          = wuss_NO_BACKGROUND;

    box_g.x0 = 0;   box_g.y0 = 0;
    box_g.x1 = 150; box_g.y1 = 150; /* G is created after H, so G is topmost and fully covers H */
    rc = wuss_window_create(wuss, &box_g, NULL, wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE, &client_g, &win_g);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidations before measuring */
    if (rc != result_OK)
      goto Failure;

    before_h = tc_h.redraw_count;
    before_g = tc_g.redraw_count;

    wuss_window_move(win_h, 60, 60); /* still entirely within G's footprint */
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_h.redraw_count != before_h || tc_g.redraw_count != before_g)
      goto Failure; /* nothing visible changed: no redraw of either window */

    rc = wuss_window_resize(win_h, 25, 25); /* still entirely within G's footprint */
    if (rc != result_OK)
      goto Failure;
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_h.redraw_count != before_h || tc_g.redraw_count != before_g)
      goto Failure;

    wuss_window_destroy(win_h);
    wuss_window_destroy(win_g);
  }

  printf("test: destroy mid-drag then move doesn't crash\n");

  tc_c.redraw_count = 0;
  tc_c.mouse_count  = 0;
  client_c.redraw      = test_redraw;
  client_c.mouse       = test_mouse;
  client_c.client_data = &tc_c;
  client_c.bg          = wuss_NO_BACKGROUND;

  box_c.x0 = 0;
  box_c.y0 = 0;
  box_c.x1 = 40;
  box_c.y1 = 40;
  rc = wuss_window_create(wuss, &box_c, "C", wuss_WINDOW_NONE, &client_c, &win_c);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_down(wuss, 5, -10, wuss_BUTTON_SELECT, &hit); /* C's titlebar, above its content */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_c)
    goto Failure;

  wuss_window_destroy(win_c);

  rc = wuss_mouse_move(wuss, 20, 20, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: destroy\n");

  wuss_window_destroy(win_a);
  wuss_window_destroy(win_b);
  wuss_destroy(wuss);

  free(pixels);

#ifdef USE_SDL
  rc = wuss_interactive_test(resources);
  if (rc != result_TEST_PASSED)
    goto Failure;
#endif

  return result_TEST_PASSED;


Failure:

  printf("wuss_test: failed (rc=0x%X)\n", rc);

  return result_TEST_FAILED;
}
