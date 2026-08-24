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

/* window B's client: flows a fixed paragraph of placeholder text over its
 * wuss-filled background, one line per bmfont_draw call */
typedef struct text_client
{
  bmfont_t *font;
  colour_t  bg, fg;
}
text_client_t;

static result_t text_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  static const char *const lines[] =
  {
    "Lorem ipsum dolor sit amet,",
    "consectetur adipiscing elit,",
    "sed do eiusmod tempor",
    "incididunt ut labore et",
    "dolore magna aliqua."
  };

  text_client_t *tcx;
  int            i, font_width, font_height;
  point_t        pos;

  NOT_USED(window);

  tcx = client_data;

  bmfont_get_info(tcx->font, &font_width, &font_height);

  pos.x = content->x0 + 4;
  pos.y = content->y0 + 4;
  for (i = 0; i < NELEMS(lines); i++)
  {
    bmfont_draw(tcx->font, scr, lines[i], (int) strlen(lines[i]), tcx->fg, tcx->bg, &pos, NULL);
    pos.y += font_height + 2;
  }

  return result_OK;
}

/* window A's client: a ball that bounces off the content box's edges, so
 * the redraw loop has something moving to repaint every frame */
typedef struct ball_client
{
  colour_t bg, ball;
  int      x, y;   /* centre, local content coords */
  int      dx, dy;
  int      radius;
}
ball_client_t;

static result_t ball_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  ball_client_t *bc;

  NOT_USED(window);

  bc = client_data;

  screen_draw_rect(scr, content->x0, content->y0,
                    content->x1 - content->x0,
                    content->y1 - content->y0,
                    bc->bg);

  screen_draw_rect(scr, content->x0 + bc->x - bc->radius, content->y0 + bc->y - bc->radius,
                    bc->radius * 2, bc->radius * 2, bc->ball);

  return result_OK;
}

/* move the ball on and invalidate the union of its old and new positions;
 * called once per frame from the main loop, not from a wuss callback */
static void ball_step(wuss_window_t *window, ball_client_t *bc)
{
  box_t content, local;
  int   width, height;
  int   old_x, old_y;

  wuss_window_get_content_bounds(window, &content);
  width  = content.x1 - content.x0;
  height = content.y1 - content.y0;

  old_x = bc->x;
  old_y = bc->y;

  bc->x += bc->dx;
  bc->y += bc->dy;

  if (bc->x - bc->radius < 0)           { bc->x = bc->radius;          bc->dx = -bc->dx; }
  else if (bc->x + bc->radius > width)  { bc->x = width - bc->radius;  bc->dx = -bc->dx; }
  if (bc->y - bc->radius < 0)           { bc->y = bc->radius;          bc->dy = -bc->dy; }
  else if (bc->y + bc->radius > height) { bc->y = height - bc->radius; bc->dy = -bc->dy; }

  local.x0 = MIN(old_x, bc->x) - bc->radius;
  local.y0 = MIN(old_y, bc->y) - bc->radius;
  local.x1 = MAX(old_x, bc->x) + bc->radius;
  local.y1 = MAX(old_y, bc->y) + bc->radius;

  wuss_window_invalidate(window, &local);
}

static wuss_button_t sdl_button_to_wuss(Uint8 button)
{
  switch (button)
  {
  case SDL_BUTTON_MIDDLE: return wuss_BUTTON_MENU;
  case SDL_BUTTON_RIGHT:  return wuss_BUTTON_ADJUST;
  default:                return wuss_BUTTON_SELECT;
  }
}

/* click windows to bring to front, drag titlebars to move, resize the
 * SDL window to see the wuss screen scale; Q or close to quit */
