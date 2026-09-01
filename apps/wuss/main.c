/* wuss/main.c -- Wuss - interactive minimal window manager demo */

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
#include "wuss/menu.h"

#include <SDL3/SDL.h>

#include "tasks/ball.h"
#include "tasks/blank.h"
#include "tasks/chars.h"
#include "tasks/checker.h"
#include "tasks/curve.h"
#include "tasks/gradient.h"
#include "tasks/icons.h"
#include "tasks/image.h"
#include "tasks/lissajous.h"
#include "tasks/palette.h"
#include "tasks/porter-duff.h"
#include "tasks/sofa.h"
#include "tasks/swatches.h"
#include "tasks/text.h"

/* ----------------------------------------------------------------------- */

/* Screen pixel format for the interactive test: 1 = 32bpp pixelfmt_bgrx8888
 * (feeds SDL directly, no per-frame conversion); 0 = pixelfmt_p4 paletted
 * (exercises screen_copy_rect's nibble-packed blit path instead). */
#define WUSS_TEST_32BPP 0

/* the launcher's spawn callbacks take no arguments, so the pieces they need
 * are stashed here instead; run_wuss runs at most once per
 * process, so a file-scope struct is as good as a passed-around context */
static struct
{
  wuss_t         *wuss;
  const colour_t *palette;
  int             npalette;
  const char     *resources;
  bmfont_t       *daydream_font;
  bool            quit; /* set by the "Quit Wuss" task-menu entry */
}
g;

/* Each spawn allocates a fresh per-instance task block so a task may run in
 * several windows at once; the block is owned by its window and freed by the
 * task's wuss_EVENT_CLOSE handler. If create fails, or (for the font-less
 * chars task) returns OK without opening a window, the block is freed here --
 * otherwise it would leak. */

