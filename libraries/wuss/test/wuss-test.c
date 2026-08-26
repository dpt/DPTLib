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

#include "tasks/ball.h"
#include "tasks/blank.h"
#include "tasks/checker.h"
#include "tasks/curve.h"
#include "tasks/image.h"
#include "tasks/launcher.h"
#include "tasks/palette.h"
#include "tasks/sofa.h"
#include "tasks/text.h"

/* Screen pixel format for the interactive test: 1 = 32bpp pixelfmt_bgrx8888
 * (feeds SDL directly, no per-frame conversion); 0 = pixelfmt_p4 paletted
 * (exercises screen_copy_rect's nibble-packed blit path instead). */
#define WUSS_TEST_32BPP 0

/* the launcher's spawn callbacks take no arguments, so the pieces they need
 * are stashed here instead; wuss_interactive_test runs at most once per
 * process, so file-scope statics are as good as a context struct */
static wuss_t         *g_wuss;
static const colour_t *g_palette;
static int              g_npalette;
static const char      *g_resources;
static bmfont_t        *g_daydream_font;

static ball_task_t     g_ball_task;
static text_task_t     g_text_task;
static blank_task_t    g_blank_task;
static palette_task_t  g_palette_task;
static image_task_t    g_image_task;
static checker_task_t  g_checker_task;
static curve_task_t    g_curve_task;
static sofa_task_t     g_sofa_task;

static result_t spawn_ball(void)    { return ball_create(g_wuss, g_palette, &g_ball_task); }
static result_t spawn_text(void)    { return text_create(g_wuss, g_palette, g_daydream_font, &g_text_task); }
static result_t spawn_blank(void)   { return blank_create(g_wuss, g_npalette, &g_blank_task); }
static result_t spawn_palette(void) { return palette_create(g_wuss, g_palette, g_npalette, &g_palette_task); }
static result_t spawn_image(void)   { return image_create(g_wuss, g_palette, g_resources, &g_image_task); }
static result_t spawn_checker(void) { return checker_create(g_wuss, g_palette, &g_checker_task); }
static result_t spawn_curve(void)   { return curve_create(g_wuss, g_palette, &g_curve_task); }
static result_t spawn_sofa(void)    { return sofa_create(g_wuss, g_palette, &g_sofa_task); }

static void destroy_ball(void)    { ball_destroy(&g_ball_task); }
static void destroy_text(void)    { text_destroy(&g_text_task); }
static void destroy_blank(void)   { blank_destroy(&g_blank_task); }
static void destroy_palette(void) { palette_destroy(&g_palette_task); }
static void destroy_image(void)   { image_destroy(&g_image_task); }
static void destroy_checker(void) { checker_destroy(&g_checker_task); }
static void destroy_curve(void)   { curve_destroy(&g_curve_task); }
static void destroy_sofa(void)    { sofa_destroy(&g_sofa_task); }

static launcher_entry_t g_launcher_entries[] =
{
  { "Ball",    spawn_ball,    destroy_ball,    false },
  { "Text",    spawn_text,    destroy_text,    false },
  { "Blank",   spawn_blank,   destroy_blank,   false },
  { "Palette", spawn_palette, destroy_palette, false },
  { "Image",   spawn_image,   destroy_image,   false },
  { "Checker", spawn_checker, destroy_checker, false },
  { "Curve",   spawn_curve,   destroy_curve,   false },
  { "Sofa",    spawn_sofa,    destroy_sofa,    false }
};

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
 * from the fixed-size Wuss screen; map back down to screen space */
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