static result_t wuss_interactive_test(const char *resources)
{
  const int   scr_width  = 640;
  const int   scr_height = 480;
  const int   rowbytes   = scr_width * 4;

  result_t              rc;
  const char            *leafname;
  const char            *filename;
  bmfont_t              *font;
  void                  *pixels;
  bitmap_t              bm;
  screen_t              scr;
  colour_t              palette[16];
  wuss_t                *wuss;
  ball_client_t          ic_a;
  text_client_t          ic_b;
  wuss_client_t          client_a, client_b, client_c;
  box_t                  box_a, box_b, box_c;
  wuss_window_t         *win_a, *win_b, *win_c;
  SDL_Window            *window;
  SDL_Renderer          *renderer;
  SDL_Texture           *texture;
  bool                   quit;
  bool                   garbage_pending;

  define_pico8_palette(palette);

  leafname = path_join_leafname("ms-sans-serif", "png");
  filename = path_join_filename(resources, 3, "resources", "bmfonts", leafname);
  rc = bmfont_create(filename, &font);
  if (rc != result_OK)
    goto Failure;

  pixels = malloc(rowbytes * scr_height);
  if (pixels == NULL)
    goto Failure;

  rc = bitmap_init(&bm, scr_width, scr_height, pixelfmt_bgrx8888, rowbytes, palette, pixels);
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
  client_a.redraw      = ball_redraw;
  client_a.mouse       = NULL;
  client_a.client_data = &ic_a;
  client_a.bg          = wuss_NO_BACKGROUND; /* ball_redraw paints its own background every frame */
  box_a.x0 = 20;  box_a.y0 = 20;
  box_a.x1 = 220; box_a.y1 = 180;
  rc = wuss_window_create(wuss, &box_a, "Bouncing Ball", &client_a, &win_a);
  if (rc != result_OK)
    goto Failure;

  ic_b.font   = font;
  ic_b.bg     = palette[palette_PICO8_BLUE]; /* matches client_b.bg, for bmfont_draw's glyph blending */
  ic_b.fg     = palette[palette_PICO8_WHITE];
  client_b.redraw      = text_redraw;
  client_b.mouse       = NULL;
  client_b.client_data = &ic_b;
  client_b.bg          = palette_PICO8_BLUE;
  box_b.x0 = 120; box_b.y0 = 100;
  box_b.x1 = 340; box_b.y1 = 280;
  rc = wuss_window_create(wuss, &box_b, "Lorem Ipsum", &client_b, &win_b);
  if (rc != result_OK)
    goto Failure;

  client_c.redraw      = NULL;
  client_c.mouse       = NULL;
  client_c.client_data = NULL;
  client_c.bg          = palette_PICO8_GREEN; /* wuss fills the content area itself */
  box_c.x0 = 260; box_c.y0 = 60;
  box_c.x1 = 460; box_c.y1 = 220;
  rc = wuss_window_create(wuss, &box_c, "Blankety Blank", &client_c, &win_c);
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
        break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        wuss_mouse_down(wuss, (int) event.button.x, (int) event.button.y,
                         sdl_button_to_wuss(event.button.button), NULL);
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        wuss_mouse_up(wuss, (int) event.button.x, (int) event.button.y,
                       sdl_button_to_wuss(event.button.button), NULL);
        break;

      case SDL_EVENT_MOUSE_MOTION:
        wuss_mouse_move(wuss, (int) event.motion.x, (int) event.motion.y, NULL);
        break;

      default:
        break;
      }
    }

    ball_step(win_a, &ic_a);

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
      box_t dirty;

      wuss_get_dirty(wuss, &dirty);
      if (!box_is_empty(&dirty))
      {
        scr.clip = dirty;
        screen_draw_rect(&scr, dirty.x0, dirty.y0,
                          dirty.x1 - dirty.x0, dirty.y1 - dirty.y0,
                          palette[palette_PICO8_WHITE]);
      }

      wuss_redraw_dirty(wuss);
    }

    SDL_UpdateTexture(texture, NULL, bm.base, bm.rowbytes);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_Delay(1000 / 60);
  }

  wuss_destroy(wuss);
  bmfont_destroy(font);

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  free(pixels);

  return result_TEST_PASSED;