static result_t spawn_ball(void)
{
  ball_task_t *t = calloc(1, sizeof(*t));
  result_t     rc;
  if (t == NULL) return result_OOM;
  rc = ball_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_text(void)
{
  text_task_t *t = calloc(1, sizeof(*t));
  result_t     rc;
  if (t == NULL) return result_OOM;
  rc = text_create(g.wuss, g.daydream_font, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_blank(void)
{
  blank_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = blank_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_chars(void)
{
  chars_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = chars_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_palette(void)
{
  palette_task_t *t = calloc(1, sizeof(*t));
  result_t        rc;
  if (t == NULL) return result_OOM;
  rc = palette_create(g.wuss, g.palette, g.npalette, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_image(void)
{
  image_task_t *t;
  const char   *leafname;
  const char   *filename;
  char          buf[DPTLIB_MAXPATH];
  const char   *ninepatch;
  result_t      rc;

  t = calloc(1, sizeof(*t));
  if (t == NULL) return result_OOM;

  leafname  = path_join_leafname("jessica", "png");
  filename  = path_join_filename(g.resources, 3, "resources", "images", leafname);
  strcpy(buf, filename);
  ninepatch = path_join_filename(g.resources, 3, "resources", "wuss",
                                 path_join_leafname("9tile", "png"));

  rc = image_create(g.wuss, buf, ninepatch, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_checker(void)
{
  checker_task_t *t = calloc(1, sizeof(*t));
  result_t        rc;
  if (t == NULL) return result_OOM;
  rc = checker_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_curve(void)
{
  curve_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = curve_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_lissajous(void)
{
  lissajous_task_t *t = calloc(1, sizeof(*t));
  result_t          rc;
  if (t == NULL) return result_OOM;
  rc = lissajous_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_sofa(void)
{
  sofa_task_t *t = calloc(1, sizeof(*t));
  result_t     rc;
  if (t == NULL) return result_OOM;
  rc = sofa_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_gradient(void)
{
  gradient_task_t *t = calloc(1, sizeof(*t));
  result_t         rc;
  if (t == NULL) return result_OOM;
  rc = gradient_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_icons(void)
{
  icons_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = icons_create(g.wuss, g.daydream_font, g.resources, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_swatches(void)
{
  swatches_task_t *t = calloc(1, sizeof(*t));
  result_t         rc;
  if (t == NULL) return result_OOM;
  rc = swatches_create(g.wuss, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_porter_duff(void)
{
  porter_duff_task_t *t = calloc(1, sizeof(*t));
  result_t            rc;
  if (t == NULL) return result_OOM;
  rc = porter_duff_create(g.wuss, g.palette, g.daydream_font, g.resources, t);
  if (rc != result_OK || t->window == NULL) { free(t); return rc; }
  return result_OK;
}

/* A static demo menu tree for the pop-up helper: a submenu, a couple of
 * ticked rows and a standalone dashed rule row above the final entry.
 * wuss_menu_open never mutates it. */
static const wuss_menu_item_t g_menu_export_items[] =
{
  { "As PNG",  wuss_MENU_ITEM_NONE,   NULL },
  { "As JPEG", wuss_MENU_ITEM_NONE,   NULL },
  { "As GIF",  wuss_MENU_ITEM_DISABLED, NULL }
};

static const wuss_menu_t g_menu_export =
{
  "Export", g_menu_export_items, NELEMS(g_menu_export_items)
};

static const wuss_menu_item_t g_menu_items[] =
{
  { "Open",      wuss_MENU_ITEM_NONE,   NULL },
  { "Show grid", wuss_MENU_ITEM_TICKED, NULL },
  { "Wireframe", wuss_MENU_ITEM_TICKED, NULL },
  { "Export",    wuss_MENU_ITEM_NONE,   &g_menu_export },
  { "Quit",      wuss_MENU_ITEM_DASHED, NULL }
};

static const wuss_menu_t g_menu =
{
  "Display", g_menu_items, NELEMS(g_menu_items)
};

static void menu_selected(const wuss_menu_t *menu,
                          int                index,
                          wuss_button_t      button,
                          void              *ctx)
{
  NOT_USED(button);
  NOT_USED(ctx);
  printf("menu: picked \"%s\"\n",
         menu->items[index].text ? menu->items[index].text : "(sep)");
}

static result_t spawn_menu(void)
{
  return wuss_menu_open(g.wuss, &g_menu, wuss_get_pointer(g.wuss),
                        menu_selected, NULL, NULL);
}

/* Same menu shape built from a descriptor string, to exercise
 * wuss_menu_create_from_desc. The tree must outlive the open chain, so it is
 * kept here and rebuilt (previous one freed) on each open. Freed for good in
 * run_wuss's teardown. */
static wuss_menu_t *g_menu_desc;

static const wuss_menu_item_t g_menu_desc_export_items[] =
{
  { "As PNG",  wuss_MENU_ITEM_NONE,     NULL },
  { "As JPEG", wuss_MENU_ITEM_NONE,     NULL },
  { "As GIF",  wuss_MENU_ITEM_DISABLED, NULL }
};

static const wuss_menu_t g_menu_desc_export =
{
  "Export", g_menu_desc_export_items, NELEMS(g_menu_desc_export_items)
};

static result_t spawn_menu_desc(void)
{
  wuss_menu_t *m;
  result_t     rc;

  rc = wuss_menu_create_from_desc(&m,
         "Display, Open, !Show grid, !Wireframe, >Export, |Quit",
         &g_menu_desc_export);
  if (rc != result_OK)
    return rc;

  wuss_menu_destroy(g_menu_desc);
  g_menu_desc = m;

  return wuss_menu_open(g.wuss, g_menu_desc, wuss_get_pointer(g.wuss),
                        menu_selected, NULL, NULL);
}

/* The task launcher is a MENU-button pop-up over the backdrop rather than a
 * window of buttons. g_task_items and g_task_spawn run in lock-step: picking
 * item i calls g_task_spawn[i]. */
typedef result_t (*task_spawn_fn_t)(void);

static const wuss_menu_item_t g_task_items[] =
{
  { "Ball",        wuss_MENU_ITEM_NONE,   NULL },
  { "Text",        wuss_MENU_ITEM_NONE,   NULL },
  { "Blank",       wuss_MENU_ITEM_NONE,   NULL },
  { "Chars",       wuss_MENU_ITEM_NONE,   NULL },
  { "Palette",     wuss_MENU_ITEM_NONE,   NULL },
  { "Image",       wuss_MENU_ITEM_NONE,   NULL },
  { "Checker",     wuss_MENU_ITEM_NONE,   NULL },
  { "Curve",       wuss_MENU_ITEM_NONE,   NULL },
  { "Lissajous",   wuss_MENU_ITEM_NONE,   NULL },
  { "Sofa",        wuss_MENU_ITEM_NONE,   NULL },
  { "Gradient",    wuss_MENU_ITEM_NONE,   NULL },
  { "Icons",       wuss_MENU_ITEM_NONE,   NULL },
  { "Swatches",    wuss_MENU_ITEM_NONE,   NULL },
  { "Porter-Duff", wuss_MENU_ITEM_NONE,   NULL },
  { "Menu",        wuss_MENU_ITEM_DASHED, NULL },
  { "Menu (desc)", wuss_MENU_ITEM_NONE,   NULL },
  { "Quit Wuss",   wuss_MENU_ITEM_DASHED, NULL }
};

static result_t spawn_quit(void)
{
  g.quit = true;
  return result_OK;
}

static const task_spawn_fn_t g_task_spawn[] =
{
  spawn_ball, spawn_text, spawn_blank, spawn_chars, spawn_palette,
  spawn_image, spawn_checker, spawn_curve, spawn_lissajous, spawn_sofa,
  spawn_gradient, spawn_icons, spawn_swatches, spawn_porter_duff, spawn_menu,
  spawn_menu_desc, spawn_quit
};

static const wuss_menu_t g_task_menu =
{
  "Tasks", g_task_items, NELEMS(g_task_items)
};

static void task_menu_selected(const wuss_menu_t *menu,
                               int                index,
                               wuss_button_t      button,
                               void              *ctx)
{
  NOT_USED(menu);
  NOT_USED(button);
  NOT_USED(ctx);
  if (index >= 0 && index < (int) NELEMS(g_task_spawn))
    (void) g_task_spawn[index]();
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
static result_t run_wuss(const char *resources)
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
  SDL_Window      *window;
  SDL_Renderer    *renderer;
  SDL_Texture     *texture;
  bool             quit;
  bool             garbage_pending;
  bool             pixel_stress_pending;
  int              i;
  bool             use_wimp16;

  {
    /* "riscos16" selects the RISC OS 16-colour palette; default is PICO-8 */
    const char *palette_name = getenv("WUSS_PALETTE");

    use_wimp16 = (palette_name != NULL && strcmp(palette_name, "wimp16") == 0);
    if (use_wimp16)
      define_wimp16_palette(palette);
    else
      define_pico8_palette(palette);
  }

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
  rc = bitmap_init(&bm, SIZE2D(scr_width, scr_height), pixelfmt_bgrx8888, rowbytes, palette, pixels);
#else
  rc = bitmap_init(&bm, SIZE2D(scr_width, scr_height), pixelfmt_p4, rowbytes, palette, pixels);
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

  window = SDL_CreateWindow("Wuss", scr_width, scr_height, 0);
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

    if (!use_wimp16) {
      config.titlebar_height           = 0;
      config.furniture.title.bg        = palette_PICO8_DARK_BLUE;
      config.furniture.title.fg        = palette_PICO8_WHITE;
      config.furniture.back            = palette_PICO8_GREEN;
      config.furniture.close           = palette_PICO8_RED;
      config.furniture.toggle          = palette_PICO8_ORANGE;
      config.furniture.resize          = palette_PICO8_LAVENDER;
      config.furniture.scroll.arrows   = palette_PICO8_BLUE;
      config.furniture.scroll.wells    = palette_PICO8_DARK_BLUE;
      config.furniture.scroll.sausages = palette_PICO8_LIGHT_GREY;
      config.bevel.light               = palette_PICO8_WHITE;
      config.bevel.dark                = palette_PICO8_DARK_GREY;
      config.accent.bg                 = palette_PICO8_DARK_BLUE;
      config.accent.fg                 = palette_PICO8_WHITE;
      config.backdrop.colour           = palette_PICO8_WHITE;
      config.backdrop.pattern          = screen_PATTERN_DOTS;
      config.backdrop.pattern_bg       = palette_PICO8_LIGHT_GREY;
    } else {
      config.titlebar_height           = 0;
      config.furniture.title.bg        = palette_WIMP16_GREY_75;
      config.furniture.title.fg        = palette_WIMP16_BLACK;
      config.furniture.back            = palette_WIMP16_GREEN;
      config.furniture.close           = palette_WIMP16_RED;
      config.furniture.toggle          = palette_WIMP16_ORANGE;
      config.furniture.resize          = palette_WIMP16_LIGHT_BLUE;
      config.furniture.scroll.arrows   = palette_WIMP16_GREY_50;
      config.furniture.scroll.wells    = palette_WIMP16_GREY_62;
      config.furniture.scroll.sausages = palette_WIMP16_GREY_87;
      config.bevel.light               = palette_WIMP16_WHITE;
      config.bevel.dark                = palette_WIMP16_GREY_50;
      config.accent.bg                 = palette_WIMP16_ORANGE;
      config.accent.fg                 = palette_WIMP16_BLACK;
      config.backdrop.colour           = palette_WIMP16_GREY_50;
      config.backdrop.pattern          = screen_PATTERN_DOTS;
      config.backdrop.pattern_bg       = palette_WIMP16_GREY_37;
    }

    rc = wuss_create(&scr, font, palette, NELEMS(palette), &config, NULL,
                     &wuss);
    if (rc != result_OK)
      goto Failure;
  }

  g.wuss          = wuss;
  g.palette       = palette;
  g.npalette      = NELEMS(palette);
  g.resources     = resources;
  g.daydream_font = daydream_font;

  quit                 = false;
  g.quit               = false;
  garbage_pending      = false;
  pixel_stress_pending = false;

  wuss_redraw(wuss);

  while (!quit && !g.quit)
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
        else if (event.key.key == SDLK_F1 && (event.key.mod & SDL_KMOD_SHIFT))
          wuss_redraw(wuss);
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
          int            x, y;
          wuss_button_t  button;
          wuss_window_t *hit;

          sdl_pos_to_scr(window, scr_width, scr_height, event.button.x, event.button.y, &x, &y);
          button = sdl_button_to_wuss(event.button.button);
          wuss_mouse_click(wuss, POINT(x, y), button, wuss_MOUSE_DOWN, &hit);

          /* MENU click on bare backdrop opens the task launcher there */
          if (hit == NULL && (button & wuss_BUTTON_MENU))
            wuss_menu_open(wuss, &g_task_menu, POINT(x, y),
                           task_menu_selected, NULL, NULL);
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.button.x, event.button.y, &x, &y);
          wuss_mouse_click(wuss, POINT(x, y), sdl_button_to_wuss(event.button.button), wuss_MOUSE_UP, NULL);
        }
        break;

      case SDL_EVENT_MOUSE_MOTION:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.motion.x, event.motion.y, &x, &y);
          wuss_mouse_move(wuss, POINT(x, y), NULL);
        }
        break;

      case SDL_EVENT_MOUSE_WHEEL:
        {
          int x, y;

          sdl_pos_to_scr(window, scr_width, scr_height, event.wheel.mouse_x, event.wheel.mouse_y, &x, &y);
          wuss_scroll(wuss, POINT(x, y), (int) event.wheel.y, NULL);
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

  /* ponytail: wuss_destroy() below frees every still-open window but not the
   * per-instance task block hung off it, so any task window left open at quit
   * leaks its block. Harmless at process exit; add a wuss close callback if a
   * task ever needs deterministic teardown. */
  wuss_menu_destroy(g_menu_desc);

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

  printf("run_wuss: failed (rc=0x%X)\n", rc);

  return result_TEST_FAILED;
}

/* ----------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
  const char *resources = ".";
  int         i;
  result_t    rc;

  for (i = 1; i < argc; i++)
    if (strcmp(argv[i], "-resources") == 0 && i + 1 < argc)
      resources = argv[++i];

  rc = run_wuss(resources);

  return rc == result_TEST_PASSED ? EXIT_SUCCESS : EXIT_FAILURE;
}