/* click windows to bring to front, drag titlebars to move, resize the
 * SDL window to see the Wuss screen scale; F2 doubles the SDL window size,
 * Shift-F2 halves it; F3 redraws the whole screen one pixel at a time, to
 * catch tasks whose drawing routines misbehave under a 1x1 clip; Q or
 * close to quit */
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
  launcher_task_t  launcher_task;
  SDL_Window      *window;
  SDL_Renderer    *renderer;
  SDL_Texture     *texture;
  bool             quit;
  bool             garbage_pending;
  bool             pixel_stress_pending;
  int              i;

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

  window = SDL_CreateWindow("DPTLib Wuss Test", scr_width, scr_height, 0);
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

  g_wuss          = wuss;
  g_palette       = palette;
  g_npalette      = NELEMS(palette);
  g_resources     = resources;
  g_daydream_font = daydream_font;

  rc = launcher_create(wuss, g_launcher_entries, NELEMS(g_launcher_entries), font, palette, &launcher_task);
  if (rc != result_OK)
    goto Failure;

  quit                 = false;
  garbage_pending      = false;
  pixel_stress_pending = false;

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
        else if (event.key.key == SDLK_F3)
          pixel_stress_pending = true;
        else if (event.key.key == SDLK_F2)
        {
          int w, h;

          SDL_GetWindowSize(window, &w, &h);
          if (event.key.mod & SDL_KMOD_SHIFT)
            SDL_SetWindowSize(window, w / 2, h / 2);
          else
            SDL_SetWindowSize(window, w * 2, h * 2);
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.button.x, event.button.y, &x, &y);
          wuss_mouse_click(wuss, x, y, sdl_button_to_wuss(event.button.button), wuss_MOUSE_DOWN, NULL);
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.button.x, event.button.y, &x, &y);
          wuss_mouse_click(wuss, x, y, sdl_button_to_wuss(event.button.button), wuss_MOUSE_UP, NULL);
        }
        break;

      case SDL_EVENT_MOUSE_MOTION:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.motion.x, event.motion.y, &x, &y);
          wuss_mouse_move(wuss, x, y, NULL);
        }
        break;

      case SDL_EVENT_MOUSE_WHEEL:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.wheel.mouse_x, event.wheel.mouse_y, &x, &y);
          wuss_scroll(wuss, x, y, (int) event.wheel.y, NULL);
        }
        break;

      default:
        break;
      }
    }

    wuss_idle(wuss);

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
    else if (pixel_stress_pending)
    {
      /* redraw the whole screen one pixel at a time: each wuss_redraw_dirty
       * call is flushed before the next pixel is invalidated, so no two
       * pixels are ever coalesced into one redraw. A task whose drawing
       * routine assumes it always gets handed a multi-pixel/aligned clip
       * (rather than trusting scr->clip) will visibly misdraw here even
       * though it looks fine under normal, larger dirty regions. */
      int x, y;

      for (y = 0; y < scr_height; y++)
      {
        for (x = 0; x < scr_width; x++)
        {
          box_t px;

          px.x0 = x;     px.y0 = y;
          px.x1 = x + 1; px.y1 = y + 1;

          wuss_invalidate(wuss, &px);
          wuss_redraw_dirty(wuss);
        }
      }

      pixel_stress_pending = false;
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

  for (i = 0; i < NELEMS(g_launcher_entries); i++)
    if (g_launcher_entries[i].running)
      g_launcher_entries[i].destroy();
  launcher_destroy(&launcher_task);

  wuss_destroy(wuss);
  bmfont_destroy(font);
  bmfont_destroy(daydream_font);

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  free(pixels);

  return result_TEST_PASSED;


Failure:

  printf("wuss_interactive_test: failed (rc=0x%X)\n", rc);

  return result_TEST_FAILED;
}

#endif /* USE_SDL */

/* ----------------------------------------------------------------------- */

typedef struct test_task
{
  int                 redraw_count;
  int                 mouse_count;
  wuss_mouse_action_t last_action;
  int                 last_x, last_y;
  wuss_button_t       last_button;
  int                 close_count;
  int                 stop_count;
}
test_task_t;