Failure:

  printf("failed\n");

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
  test_client_t  tc_a, tc_b, tc_c;
  wuss_client_t  client_a, client_b, client_c;
  box_t          box_a, box_b, box_c;
  wuss_window_t *win_a, *win_b, *win_c;
  wuss_window_t *hit;
  box_t          visible;
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
  box_a.y1 = 10; /* not taller than titlebar_height (20) */
  rc = wuss_window_create(wuss, &box_a, "toosmall", NULL, &win_a);
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
  rc = wuss_window_create(wuss, &box_a, "A", &client_a, &win_a);
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
  rc = wuss_window_create(wuss, &box_b, "B", &client_b, &win_b);
  if (rc != result_OK)
    goto Failure;

  printf("test: redraw\n");

  rc = wuss_redraw(wuss);
  if (rc != result_OK)
    goto Failure;
  if (tc_a.redraw_count != 1 || tc_b.redraw_count != 1)
    goto Failure;

  printf("test: z-order hit test and local coordinate translation (B on top)\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 75, 75, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;
  if (tc_b.last_action != wuss_MOUSE_DOWN || tc_b.last_x != 25 || tc_b.last_y != 5)
    goto Failure;

  rc = wuss_mouse_up(wuss, 75, 75, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: click-to-front changes subsequent overlap hits\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 10, 10, wuss_BUTTON_SELECT, &hit); /* A's titlebar */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_up(wuss, 10, 10, wuss_BUTTON_SELECT, &hit);
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
  if (tc_a.mouse_count != 1 || tc_a.last_x != 75 || tc_a.last_y != 55)
    goto Failure;

  rc = wuss_mouse_up(wuss, 75, 75, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: titlebar click starts a drag, not delivered as content\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 10, 10, wuss_BUTTON_SELECT, &hit); /* A's titlebar, A already topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 0)
    goto Failure;

  printf("test: drag-move updates visible bounds and invalidates the affected region\n");

  before_a = tc_a.redraw_count;
  before_b = tc_b.redraw_count;
  rc = wuss_mouse_move(wuss, 30, 15, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.redraw_count != before_a || tc_b.redraw_count != before_b)
    goto Failure; /* invalidated, not yet redrawn */

  {
    box_t dirty;

    wuss_get_dirty(wuss, &dirty);
    if (box_is_empty(&dirty))
      goto Failure;
  }

  rc = wuss_redraw_dirty(wuss);
  if (rc != result_OK)
    goto Failure;
  if (tc_a.redraw_count != before_a + 1 || tc_b.redraw_count != before_b + 1)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 20 || visible.y0 != 5)
    goto Failure;

  printf("test: mouse-up ends the drag\n");

  rc = wuss_mouse_up(wuss, 30, 15, wuss_BUTTON_SELECT, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  printf("test: Adjust-drag moves a window without bringing it to front\n");

  rc = wuss_mouse_down(wuss, 140, 55, wuss_BUTTON_ADJUST, &hit); /* B's titlebar, clear of A */
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
  if (visible.x0 != 55 || visible.y0 != 55)
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
  if (visible.x0 != 20 || visible.y0 != 5)
    goto Failure;

  printf("test: window_resize valid and too-small cases\n");

  rc = wuss_window_resize(win_a, 50, 10); /* not taller than titlebar_height */
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  rc = wuss_window_resize(win_a, 50, 50);
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_visible_bounds(win_a, &visible);
  width  = visible.x1 - visible.x0;
  height = visible.y1 - visible.y0;
  if (width != 50 || height != 50)
    goto Failure;

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
  rc = wuss_window_create(wuss, &box_c, "C", &client_c, &win_c);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_down(wuss, 5, 5, wuss_BUTTON_SELECT, &hit); /* C's titlebar */
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

  printf("failed\n");

  return result_TEST_FAILED;
}
