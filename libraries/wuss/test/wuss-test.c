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
#include "tasks/chars.h"
#include "tasks/checker.h"
#include "tasks/curve.h"
#include "tasks/gradient.h"
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
static chars_task_t    g_chars_task;
static palette_task_t  g_palette_task;
static image_task_t    g_image_task;
static checker_task_t  g_checker_task;
static curve_task_t    g_curve_task;
static sofa_task_t     g_sofa_task;
static gradient_task_t g_gradient_task;

static result_t spawn_ball(void)     { return ball_create(g_wuss, g_palette, &g_ball_task); }
static result_t spawn_text(void)     { return text_create(g_wuss, g_palette, g_daydream_font, &g_text_task); }
static result_t spawn_blank(void)    { return blank_create(g_wuss, g_npalette, &g_blank_task); }
static result_t spawn_chars(void)    { return chars_create(g_wuss, g_palette, &g_chars_task); }
static result_t spawn_palette(void)  { return palette_create(g_wuss, g_palette, g_npalette, &g_palette_task); }
static result_t spawn_image(void)    { return image_create(g_wuss, g_palette, g_resources, &g_image_task); }
static result_t spawn_checker(void)  { return checker_create(g_wuss, g_palette, &g_checker_task); }
static result_t spawn_curve(void)    { return curve_create(g_wuss, g_palette, &g_curve_task); }
static result_t spawn_sofa(void)     { return sofa_create(g_wuss, g_palette, &g_sofa_task); }
static result_t spawn_gradient(void) { return gradient_create(g_wuss, &g_gradient_task); }

static void destroy_ball(void)     { ball_destroy(&g_ball_task); }
static void destroy_text(void)     { text_destroy(&g_text_task); }
static void destroy_blank(void)    { blank_destroy(&g_blank_task); }
static void destroy_chars(void)    { chars_destroy(&g_chars_task); }
static void destroy_palette(void)  { palette_destroy(&g_palette_task); }
static void destroy_image(void)    { image_destroy(&g_image_task); }
static void destroy_checker(void)  { checker_destroy(&g_checker_task); }
static void destroy_curve(void)    { curve_destroy(&g_curve_task); }
static void destroy_sofa(void)     { sofa_destroy(&g_sofa_task); }
static void destroy_gradient(void) { gradient_destroy(&g_gradient_task); }