static result_t test_handle(wuss_window_t     *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  test_task_t *tc;

  NOT_USED(window);

  tc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    tc->redraw_count++;
    break;

  case wuss_EVENT_MOUSE:
    tc->mouse_count++;
    tc->last_action = event->data.mouse.action;
    tc->last_x      = event->data.mouse.x;
    tc->last_y      = event->data.mouse.y;
    tc->last_button = event->data.mouse.button;
    break;

  case wuss_EVENT_CLOSE:
    tc->close_count++;
    break;

  case wuss_EVENT_QUIT:
    tc->stop_count++;
    break;

  default:
    break;
  }

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
  test_task_t  tc_a, tc_b, tc_c, tc_d;
  wuss_task_t  delegate_a, delegate_b, delegate_c, delegate_d;
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
  rc = wuss_window_create(wuss,
                          &box_a,
                          "toosmall",
                          wuss_WINDOW_NONE,
                          NULL,
                          box_a.x1 - box_a.x0,
                          box_a.y1 - box_a.y0,
                          &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  tc_a.redraw_count = 0;
  tc_a.mouse_count  = 0;
  delegate_a.handle      = test_handle;
  delegate_a.task_data = &tc_a;
  delegate_a.bg          = wuss_NO_BACKGROUND;

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 100;
  rc = wuss_window_create(wuss,
                          &box_a,
                          "A",
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          &delegate_a,
                          box_a.x1 - box_a.x0,
                          box_a.y1 - box_a.y0,
                          &win_a);
  if (rc != result_OK)
    goto Failure;

  tc_b.redraw_count = 0;
  tc_b.mouse_count  = 0;
  delegate_b.handle      = test_handle;
  delegate_b.task_data = &tc_b;
  delegate_b.bg          = wuss_NO_BACKGROUND;

  box_b.x0 = 50;
  box_b.y0 = 50;
  box_b.x1 = 150;
  box_b.y1 = 150;
  rc = wuss_window_create(wuss,
                          &box_b,
                          "B",
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          &delegate_b,
                          box_b.x1 - box_b.x0,
                          box_b.y1 - box_b.y0,
                          &win_b);
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
  rc = wuss_mouse_click(wuss, 75, 75, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;
  if (tc_b.last_action != wuss_MOUSE_DOWN || tc_b.last_x != 25 || tc_b.last_y != 25)
    goto Failure;

  rc = wuss_mouse_click(wuss, 75, 75, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: clicking a window's close icon sends wuss_EVENT_CLOSE, not a drag\n");

  tc_a.close_count = 0;
  rc = wuss_mouse_click(wuss, 6, 11, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.close_count != 1)
    goto Failure;

  rc = wuss_mouse_click(wuss, 31, 36, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit); /* if the close click had started a drag, this would move A */
  if (rc != result_OK)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 0)
    goto Failure; /* unmoved: no drag was started by the close click */

  printf("test: click-to-front changes subsequent overlap hits\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, 31, 11, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's titlebar, above its content, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, 31, 11, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click does not change z-order\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_click(wuss, 120, 120, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* B's content, only within B */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1)
    goto Failure;

  rc = wuss_mouse_click(wuss, 120, 120, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, 75, 75, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A still topmost: B's content click above didn't bring it to front */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1 || tc_a.last_x != 74 || tc_a.last_y != 54)
    goto Failure;

  rc = wuss_mouse_click(wuss, 75, 75, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: titlebar click starts a drag, not delivered as content\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, 31, 11, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's titlebar, A already topmost, clear of the close icon */
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
  rc = wuss_mouse_move(wuss, 31, 36, &hit);
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
    goto Failure; /* blitted, not redrawn: A's own pixels moved without a task
                    * callback, and the vacated sliver behind its old position
                    * exposes only background, not B */

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 25)
    goto Failure;

  printf("test: mouse-up ends the drag\n");

  rc = wuss_mouse_click(wuss, 31, 36, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  printf("test: Adjust-drag moves a window without bringing it to front\n");

  rc = wuss_mouse_click(wuss, 140, 35, wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* B's titlebar, clear of A */
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

  rc = wuss_mouse_click(wuss, 145, 60, wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_click(wuss, 75, 75, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* within both A and B; A still topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, 75, 75, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_move(wuss, 200, 200, &hit); /* off all windows, drag must have ended */
  if (rc != result_OK)
    goto Failure;
  if (hit != NULL)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 25)
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
  delegate_d.handle      = test_handle;
  delegate_d.task_data = &tc_d;
  delegate_d.bg          = wuss_NO_BACKGROUND;

  box_d.x0 = 0;  box_d.y0 = 160;
  box_d.x1 = 30; box_d.y1 = 175; /* shorter than the 20px titlebar_height, still valid: no titlebar to fit */
  rc = wuss_window_create(wuss,
                          &box_d,
                          "ignored",
                          wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          &delegate_d,
                          box_d.x1 - box_d.x0,
                          box_d.y1 - box_d.y0,
                          &win_d);
  if (rc != result_OK)
    goto Failure;

  wuss_window_get_visible_bounds(win_d, &visible);
  if (visible.x0 != box_d.x0 || visible.y0 != box_d.y0 ||
      visible.x1 != box_d.x1 || visible.y1 != box_d.y1)
    goto Failure;

  printf("test: click within a title-less window's top edge is delivered as content, not a drag\n");

  rc = wuss_mouse_click(wuss, 5, 165, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_d)
    goto Failure;
  if (tc_d.mouse_count != 1 || tc_d.last_action != wuss_MOUSE_DOWN || tc_d.last_x != 5 || tc_d.last_y != 5)
    goto Failure;

  rc = wuss_mouse_click(wuss, 5, 165, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click on a title-less window does not change z-order\n");

  {
    test_task_t  tc_e, tc_f;
    wuss_task_t  delegate_e, delegate_f;
    box_t          box_e, box_f;
    wuss_window_t *win_e, *win_f;

    tc_e.redraw_count = 0;
    tc_e.mouse_count  = 0;
    delegate_e.handle      = test_handle;
    delegate_e.task_data = &tc_e;
    delegate_e.bg          = wuss_NO_BACKGROUND;

    box_e.x0 = 100; box_e.y0 = 0;
    box_e.x1 = 150; box_e.y1 = 50;
    rc = wuss_window_create(wuss,
                            &box_e,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_e,
                            box_e.x1 - box_e.x0,
                            box_e.y1 - box_e.y0,
                            &win_e);
    if (rc != result_OK)
      goto Failure;

    tc_f.redraw_count = 0;
    tc_f.mouse_count  = 0;
    delegate_f.handle      = test_handle;
    delegate_f.task_data = &tc_f;
    delegate_f.bg          = wuss_NO_BACKGROUND;

    box_f.x0 = 130; box_f.y0 = 20;
    box_f.x1 = 180; box_f.y1 = 70;
    rc = wuss_window_create(wuss,
                            &box_f,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_f,
                            box_f.x1 - box_f.x0,
                            box_f.y1 - box_f.y0,
                            &win_f);
    if (rc != result_OK)
      goto Failure;

    /* F was created after E, so F is topmost; clicking E's exposed content
     * (outside the overlap) is delivered to E but must not raise it */
    rc = wuss_mouse_click(wuss, 110, 10, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* within E only */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_e)
      goto Failure;

    rc = wuss_mouse_click(wuss, 110, 10, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, 135, 25, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: F still on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_f)
      goto Failure;

    rc = wuss_mouse_click(wuss, 135, 25, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
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
    test_task_t  tc_h, tc_g;
    wuss_task_t  delegate_h, delegate_g;
    box_t          box_h, box_g;
    wuss_window_t *win_h, *win_g;
    int             before_h, before_g;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h.handle      = test_handle;
    delegate_h.task_data = &tc_h;
    delegate_h.bg          = wuss_NO_BACKGROUND;

    box_h.x0 = 10; box_h.y0 = 10;
    box_h.x1 = 30; box_h.y1 = 30;
    rc = wuss_window_create(wuss,
                            &box_h,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_h,
                            box_h.x1 - box_h.x0,
                            box_h.y1 - box_h.y0,
                            &win_h);
    if (rc != result_OK)
      goto Failure;

    tc_g.redraw_count = 0;
    tc_g.mouse_count  = 0;
    delegate_g.handle      = test_handle;
    delegate_g.task_data = &tc_g;
    delegate_g.bg          = wuss_NO_BACKGROUND;

    box_g.x0 = 0;   box_g.y0 = 0;
    box_g.x1 = 150; box_g.y1 = 150; /* G is created after H, so G is topmost and fully covers H */
    rc = wuss_window_create(wuss,
                            &box_g,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_g,
                            box_g.x1 - box_g.x0,
                            box_g.y1 - box_g.y0,
                            &win_g);
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

  printf("test: bring-to-front only invalidates the newly-uncovered part\n");

  {
    test_task_t  tc_i, tc_j;
    wuss_task_t  delegate_i, delegate_j;
    box_t          box_i, box_j, dirty;
    wuss_window_t *win_i, *win_j;

    tc_i.redraw_count = 0;
    tc_i.mouse_count  = 0;
    delegate_i.handle      = test_handle;
    delegate_i.task_data = &tc_i;
    delegate_i.bg          = wuss_NO_BACKGROUND;

    box_i.x0 = 0; box_i.y0 = 0;
    box_i.x1 = 100; box_i.y1 = 100;
    rc = wuss_window_create(wuss,
                            &box_i,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_i,
                            box_i.x1 - box_i.x0,
                            box_i.y1 - box_i.y0,
                            &win_i);
    if (rc != result_OK)
      goto Failure;

    tc_j.redraw_count = 0;
    tc_j.mouse_count  = 0;
    delegate_j.handle      = test_handle;
    delegate_j.task_data = &tc_j;
    delegate_j.bg          = wuss_NO_BACKGROUND;

    box_j.x0 = 50; box_j.y0 = 0;
    box_j.x1 = 150; box_j.y1 = 100; /* J created after I, so J is topmost, covering I's right half */
    rc = wuss_window_create(wuss,
                            &box_j,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_j,
                            box_j.x1 - box_j.x0,
                            box_j.y1 - box_j.y0,
                            &win_j);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidations before measuring */
    if (rc != result_OK)
      goto Failure;

    wuss_window_restack(win_i, wuss_ZORDER_FRONT);

    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    wuss_get_dirty(wuss, 0, &dirty);
    if (dirty.x0 != 50 || dirty.y0 != 0 || dirty.x1 != 100 || dirty.y1 != 100)
      goto Failure; /* only I's previously-hidden right half, not its whole 0..100 footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_destroy(win_i);
    wuss_window_destroy(win_j);
  }

  printf("test: dragging off-screen and back on repaints the reappearing edge\n");

  {
    test_task_t  tc_m;
    wuss_task_t  delegate_m;
    box_t          box_m;
    wuss_window_t *win_m;
    int            before_m;

    tc_m.redraw_count = 0;
    tc_m.mouse_count  = 0;
    delegate_m.handle      = test_handle;
    delegate_m.task_data = &tc_m;
    delegate_m.bg          = wuss_NO_BACKGROUND;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 60; box_m.y1 = 60; /* 50x50, fully on-screen, topmost (created last) */
    rc = wuss_window_create(wuss,
                            &box_m,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_m,
                            box_m.x1 - box_m.x0,
                            box_m.y1 - box_m.y0,
                            &win_m);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidation, paint M's initial content */
    if (rc != result_OK)
      goto Failure;

    wuss_window_move(win_m, -40, 10); /* slide left until half of M is off the left edge */
    rc = wuss_redraw_dirty(wuss); /* flush the vacated-sliver repaint from this move */
    if (rc != result_OK)
      goto Failure;

    before_m = tc_m.redraw_count;

    wuss_window_move(win_m, 10, 10); /* slide back: the part that re-enters the screen was
                                       * never blitted (its source pixels were off-screen),
                                       * so it must be a real task redraw, not a blit */
    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_m.redraw_count != before_m + 1)
      goto Failure; /* M must get a genuine redraw call to repaint the reappeared part */

    wuss_window_destroy(win_m);
  }

  printf("test: Adjust-click on a window's back icon brings it to front\n");

  {
    test_task_t    tc_g, tc_h;
    wuss_task_t    delegate_g, delegate_h;
    box_t          box_g, box_h;
    wuss_window_t *win_g, *win_h;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h.handle    = test_handle;
    delegate_h.task_data = &tc_h;
    delegate_h.bg        = wuss_NO_BACKGROUND;

    box_h.x0 = 130; box_h.y0 = 50;
    box_h.x1 = 190; box_h.y1 = 100;
    rc = wuss_window_create(wuss,
                            &box_h,
                            "H",
                            wuss_WINDOW_NONE,
                            &delegate_h,
                            box_h.x1 - box_h.x0,
                            box_h.y1 - box_h.y0,
                            &win_h);
    if (rc != result_OK)
      goto Failure;

    tc_g.redraw_count = 0;
    tc_g.mouse_count  = 0;
    delegate_g.handle    = test_handle;
    delegate_g.task_data = &tc_g;
    delegate_g.bg        = wuss_NO_BACKGROUND;

    box_g.x0 = 110; box_g.y0 = 30;
    box_g.x1 = 160; box_g.y1 = 80;
    rc = wuss_window_create(wuss,
                            &box_g,
                            "G",
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL,
                            &delegate_g,
                            box_g.x1 - box_g.x0,
                            box_g.y1 - box_g.y0,
                            &win_g);
    if (rc != result_OK)
      goto Failure;

    /* G is topmost here, overlapping H; G's own back icon (top-left
     * corner) never falls under H, so it stays clickable either way */
    wuss_window_get_visible_bounds(win_g, &visible);

    rc = wuss_mouse_click(wuss, 145, 65, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap of G and H */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, 145, 65, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, visible.x0 + 5, visible.y0 + 5, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's back icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, visible.x0 + 5, visible.y0 + 5, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, 145, 65, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: H now on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_h) /* G was sent to back */
      goto Failure;

    rc = wuss_mouse_click(wuss, 145, 65, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, visible.x0 + 5, visible.y0 + 5, wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* Adjust-click G's back icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, visible.x0 + 5, visible.y0 + 5, wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, 145, 65, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: G back on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g) /* Adjust-click on the back icon brought G back to front */
      goto Failure;

    rc = wuss_mouse_click(wuss, 145, 65, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    printf("test: drag-resize stops at doc_width/doc_height, not just WUSS_MIN_CONTENT\n");

    wuss_window_get_content_bounds(win_g, &content); /* G's doc_width/doc_height are 50x50, same as its initial content size */

    rc = wuss_mouse_click(wuss, visible.x1 - 3, visible.y1 - 3, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_move(wuss, content.x0 + 50, content.y0 + 50, &hit); /* drag to exactly the doc extent */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, content.x0 + 50, content.y0 + 50, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_g, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;

    rc = wuss_mouse_click(wuss, visible.x1 - 3, visible.y1 - 3, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's resize icon again */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_move(wuss, content.x0 + 500, content.y0 + 500, &hit); /* drag far past the doc extent */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, content.x0 + 500, content.y0 + 500, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_g, &content);
    if (content.x1 - content.x0 != width || content.y1 - content.y0 != height)
      goto Failure; /* clamped to the same size as dragging to exactly the doc extent: not left to grow past it */

    wuss_window_destroy(win_g);
    wuss_window_destroy(win_h);
  }

  printf("test: destroy mid-drag then move doesn't crash\n");

  tc_c.redraw_count = 0;
  tc_c.mouse_count  = 0;
  delegate_c.handle      = test_handle;
  delegate_c.task_data = &tc_c;
  delegate_c.bg          = wuss_NO_BACKGROUND;

  box_c.x0 = 0;
  box_c.y0 = 0;
  box_c.x1 = 40;
  box_c.y1 = 40;
  rc = wuss_window_create(wuss,
                          &box_c,
                          "C",
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          &delegate_c,
                          box_c.x1 - box_c.x0,
                          box_c.y1 - box_c.y0,
                          &win_c);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, 31, 11, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* C's titlebar, above its content, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_c)
    goto Failure;

  wuss_window_destroy(win_c);

  rc = wuss_mouse_move(wuss, 20, 20, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: destroy sends wuss_EVENT_QUIT to each window's task\n");

  tc_a.stop_count = 0;
  tc_b.stop_count = 0;
  wuss_window_destroy(win_a);
  wuss_window_destroy(win_b);
  if (tc_a.stop_count != 1 || tc_b.stop_count != 1)
    goto Failure;

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