static launcher_entry_t g_launcher_entries[] =
{
  { "Ball",     spawn_ball,     destroy_ball,     false },
  { "Text",     spawn_text,     destroy_text,     false },
  { "Blank",    spawn_blank,    destroy_blank,    false },
  { "Chars",    spawn_chars,    destroy_chars,    false },
  { "Palette",  spawn_palette,  destroy_palette,  false },
  { "Image",    spawn_image,    destroy_image,    false },
  { "Checker",  spawn_checker,  destroy_checker,  false },
  { "Curve",    spawn_curve,    destroy_curve,    false },
  { "Sofa",     spawn_sofa,     destroy_sofa,     false },
  { "Gradient", spawn_gradient, destroy_gradient, false }
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

  leafname = path_join_leafname("digits", "png");
  filename = path_join_filename(resources, 3, "resources", "bmfonts", leafname);
  rc = bmfont_create(filename, &font);
  if (rc != result_OK)
    goto Failure;

  leafname = path_join_leafname("daydream", "png");
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

    config.titlebar_height   = 0;
    config.palette.title.bg  = palette_PICO8_DARK_BLUE;
    config.palette.title.fg  = palette_PICO8_WHITE;
    config.palette.back      = palette_PICO8_GREEN;
    config.palette.close     = palette_PICO8_RED;
    config.palette.toggle    = palette_PICO8_ORANGE;
    config.palette.resize    = palette_PICO8_LAVENDER;
    config.palette.arrows    = palette_PICO8_BLUE;
    config.palette.wells     = palette_PICO8_DARK_BLUE;
    config.palette.sausages  = palette_PICO8_LIGHT_GREY;

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
          wuss_mouse_click(wuss, (point_t) { x, y }, sdl_button_to_wuss(event.button.button), wuss_MOUSE_DOWN, NULL);
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.button.x, event.button.y, &x, &y);
          wuss_mouse_click(wuss, (point_t) { x, y }, sdl_button_to_wuss(event.button.button), wuss_MOUSE_UP, NULL);
        }
        break;

      case SDL_EVENT_MOUSE_MOTION:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.motion.x, event.motion.y, &x, &y);
          wuss_mouse_move(wuss, (point_t) { x, y }, NULL);
        }
        break;

      case SDL_EVENT_MOUSE_WHEEL:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.wheel.mouse_x, event.wheel.mouse_y, &x, &y);
          wuss_scroll(wuss, (point_t) { x, y }, (int) event.wheel.y, NULL);
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
  int                 open_count;
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
    tc->last_x      = event->data.mouse.point.x;
    tc->last_y      = event->data.mouse.point.y;
    tc->last_button = event->data.mouse.button;
    break;

  case wuss_EVENT_CLOSE:
    tc->close_count++;
    break;

  case wuss_EVENT_QUIT:
    tc->stop_count++;
    break;

  case wuss_EVENT_OPEN:
    tc->open_count++;
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

  bad_config.titlebar_height  = 0;
  bad_config.palette.title.bg = 999;
  bad_config.palette.title.fg = 0;
  bad_config.palette.back     = 0;
  bad_config.palette.close    = 0;
  bad_config.palette.toggle   = 0;
  bad_config.palette.resize   = 0;
  bad_config.palette.arrows   = 0;
  bad_config.palette.wells    = 0;
  bad_config.palette.sausages = 0;
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
  tc_a.open_count   = 0;
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
  /* A's visible footprint is L-shaped (B covers its bottom-right corner),
   * so it's redrawn as two non-overlapping pieces; B is unoccluded, one */
  if (tc_a.redraw_count != 2 || tc_b.redraw_count != 1)
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
  rc = wuss_mouse_click(wuss, (point_t) { 75, 75 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;
  if (tc_b.last_action != wuss_MOUSE_DOWN || tc_b.last_x != 25 || tc_b.last_y != 25)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 75, 75 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: clicking a window's close icon sends wuss_EVENT_CLOSE, not a drag\n");

  tc_a.close_count = 0;
  rc = wuss_mouse_click(wuss, (point_t) { 6, 11 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.close_count != 1)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 31, 36 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit); /* if the close click had started a drag, this would move A */
  if (rc != result_OK)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 0)
    goto Failure; /* unmoved: no drag was started by the close click */

  printf("test: click-to-front changes subsequent overlap hits\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, (point_t) { 31, 11 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's titlebar, above its content, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 31, 11 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click does not change z-order\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_click(wuss, (point_t) { 120, 120 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* B's content, only within B */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 120, 120 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, (point_t) { 75, 75 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A still topmost: B's content click above didn't bring it to front */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1 || tc_a.last_x != 74 || tc_a.last_y != 54)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 75, 75 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: titlebar click starts a drag, not delivered as content\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, (point_t) { 31, 11 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's titlebar, A already topmost, clear of the close icon */
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
  rc = wuss_mouse_move(wuss, (point_t) { 31, 36 }, &hit);
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

  rc = wuss_mouse_click(wuss, (point_t) { 31, 36 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  printf("test: Adjust-drag moves a window without bringing it to front\n");

  rc = wuss_mouse_click(wuss, (point_t) { 140, 35 }, wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* B's titlebar, clear of A */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_move(wuss, (point_t) { 145, 60 }, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  wuss_window_get_visible_bounds(win_b, &visible);
  if (visible.x0 != 54 || visible.y0 != 54)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 145, 60 }, wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 75, 75 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* within both A and B; A still topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 75, 75 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_move(wuss, (point_t) { 200, 200 }, &hit); /* off all windows, drag must have ended */
  if (rc != result_OK)
    goto Failure;
  if (hit != NULL)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 25)
    goto Failure;

  if (tc_a.open_count != 1)
    goto Failure; /* wuss_EVENT_OPEN sent once for the drag-move above */

  printf("test: window_resize valid and too-small cases\n");

  rc = wuss_window_resize(win_a, 50, 0); /* zero-height content is invalid */
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;
  if (tc_a.open_count != 1)
    goto Failure; /* rejected resize: no wuss_EVENT_OPEN */

  rc = wuss_window_resize(win_a, 50, 50);
  if (rc != result_OK)
    goto Failure;
  if (tc_a.open_count != 2)
    goto Failure; /* wuss_EVENT_OPEN sent for the successful resize */

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

  rc = wuss_mouse_click(wuss, (point_t) { 5, 165 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_d)
    goto Failure;
  if (tc_d.mouse_count != 1 || tc_d.last_action != wuss_MOUSE_DOWN || tc_d.last_x != 5 || tc_d.last_y != 5)
    goto Failure;

  rc = wuss_mouse_click(wuss, (point_t) { 5, 165 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
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
    rc = wuss_mouse_click(wuss, (point_t) { 110, 10 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* within E only */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_e)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { 110, 10 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { 135, 25 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: F still on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_f)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { 135, 25 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_e);
    wuss_window_close(win_f);
  }

  printf("test: wuss_window_set_background\n");

  rc = wuss_window_set_background(win_d, 999);
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  rc = wuss_window_set_background(win_d, palette_PICO8_ORANGE);
  if (rc != result_OK)
    goto Failure;

  wuss_window_close(win_d);

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

    wuss_window_move(win_h, (point_t) { 60, 60 }); /* still entirely within G's footprint */
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

    wuss_window_close(win_h);
    wuss_window_close(win_g);
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

    wuss_window_close(win_i);
    wuss_window_close(win_j);
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

    wuss_window_move(win_m, (point_t) { -40, 10 }); /* slide left until half of M is off the left edge */
    rc = wuss_redraw_dirty(wuss); /* flush the vacated-sliver repaint from this move */
    if (rc != result_OK)
      goto Failure;

    before_m = tc_m.redraw_count;

    wuss_window_move(win_m, (point_t) { 10, 10 }); /* slide back: the part that re-enters the screen was
                                       * never blitted (its source pixels were off-screen),
                                       * so it must be a real task redraw, not a blit */
    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_m.redraw_count != before_m + 1)
      goto Failure; /* M must get a genuine redraw call to repaint the reappeared part */

    wuss_window_close(win_m);
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

    rc = wuss_mouse_click(wuss, (point_t) { 145, 65 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap of G and H */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { 145, 65 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { visible.x0 + 5, visible.y0 + 5 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's back icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { visible.x0 + 5, visible.y0 + 5 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { 145, 65 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: H now on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_h) /* G was sent to back */
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { 145, 65 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { visible.x0 + 5, visible.y0 + 5 }, wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* Adjust-click G's back icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { visible.x0 + 5, visible.y0 + 5 }, wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { 145, 65 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: G back on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g) /* Adjust-click on the back icon brought G back to front */
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { 145, 65 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    printf("test: drag-resize stops at doc_width/doc_height, not just WUSS_MIN_CONTENT\n");

    wuss_window_get_content_bounds(win_g, &content); /* G's doc_width/doc_height are 50x50, same as its initial content size */

    rc = wuss_mouse_click(wuss, (point_t) { visible.x1 - 3, visible.y1 - 3 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_move(wuss, (point_t) { content.x0 + 50, content.y0 + 50 }, &hit); /* drag to exactly the doc extent */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { content.x0 + 50, content.y0 + 50 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_g, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;

    rc = wuss_mouse_click(wuss, (point_t) { visible.x1 - 3, visible.y1 - 3 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's resize icon again */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_move(wuss, (point_t) { content.x0 + 500, content.y0 + 500 }, &hit); /* drag far past the doc extent */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, (point_t) { content.x0 + 500, content.y0 + 500 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_g, &content);
    if (content.x1 - content.x0 != width || content.y1 - content.y0 != height)
      goto Failure; /* clamped to the same size as dragging to exactly the doc extent: not left to grow past it */

    wuss_window_close(win_g);
    wuss_window_close(win_h);
  }

  printf("test: toggle-size blits rather than redrawing the whole window\n");

  {
    test_task_t    tc_t;
    wuss_task_t    delegate_t;
    box_t          box_t_win, before, after, titlebar, toggle;
    wuss_window_t *win_t;
    int            outline_px, titlebar_height, inset, icon;
    int            i, dirty_area, full_area, cx, cy, old_icon_x, old_icon_y, found;
    int            interior_x, interior_y, interior_dirty;
    int            old_vscroll_x, old_vscroll_y, old_vscroll_found;

    tc_t.redraw_count = 0;
    tc_t.mouse_count  = 0;
    delegate_t.handle    = test_handle;
    delegate_t.task_data = &tc_t;
    delegate_t.bg        = wuss_NO_BACKGROUND;

    box_t_win.x0 = 10; box_t_win.y0 = 10;
    box_t_win.x1 = 50; box_t_win.y1 = 50; /* 40x40 content, room to grow to a 200x200 doc */
    rc = wuss_window_create(wuss, &box_t_win, "T", wuss_WINDOW_NONE,
                            &delegate_t, 200, 200, &win_t);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    /* toggle icon: top-right of the titlebar, inset by 3px, sized 20 - 2*3
     * (default titlebar height 20, WUSS_ICON_INSET 3), matching
     * wuss__toggle_box's formula -- mirrored here since the test only sees
     * the public API */
    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_t, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;
    old_icon_x = cx; old_icon_y = cy; /* pre-grow icon centre: ends up mid-titlebar once the window widens */

    /* pre-grow content interior, well clear of outline/titlebar/scrollbar
     * furniture on every side (carve.x == carve.y == icon here, since both
     * scrollbars are enabled) -- a point the blit must genuinely have
     * preserved, unlike a naive summed-region-area comparison, which
     * overcounts once furniture invalidation adds several regions that
     * overlap each other and the grown-edge region without merging (only
     * exact-edge-aligned boxes merge; see box_merge in invalidate.c) */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    /* pre-grow vscroll column, at the *old* right edge: once the window
     * widens this sits mid-content rather than at the (now further right)
     * new column, inside the region the blit reuses as valid pixels --
     * the content redraw never touches it, so only a forced old-furniture
     * invalidate stops the old scrollbar glyph being left behind there */
    old_vscroll_x = before.x1 - outline_px - icon / 2;
    old_vscroll_y = interior_y;

    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* T's toggle-size icon: grow */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_t)
      goto Failure;
    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    found             = 0;
    interior_dirty    = 0;
    old_vscroll_found = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, old_icon_x, old_icon_y))
        found = 1;
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
      if (box_contains_point(&region, old_vscroll_x, old_vscroll_y))
        old_vscroll_found = 1;
    }
    if (!old_vscroll_found)
      goto Failure; /* the old vscroll column, now mid-content rather than at
                      * the (further right) new right edge, falls inside both
                      * "before" and the grown "visible" same as the toggle
                      * icon above -- the blit alone leaves its stale pixels
                      * on screen unless the old furniture position is also
                      * forced dirty */
    if (!found)
      goto Failure; /* the old toggle-icon glyph, now mid-titlebar rather than
                      * at its corner, falls inside both "before" and the
                      * grown "visible" -- the content blit alone would leave
                      * it un-redrawn as a ghost; furniture must be forced
                      * dirty separately since its layout depends on size */

    if (interior_dirty)
      goto Failure; /* interior content pixel, untouched by any furniture or
                      * grown-edge region: the blit must have reused it */

    wuss_window_get_visible_bounds(win_t, &after);
    if (after.x1 > 200 || after.y1 > 200)
      goto Failure; /* maximizing must stay on-screen: bounded by what's left
                      * of the screen from T's own x0/y0 (10,10), not by the
                      * screen's full width/height as if T were at the origin */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. Icon moved with the grown titlebar, so recompute. */
    wuss_window_get_visible_bounds(win_t, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* T's toggle-size icon: shrink */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_t)
      goto Failure;
    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    dirty_area = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      dirty_area += (region.x1 - region.x0) * (region.y1 - region.y0);
    }

    full_area = (before.x1 - before.x0) * (before.y1 - before.y0); /* the grown box: what a full-union invalidate would have covered */
    if (dirty_area >= full_area)
      goto Failure; /* vacated edge only, not the whole grown footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_t);
  }

  printf("test: toggle-size that forces a scroll re-clamp invalidates the content it's about to redraw at the new offset, not just the blit's edge sliver\n");

  {
    test_task_t    tc_r;
    wuss_task_t    delegate_r;
    box_t          box_r, before, titlebar, toggle;
    wuss_window_t *win_r;
    int            outline_px, titlebar_height, inset, icon;
    int            i, interior_x, interior_y, interior_dirty, cx, cy;

    tc_r.redraw_count = 0;
    tc_r.mouse_count  = 0;
    delegate_r.handle    = test_handle;
    delegate_r.task_data = &tc_r;
    delegate_r.bg        = wuss_NO_BACKGROUND;

    box_r.x0 = 10; box_r.y0 = 10;
    box_r.x1 = 50; box_r.y1 = 50; /* 40x40 content; doc bigger than that, so it starts scrollable */
    rc = wuss_window_create(wuss, &box_r, "R", wuss_WINDOW_NONE,
                            &delegate_r, 70, 70, &win_r);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_r, (point_t) { 0, 15 }); /* within range: max_y = 70 - 40 = 30 */

    rc = wuss_redraw_dirty(wuss); /* flush the scroll's own invalidate */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_r, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    /* interior of the OLD content box, clear of outline/titlebar/scrollbar
     * furniture -- well inside "before", so a plain grow (no re-clamp) would
     * leave it untouched by the blit-reuse optimisation, same as the
     * "toggle-size blits" test above. But this window starts scrolled, and
     * growing to doc_height (70) here forces content_size up to doc_size,
     * clamping scroll.y back to 0 -- the content this point shows is stale
     * regardless of the blit, so it must be invalidated outright. */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* R's toggle-size icon: grow past doc_height, forcing a scroll re-clamp */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_r)
      goto Failure;
    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (!interior_dirty)
      goto Failure; /* the re-clamp changed scroll.y, so every pixel in the
                      * content box is now showing the wrong offset -- if
                      * this interior point (untouched by the toggle's own
                      * blit-reuse/furniture invalidation) isn't marked
                      * dirty, the fix has regressed to relying on
                      * wuss_window_set_scroll's live blit-and-shift, which
                      * is invalid mid-toggle: the screen doesn't reflect
                      * the new geometry yet at that point */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_r);
  }

  printf("test: wuss_WINDOW_NO_TOGGLE_BLIT redraws the whole window instead of blitting\n");

  {
    test_task_t    tc_nb;
    wuss_task_t    delegate_nb;
    box_t          box_nb, before, titlebar, toggle;
    wuss_window_t *win_nb;
    int            outline_px, titlebar_height, inset, icon;
    int            i, cx, cy, interior_x, interior_y, interior_dirty;

    tc_nb.redraw_count = 0;
    tc_nb.mouse_count  = 0;
    delegate_nb.handle    = test_handle;
    delegate_nb.task_data = &tc_nb;
    delegate_nb.bg        = wuss_NO_BACKGROUND;

    box_nb.x0 = 10; box_nb.y0 = 10;
    box_nb.x1 = 50; box_nb.y1 = 50; /* 40x40 content, room to grow to a 200x200 doc */
    rc = wuss_window_create(wuss, &box_nb, "NB", wuss_WINDOW_NO_TOGGLE_BLIT,
                            &delegate_nb, 200, 200, &win_nb);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_nb, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    /* same pre-grow interior point as the blit test above -- there, the
     * blit must preserve it (never dirty); here, the flag must force a full
     * redraw instead of a blit, so this point must come out dirty */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* NB's toggle-size icon: grow */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_nb)
      goto Failure;
    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (!interior_dirty)
      goto Failure; /* wuss_WINDOW_NO_TOGGLE_BLIT must skip the blit path
                      * entirely, so even an interior pixel the blit would
                      * otherwise have preserved comes out dirty */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_nb);
  }

  printf("test: toggle-size maximize accounts for scrollbar furniture, not just outline/titlebar\n");

  {
    test_task_t    tc_u;
    wuss_task_t    delegate_u;
    box_t          box_u, ub, titlebar, toggle;
    wuss_window_t *win_u;
    int            outline_px, titlebar_height, inset, icon;
    int            cx, cy;

    tc_u.redraw_count = 0;
    tc_u.mouse_count  = 0;
    delegate_u.handle    = test_handle;
    delegate_u.task_data = &tc_u;
    delegate_u.bg        = wuss_NO_BACKGROUND;

    box_u.x0 = 80; box_u.y0 = 80;
    box_u.x1 = 120; box_u.y1 = 120; /* 40x40 content */
    rc = wuss_window_create(wuss, &box_u, "U", wuss_WINDOW_NONE, /* scrollbars on: carve.x/y = icon size */
                            &delegate_u, 70, 70, &win_u); /* doc size well within the 200x200 screen: growth is doc-limited, not screen-limited */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_u, &ub);
    titlebar.x0 = ub.x0 + outline_px;
    titlebar.x1 = ub.x1 - outline_px;
    titlebar.y0 = ub.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* U's toggle-size icon: grow to doc size */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_u)
      goto Failure;
    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_u, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;
    if (width != 70 || height != 70)
      goto Failure; /* visible must grow by the scrollbar breadth too, on top
                      * of outline/titlebar, or content ends up icon-size
                      * short of doc_width/doc_height */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. Icon moved with the grown titlebar, so recompute. */
    wuss_window_get_visible_bounds(win_u, &ub);
    titlebar.x0 = ub.x0 + outline_px;
    titlebar.x1 = ub.x1 - outline_px;
    titlebar.y0 = ub.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* U's toggle-size icon: shrink back */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_u)
      goto Failure;
    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_u, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;
    if (width != 40 || height != 40)
      goto Failure; /* restores exactly the pre-toggle content size */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_u);
  }

  printf("test: toggle-size maximize stays a valid box for a window dragged off the right/bottom edge\n");

  {
    test_task_t    tc_v;
    wuss_task_t    delegate_v;
    box_t          box_v, vb, titlebar, toggle;
    wuss_window_t *win_v;
    int            outline_px, titlebar_height, inset, icon;
    int            cx, cy;

    tc_v.redraw_count = 0;
    tc_v.mouse_count  = 0;
    delegate_v.handle    = test_handle;
    delegate_v.task_data = &tc_v;
    delegate_v.bg        = wuss_NO_BACKGROUND;

    box_v.x0 = 10; box_v.y0 = 10;
    box_v.x1 = 70; box_v.y1 = 70; /* 60x60 content -- wide enough titlebar that
                                    * back/close/toggle icons don't overlap
                                    * (a titlebar much narrower than that makes
                                    * the toggle icon's box overlap close's, so
                                    * a click meant for toggle lands on close
                                    * instead, since hit-test checks close
                                    * first -- a separate, pre-existing issue
                                    * unrelated to toggle-size specifically).
                                    * Doc big enough that maximize is
                                    * screen-limited, not doc-limited. */
    rc = wuss_window_create(wuss, &box_v, "V", wuss_WINDOW_NONE,
                            &delegate_v, 200, 200, &win_v);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* drag well past the right/bottom screen edge (200x200 screen) -- an
     * ordinary, already-supported position (see the drag-off-screen test
     * above): titlebar drag calls wuss_window_move with no clamping. From
     * here, "available space to the screen edge" (scr_width - visible.x0 -
     * furniture) goes negative, further than furniture alone can absorb. */
    wuss_window_move(win_v, (point_t) { 310, 310 });

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_v, &vb);
    titlebar.x0 = vb.x0 + outline_px;
    titlebar.x1 = vb.x1 - outline_px;
    titlebar.y0 = vb.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* V's toggle-size icon: maximize while off-screen */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_v)
      goto Failure;
    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_v, &vb);
    if (vb.x1 <= vb.x0 || vb.y1 <= vb.y0)
      goto Failure; /* screen-limited width/height went negative (x0 is
                      * further right than the screen edge plus furniture
                      * can make room for), producing an inverted box --
                      * un-hit-testable forever after, since box_contains_point
                      * can never match x0>x1: the window is stuck, unclickable,
                      * unclosable. Must floor at WUSS_MIN_CONTENT like
                      * drag-resize.c does, even if that leaves the maximized
                      * window hanging off the visible screen. */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. Confirm the window is still reachable at all --
     * this alone would already fail (rc != result_OK from a wrong hit) if
     * the icon had become permanently unhittable above. */
    wuss_window_get_visible_bounds(win_v, &vb);
    titlebar.x0 = vb.x0 + outline_px;
    titlebar.x1 = vb.x1 - outline_px;
    titlebar.y0 = vb.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* V's toggle-size icon: shrink back */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_v)
      goto Failure;
    rc = wuss_mouse_click(wuss, (point_t) { cx, cy }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_v, &vb);
    if (vb.x1 <= vb.x0 || vb.y1 <= vb.y0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_v);
  }

  printf("test: dragging a back-most window with nothing above it still blits\n");

  {
    test_task_t    tc_k, tc_l;
    wuss_task_t    delegate_k, delegate_l;
    box_t          box_k, box_l;
    wuss_window_t *win_k, *win_l;
    int            before_k, before_l;

    tc_k.redraw_count = 0;
    tc_k.mouse_count  = 0;
    delegate_k.handle    = test_handle;
    delegate_k.task_data = &tc_k;
    delegate_k.bg        = wuss_NO_BACKGROUND;

    box_k.x0 = 0; box_k.y0 = 140; /* clear of the still-open A/B windows above */
    box_k.x1 = 50; box_k.y1 = 175;
    rc = wuss_window_create(wuss,
                            &box_k,
                            "K",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_k,
                            box_k.x1 - box_k.x0,
                            box_k.y1 - box_k.y0,
                            &win_k);
    if (rc != result_OK)
      goto Failure;

    tc_l.redraw_count = 0;
    tc_l.mouse_count  = 0;
    delegate_l.handle    = test_handle;
    delegate_l.task_data = &tc_l;
    delegate_l.bg        = wuss_NO_BACKGROUND;

    box_l.x0 = 120; box_l.y0 = 140; /* well clear of K, so never overlaps it */
    box_l.x1 = 170; box_l.y1 = 175;
    rc = wuss_window_create(wuss,
                            &box_l,
                            "L",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_l,
                            box_l.x1 - box_l.x0,
                            box_l.y1 - box_l.y0,
                            &win_l);
    if (rc != result_OK)
      goto Failure;

    wuss_window_restack(win_k, wuss_ZORDER_BACK); /* K is no longer topmost, but L never overlaps it */

    rc = wuss_redraw_dirty(wuss); /* flush the restack's own dirty region first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_k, &visible);

    rc = wuss_mouse_click(wuss, (point_t) { visible.x0 + 31, visible.y0 + 11 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* K's titlebar, clear of the close icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_k)
      goto Failure;

    before_k = tc_k.redraw_count;
    before_l = tc_l.redraw_count;
    rc = wuss_mouse_move(wuss, (point_t) { visible.x0 + 45, visible.y0 + 21 }, &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_k)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_k.redraw_count != before_k || tc_l.redraw_count != before_l)
      goto Failure; /* blitted, not redrawn: nothing above K overlapped its old
                      * footprint, so the move fast path must still apply even
                      * though K isn't topmost */

    rc = wuss_mouse_click(wuss, (point_t) { visible.x0 + 45, visible.y0 + 21 }, wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_k);
    wuss_window_close(win_l);
  }

  printf("test: resizing a window only invalidates the grown/shrunk sliver\n");

  {
    test_task_t    tc_m2;
    wuss_task_t    delegate_m2;
    box_t          box_m2, before2, after2, region;
    wuss_window_t *win_m2;
    int            i, dirty_area, full_area, interior_x, interior_y, interior_dirty;

    tc_m2.redraw_count = 0;
    tc_m2.mouse_count  = 0;
    delegate_m2.handle    = test_handle;
    delegate_m2.task_data = &tc_m2;
    delegate_m2.bg        = wuss_NO_BACKGROUND;

    box_m2.x0 = 0; box_m2.y0 = 0;
    box_m2.x1 = 40; box_m2.y1 = 40;
    rc = wuss_window_create(wuss,
                            &box_m2,
                            "M2",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL,
                            &delegate_m2,
                            box_m2.x1 - box_m2.x0,
                            box_m2.y1 - box_m2.y0,
                            &win_m2);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the creation invalidation first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_m2, &before2);
    interior_x = before2.x0 + 2;  /* inside the untouched left edge */
    interior_y = before2.y0 + 25; /* below the titlebar, in plain content */

    rc = wuss_window_resize(win_m2, 80, 80); /* grow */
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    dirty_area     = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
      dirty_area += (region.x1 - region.x0) * (region.y1 - region.y0);
    }
    if (interior_dirty)
      goto Failure; /* untouched top-left corner, unchanged by growing bottom-right */

    wuss_window_get_visible_bounds(win_m2, &after2);
    full_area = (after2.x1 - after2.x0) * (after2.y1 - after2.y0);
    if (dirty_area >= full_area)
      goto Failure; /* must be less than a full redraw of the grown footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    before2 = after2;

    rc = wuss_window_resize(win_m2, 40, 40); /* shrink back */
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    dirty_area     = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
      dirty_area += (region.x1 - region.x0) * (region.y1 - region.y0);
    }
    if (interior_dirty)
      goto Failure; /* still untouched: the corner that remains after shrinking */

    full_area = (before2.x1 - before2.x0) * (before2.y1 - before2.y0);
    if (dirty_area >= full_area)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_m2);
  }

  printf("test: resizing a wuss_WINDOW_NO_TOGGLE_BLIT window redraws it fully\n");

  {
    test_task_t    tc_nb2;
    wuss_task_t    delegate_nb2;
    box_t          box_nb2, before3, after3, region;
    wuss_window_t *win_nb2;
    int            i, dirty_area, full_area;

    tc_nb2.redraw_count = 0;
    tc_nb2.mouse_count  = 0;
    delegate_nb2.handle    = test_handle;
    delegate_nb2.task_data = &tc_nb2;
    delegate_nb2.bg        = wuss_NO_BACKGROUND;

    box_nb2.x0 = 0; box_nb2.y0 = 0;
    box_nb2.x1 = 40; box_nb2.y1 = 40;
    rc = wuss_window_create(wuss, &box_nb2, "NB2", wuss_WINDOW_NO_TOGGLE_BLIT,
                            &delegate_nb2,
                            box_nb2.x1 - box_nb2.x0,
                            box_nb2.y1 - box_nb2.y0,
                            &win_nb2);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the creation invalidation first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_nb2, &before3);

    rc = wuss_window_resize(win_nb2, 80, 80); /* grow */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_nb2, &after3);
    full_area  = (after3.x1 - after3.x0) * (after3.y1 - after3.y0);
    dirty_area = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      dirty_area += (region.x1 - region.x0) * (region.y1 - region.y0);
    }
    if (dirty_area < full_area)
      goto Failure; /* NO_TOGGLE_BLIT must fully redraw, not just the sliver */

    wuss_window_close(win_nb2);
  }

  printf("test: dragging a clear window onto an occluder leaves the occluder untouched\n");

  {
    test_task_t    tc_n, tc_o;
    wuss_task_t    delegate_n, delegate_o;
    box_t          box_n, box_o, visible_o, exposed, occluded, region;
    wuss_window_t *win_n, *win_o;
    int            i, exposed_dirty, occluded_dirty;

    tc_n.redraw_count = 0;
    tc_n.mouse_count  = 0;
    delegate_n.handle    = test_handle;
    delegate_n.task_data = &tc_n;
    delegate_n.bg        = wuss_NO_BACKGROUND;

    box_n.x0 = 0; box_n.y0 = 140; /* clear of any occluder to start */
    box_n.x1 = 60; box_n.y1 = 170;
    rc = wuss_window_create(wuss, &box_n, "N",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_n,
                            box_n.x1 - box_n.x0,
                            box_n.y1 - box_n.y0,
                            &win_n);
    if (rc != result_OK)
      goto Failure;

    tc_o.redraw_count = 0;
    tc_o.mouse_count  = 0;
    delegate_o.handle    = test_handle;
    delegate_o.task_data = &tc_o;
    delegate_o.bg        = wuss_NO_BACKGROUND;

    box_o.x0 = 90; box_o.y0 = 140; /* N will be dragged partly on top of O */
    box_o.x1 = 130; box_o.y1 = 180;
    rc = wuss_window_create(wuss, &box_o, "O",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_o,
                            box_o.x1 - box_o.x0,
                            box_o.y1 - box_o.y0,
                            &win_o);
    if (rc != result_OK)
      goto Failure;

    /* O is created after N, so O is topmost -- N's destination footprint
     * will overlap an occluder above it in z-order. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_n, &visible);

    /* Move N far enough right that its new footprint lands partly under O
     * (which stays wholly untouched), while its old footprint started
     * entirely clear of O. */
    wuss_window_move(win_n, (point_t) { visible.x0 + 80, visible.y0 + 10 });

    wuss_window_get_visible_bounds(win_n, &visible);
    wuss_window_get_visible_bounds(win_o, &visible_o);
    exposed.x0 = visible.x0;   exposed.y0 = visible.y0;
    exposed.x1 = visible_o.x0; exposed.y1 = visible.y1; /* N's part left of O */
    box_intersection(&visible, &visible_o, &occluded); /* N's part under O */

    /* The part of N's new footprint that lands under O must NOT be queued
     * dirty -- O hasn't moved, so its pixels are already correct there, and
     * the move blit must have skipped blitting into that area rather than
     * pasting N's stale pixels over it and forcing a repair. */
    occluded_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &occluded))
        occluded_dirty = 1;
    }
    if (occluded_dirty)
      goto Failure;

    /* The exposed part of N's new footprint, not under any occluder, must
     * NOT be queued dirty -- it was already moved there correctly by the
     * blit, so redrawing it too would be exactly the "repaint what could
     * have been left in place" waste this fast path exists to avoid. */
    exposed_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &exposed))
        exposed_dirty = 1;
    }
    if (exposed_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_n);
    wuss_window_close(win_o);
  }

  printf("test: moving a partly-occluded window blits its clean part and only repaints the occluded part\n");

  {
    test_task_t    tc_a, tc_b;
    wuss_task_t    delegate_a, delegate_b;
    box_t          box_a, box_b, visible_a, visible_b_before;
    box_t          clean_new, hidden_new, region;
    wuss_window_t *win_a, *win_b;
    int            i, dx, clean_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b.handle    = test_handle;
    delegate_b.task_data = &tc_b;
    delegate_b.bg        = wuss_NO_BACKGROUND;

    box_b.x0 = 20; box_b.y0 = 10; /* left half will sit under A */
    box_b.x1 = 80; box_b.y1 = 50;
    rc = wuss_window_create(wuss, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_b,
                            box_b.x1 - box_b.x0,
                            box_b.y1 - box_b.y0,
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a.handle    = test_handle;
    delegate_a.task_data = &tc_a;
    delegate_a.bg        = wuss_NO_BACKGROUND;

    box_a.x0 = 0; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 40; box_a.y1 = 100;
    rc = wuss_window_create(wuss, &box_a, "A",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_a,
                            box_a.x1 - box_a.x0,
                            box_a.y1 - box_a.y0,
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (x:20-80,y:10-50) is split by A (x:0-40) into a
     * hidden strip (x:20-40, under A) and a clean strip (x:40-80, exposed). */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    /* Move B far enough right that its whole new footprint clears A. */
    dx = 60;
    wuss_window_move(win_b, (point_t) { visible_b_before.x0 + dx,
                                        visible_b_before.y0 });

    /* The clean strip (previously exposed, genuinely B's own pixels) lands
     * at its translated destination and must have been blitted there, not
     * repainted. */
    clean_new.x0 = 40 + dx; clean_new.y0 = 10;
    clean_new.x1 = 80 + dx; clean_new.y1 = 50;

    /* The hidden strip (previously under A, never B's valid rendering) has
     * no valid source pixels, so its translated destination must be a real
     * repaint. */
    hidden_new.x0 = 20 + dx; hidden_new.y0 = 10;
    hidden_new.x1 = 40 + dx; hidden_new.y1 = 50;

    hidden_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &hidden_new))
        hidden_dirty = 1;
    }
    if (!hidden_dirty)
      goto Failure;

    clean_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &clean_new))
        clean_dirty = 1;
    }
    if (clean_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: moving a window whose occluded piece was never blitted doesn't redraw the occluder\n");

  {
    test_task_t    tc_a, tc_b;
    wuss_task_t    delegate_a, delegate_b;
    box_t          box_a, box_b, visible_a, visible_b_before;
    box_t          region;
    wuss_window_t *win_a, *win_b;
    int            i, occluder_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b.handle    = test_handle;
    delegate_b.task_data = &tc_b;
    delegate_b.bg        = wuss_NO_BACKGROUND;

    box_b.x0 = 0; box_b.y0 = 0; /* right part sits under A throughout */
    box_b.x1 = 60; box_b.y1 = 40;
    rc = wuss_window_create(wuss, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_b,
                            box_b.x1 - box_b.x0,
                            box_b.y1 - box_b.y0,
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a.handle    = test_handle;
    delegate_a.task_data = &tc_a;
    delegate_a.bg        = wuss_NO_BACKGROUND;

    box_a.x0 = 40; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 100; box_a.y1 = 40;
    rc = wuss_window_create(wuss, &box_a, "A",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_a,
                            box_a.x1 - box_a.x0,
                            box_a.y1 - box_a.y0,
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    /* Move B straight down: its clean piece (x:0-40) and hidden piece
     * (x:40-60, under A) both stay clear of / under A exactly as before --
     * nothing about A's own pixels is ever touched by the blit, so A must
     * not be forced to redraw. */
    wuss_window_move(win_b, (point_t) { visible_b_before.x0,
                                        visible_b_before.y0 + 5 });

    occluder_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &visible_a))
        occluder_dirty = 1;
    }
    if (occluder_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: moving a window split by a mid-band occluder past the gap between bands blits both bands in a safe order\n");

  {
    test_task_t    tc_a, tc_b;
    wuss_task_t    delegate_a, delegate_b;
    box_t          box_a, box_b, visible_b_before;
    box_t          occluded_overlap, hidden_new, region;
    wuss_window_t *win_a, *win_b;
    int            i, occluded_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b.handle    = test_handle;
    delegate_b.task_data = &tc_b;
    delegate_b.bg        = wuss_NO_BACKGROUND;

    box_b.x0 = 0; box_b.y0 = 0; /* middle band sits under A */
    box_b.x1 = 60; box_b.y1 = 60;
    rc = wuss_window_create(wuss, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_b,
                            box_b.x1 - box_b.x0,
                            box_b.y1 - box_b.y0,
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a.handle    = test_handle;
    delegate_a.task_data = &tc_a;
    delegate_a.bg        = wuss_NO_BACKGROUND;

    box_a.x0 = 0; box_a.y0 = 20; /* created after B, so A is topmost */
    box_a.x1 = 60; box_a.y1 = 40;
    rc = wuss_window_create(wuss, &box_a, "A",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_a,
                            box_a.x1 - box_a.x0,
                            box_a.y1 - box_a.y0,
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (y:0-60) is split by A (y:20-40) into a top clean
     * band (y:0-20), a hidden middle band (y:20-40) and a bottom clean band
     * (y:40-60), each spanning the full width. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);

    /* Move B down by 25px, past the 20px gap between the two clean bands:
     * the top band's destination (y:25-45) lands on the bottom band's
     * still-unread old source (y:40-60). Blitted in the other order --
     * bottom band first (its destination y:65-85 doesn't touch the top
     * band's source), then the top band -- both blits are safe, so this
     * is not a genuine clobber cycle (translating disjoint pieces by the
     * same offset never produces one: any conflict is consistently
     * oriented by the direction of the move). The top band's destination
     * overlap with A (y:25-40) is skipped by the blit entirely (A hasn't
     * moved, its pixels there are already correct), so it must stay clean;
     * the translated hidden band (y:45-65, never had valid pixels) still
     * needs forcing dirty for a real repaint. */
    wuss_window_move(win_b, (point_t) { visible_b_before.x0,
                                        visible_b_before.y0 + 25 });

    occluded_overlap.x0 = 0;  occluded_overlap.y0 = 25;
    occluded_overlap.x1 = 60; occluded_overlap.y1 = 40;
    hidden_new.x0       = 0;  hidden_new.y0       = 45;
    hidden_new.x1       = 60; hidden_new.y1       = 65;

    occluded_dirty = hidden_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &occluded_overlap))
        occluded_dirty = 1;
      if (box_contains_box(&hidden_new, &region))
        hidden_dirty = 1;
    }
    if (occluded_dirty || !hidden_dirty)
      goto Failure;

    /* The blit must have actually happened, not fallen back: the part of
     * B's new footprint that's clear of A and not the hidden band (e.g.
     * the bottom band's new position, y:65-85) must not be dirtied. */
    {
      box_t clean_after, dirty_check;

      clean_after.x0 = 0;  clean_after.y0 = 65;
      clean_after.x1 = 60; clean_after.y1 = 85;
      for (i = 0; i < wuss_get_dirty_count(wuss); i++)
      {
        wuss_get_dirty(wuss, i, &region);
        if (!box_intersection(&region, &clean_after, &dirty_check))
          goto Failure;
      }
    }

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: dragging a window deeper under a corner occluder blits both L-shaped pieces in a safe order\n");

  {
    test_task_t    tc_a, tc_b;
    wuss_task_t    delegate_a, delegate_b;
    box_t          box_a, box_b, visible_b_before, visible_a, region;
    wuss_window_t *win_a, *win_b;
    int            i;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b.handle    = test_handle;
    delegate_b.task_data = &tc_b;
    delegate_b.bg        = wuss_NO_BACKGROUND;

    box_b.x0 = 80; box_b.y0 = 80; /* corner already under A */
    box_b.x1 = 140; box_b.y1 = 140;
    rc = wuss_window_create(wuss, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_b,
                            box_b.x1 - box_b.x0,
                            box_b.y1 - box_b.y0,
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a.handle    = test_handle;
    delegate_a.task_data = &tc_a;
    delegate_a.bg        = wuss_NO_BACKGROUND;

    box_a.x0 = 0; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 100; box_a.y1 = 100;
    rc = wuss_window_create(wuss, &box_a, "A",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            &delegate_a,
                            box_a.x1 - box_a.x0,
                            box_a.y1 - box_a.y0,
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (80,80)-(140,140) overlaps A (0,0)-(100,100) in its
     * corner (80,80)-(100,100); the rest of B is split into an L-shaped
     * clean region of two pieces, one of whose destination lands on the
     * other's still-unread source -- but blitting the other piece first
     * avoids that entirely, so this must NOT fall back to a full clipped
     * redraw (that was the "Adjust drag behind a corner fully redraws the
     * window" regression). Dragging B up-left by (-15,-15) also grows the
     * overlap with A without ever fully hiding or fully clearing it -- the
     * blit skips the part of each piece's destination that now lands under
     * A, so A's own rendering there is never touched and needs no repair. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    wuss_window_move(win_b, (point_t) { visible_b_before.x0 - 15,
                                        visible_b_before.y0 - 15 });

    /* The blit must have actually happened, not fallen back: B's own
     * footprint (outside A) must not be dirtied wholesale. */
    {
      box_t  visible_b_after, whole_footprint, dirty_area_box;
      int    dirty_area, footprint_area;

      wuss_window_get_visible_bounds(win_b, &visible_b_after);
      box_union(&visible_b_before, &visible_b_after, &whole_footprint);

      dirty_area = 0;
      for (i = 0; i < wuss_get_dirty_count(wuss); i++)
      {
        wuss_get_dirty(wuss, i, &region);
        if (!box_intersection(&region, &whole_footprint, &dirty_area_box))
          dirty_area += (dirty_area_box.x1 - dirty_area_box.x0) *
                        (dirty_area_box.y1 - dirty_area_box.y0);
      }
      footprint_area = (whole_footprint.x1 - whole_footprint.x0) *
                       (whole_footprint.y1 - whole_footprint.y0);
      if (dirty_area >= footprint_area)
        goto Failure; /* fell back to a full redraw instead of blitting */
    }

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
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

  rc = wuss_mouse_click(wuss, (point_t) { 31, 11 }, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* C's titlebar, above its content, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_c)
    goto Failure;

  wuss_window_close(win_c);

  rc = wuss_mouse_move(wuss, (point_t) { 20, 20 }, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: wuss_task_stop sends wuss_EVENT_QUIT to each window's task\n");

  tc_a.stop_count = 0;
  tc_b.stop_count = 0;
  wuss_task_stop(win_a);
  wuss_task_stop(win_b);
  wuss_window_close(win_a);
  wuss_window_close(win_b);
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
