/* wuss/main.c -- Wuss - interactive minimal window manager demo */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RISC OS's GCCSDK newlib has getopt() but not the GNU getopt_long()
 * extension, so parse_args() keeps a hand-rolled fallback there. */
#ifndef __riscos
#include <getopt.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
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
#include "wuss/task.h"
#include "wuss/wuss.h"

#include "frontend.h"
#include "tasks.h"

#include "tasks/palette.h" /* palette_load_hex for the startup *.hex */

/* ----------------------------------------------------------------------- */

/* Fill config with the chrome colours for the PICO-8 (default) or RISC OS
 * 16-colour Wimp palette. */
static void fill_chrome_config(wuss_config_t *config, int use_wimp16)
{
  config->titlebar_height = 0;
  config->backdrop.pattern = screen_PATTERN_DOTS;

  if (use_wimp16)
  {
    config->furniture.title.bg        = palette_WIMP16_GREY_75;
    config->furniture.title.fg        = palette_WIMP16_BLACK;
    config->furniture.outline         = palette_WIMP16_BLACK;
    config->furniture.back            = palette_WIMP16_GREEN;
    config->furniture.close           = palette_WIMP16_RED;
    config->furniture.toggle          = palette_WIMP16_ORANGE;
    config->furniture.resize          = palette_WIMP16_LIGHT_BLUE;
    config->furniture.scroll.arrows   = palette_WIMP16_GREY_50;
    config->furniture.scroll.wells    = palette_WIMP16_GREY_62;
    config->furniture.scroll.sausages = palette_WIMP16_GREY_87;
    config->bevel.light               = palette_WIMP16_WHITE;
    config->bevel.dark                = palette_WIMP16_GREY_50;
    config->bevel.divider             = palette_WIMP16_GREY_75;
    config->button.bg                 = palette_WIMP16_GREY_87;
    config->button.fg                 = palette_WIMP16_BLACK;
    config->button.pressed            = palette_WIMP16_GREY_62;
    config->accent.colour             = palette_WIMP16_ORANGE;
    config->backdrop.colour           = palette_WIMP16_GREY_37;
    config->backdrop.pattern_bg       = palette_WIMP16_GREY_50;
    config->body.window               = palette_WIMP16_GREY_87;
    config->body.menu                 = palette_WIMP16_WHITE;
  }
  else
  {
    config->furniture.title.bg        = palette_PICO8_DARK_BLUE;
    config->furniture.title.fg        = palette_PICO8_WHITE;
    config->furniture.outline         = palette_PICO8_BLACK;
    config->furniture.back            = palette_PICO8_GREEN;
    config->furniture.close           = palette_PICO8_RED;
    config->furniture.toggle          = palette_PICO8_ORANGE;
    config->furniture.resize          = palette_PICO8_LAVENDER;
    config->furniture.scroll.arrows   = palette_PICO8_BLUE;
    config->furniture.scroll.wells    = palette_PICO8_DARK_BLUE;
    config->furniture.scroll.sausages = palette_PICO8_LIGHT_GREY;
    config->bevel.light               = palette_PICO8_WHITE;
    config->bevel.dark                = palette_PICO8_DARK_GREY;
    config->bevel.divider             = palette_PICO8_LAVENDER;
    config->button.bg                 = palette_PICO8_LIGHT_GREY;
    config->button.fg                 = palette_PICO8_BLACK;
    config->button.pressed            = palette_PICO8_LAVENDER;
    config->accent.colour             = palette_PICO8_ORANGE;
    config->backdrop.colour           = palette_PICO8_LAVENDER;
    config->backdrop.pattern_bg       = palette_PICO8_LIGHT_GREY;
    config->body.window               = palette_PICO8_LIGHT_GREY;
    config->body.menu                 = palette_PICO8_WHITE;
  }
}

/* Redraw the whole screen one pixel at a time: each wuss_redraw_dirty call is
 * flushed before the next pixel is invalidated, so no two pixels are ever
 * coalesced into one redraw. A task whose drawing routine assumes it always
 * gets a multi-pixel/aligned clip (rather than trusting scr->clip) will
 * visibly misdraw here even though it looks fine under larger dirty regions. */
static void pixel_stress(wuss_t *wuss, int scr_width, int scr_height)
{
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
}

/* one iteration of the event/redraw loop; a struct because Emscripten drives
 * it as a callback (emscripten_set_main_loop_arg) rather than a plain while */
struct wuss_frame_ctx
{
  wuss_t          *wuss;
  wuss_frontend_t *frontend;
  bitmap_t        *bm;
  unsigned char   *pixels;
  int              rowbytes;
  int              scr_width;
  int              scr_height;
  colour_t        *palette;
  int              npalette;
};

static void wuss_frame(void *arg)
{
  struct wuss_frame_ctx *c = arg;
  wuss_input_t           ev;
  bool                   pixel_stress_pending = false;
  bool                   garbage_pending      = false;

  while (wuss_frontend_poll(c->frontend, &ev))
  {
    switch (ev.kind)
    {
    case wuss_INPUT_QUIT:
      g.quit = true;
      break;

    case wuss_INPUT_REDRAW_ALL:
      wuss_redraw(c->wuss);
      break;

    case wuss_INPUT_GARBAGE:
      garbage_pending = true;
      break;

    case wuss_INPUT_PIXEL_STRESS:
      pixel_stress_pending = true;
      break;

    case wuss_INPUT_MOUSE_DOWN:
      {
        wuss_window_t *hit;

        wuss_mouse_click(c->wuss, ev.pos, ev.button, wuss_MOUSE_DOWN, &hit);

        /* MENU click on bare backdrop opens the task launcher there */
        if (hit == NULL && (ev.button & wuss_BUTTON_MENU))
          tasks_open_launcher(ev.pos);
      }
      break;

    case wuss_INPUT_MOUSE_UP:
      wuss_mouse_click(c->wuss, ev.pos, ev.button, wuss_MOUSE_UP, NULL);
      break;

    case wuss_INPUT_MOUSE_MOVE:
      wuss_mouse_move(c->wuss, ev.pos, NULL);
      break;

    case wuss_INPUT_WHEEL:
      wuss_scroll(c->wuss, ev.pos, ev.wheel, NULL);
      break;

    default:
      break;
    }
  }

  wuss_idle(c->wuss);

  if (garbage_pending)
  {
    /* corrupt the whole framebuffer and present it, then leave it alone --
     * wuss only repaints what it knows is dirty, so the junk stays put
     * until something else invalidates the screen */
    unsigned char *p;
    size_t         n;
    size_t         i;

    p = c->pixels;
    n = (size_t) c->rowbytes * c->scr_height;
    for (i = 0; i < n; i++)
      p[i] = (unsigned char) rand();

    wuss_frontend_present(c->frontend, c->bm);
  }
  else
  {
    if (pixel_stress_pending)
      pixel_stress(c->wuss, c->scr_width, c->scr_height);
    else
      wuss_redraw_dirty(c->wuss);

    wuss_frontend_present(c->frontend, c->bm);
  }

#ifdef __EMSCRIPTEN__
  if (g.quit)
    emscripten_cancel_main_loop();
#endif
}

/* fonts[]/descs[] below hold [0] regular (system font), [1] bold, [2] symbol
 * (menu tick / submenu arrow); kept <= wuss_MAX_FONTS so wuss_create's own
 * nfonts check is never reached with arrays it can no longer fit in */
#define WUSS_MAIN_NFONTS 3
#if WUSS_MAIN_NFONTS > wuss_MAX_FONTS
#error WUSS_MAIN_NFONTS exceeds wuss_MAX_FONTS
#endif

/* click windows to bring to front, drag titlebars to move; the redraw-all
 * input redraws the whole screen, the pixel-stress input does it one pixel at
 * a time to catch tasks that misbehave under a 1x1 clip; the palette task's
 * picker menu swaps the system palette live (wuss_set_palette, picked up by
 * menu_handle's wuss_EVENT_PALETTE case); the quit input or closing the
 * window exits */
static result_t run_wuss(const char *resources,
                         const char *palette_name,
                         int         depth,
                         int         scale)
{
  const int        scr_width  = 640;
  const int        scr_height = 480;

  result_t           rc;
  const char        *leafname;
  const char        *filename;
  bmfont_t          *fonts[WUSS_MAIN_NFONTS];
  static const char *const names[WUSS_MAIN_NFONTS] =
    { "DPT-Digits-Regular", "DPT-Digits-Bold", "Symbols" };
  int                nfonts;
  int                i;
  void              *pixels;
  int                rowbytes;
  pixelfmt_t         fmt;
  bitmap_t           bm;
  screen_t           scr;
  colour_t           palette[16]; /* the fixed-size UI palette */
  colour_t           scr_palette[256]; /* palette[] padded out to whatever
                                        * count the chosen depth's bitmap
                                        * needs (p8 reads all 256) */
  wuss_t            *wuss;
  wuss_frontend_t   *frontend;
  bool               use_wimp16;
  int                palette_index;

  {
    /* palette_name (from -palette, default "PICO-8") names a *.hex file under
     * resources/palettes, extension stripped, e.g. "RISC-OS". Chrome was
     * never derived from *.hex content (see fill_chrome_config), so it stays
     * keyed by whether the startup file is "RISC-OS" specifically, not by
     * whatever the picker menu later loads. */
    use_wimp16 = (strcmp(palette_name, "RISC-OS") == 0);
    palette_index = use_wimp16 ? 1 : 0;

    rc = palette_load_hex(resources, palette_name, palette);
    if (rc != result_OK)
    {
      logf_error("wuss: palette_load_hex(\"%s\") failed, rc=0x%X (%s)",
                palette_name, rc, result_string(rc));
      goto Failure;
    }

    g.palette_name = palette_name;
  }

  logf_info("wuss: resources root = \"%s\"", resources);

  {
    nfonts = 0;
    for (i = 0; i < WUSS_MAIN_NFONTS; i++)
    {
      leafname = path_join_leafname(names[i], "png");
      filename = path_join_filename(resources, 3, "resources", "bmfonts",
                                    leafname);
      logf_info("wuss: loading font \"%s\"", filename);
      rc = bmfont_create(filename, &fonts[i]);
      if (rc != result_OK)
      {
        logf_error("wuss: bmfont_create(\"%s\") failed, rc=0x%X (%s)", filename,
                   rc, result_string(rc));
        goto Failure;
      }
      nfonts++;
    }
  }

  rc = wuss_frontend_open(scr_width, scr_height, palette, NELEMS(palette),
                          depth, scale, &pixels, &rowbytes, &fmt, &frontend);
  logf_info("wuss: wuss_frontend_open -> rc=0x%X (%s)", rc, result_string(rc));
  if (rc != result_OK)
    goto Failure;

  /* bitmap_set_palette reads every entry a paletted format's bitmap_init
   * needs (up to 256 for p8); pad scr_palette with palette's 16 UI colours
   * so closest_palette_entry always has exact matches to find, then the
   * web-safe 216 (6x6x6 cube, steps of 0x33) so a p8 screen has real range
   * beyond the UI colours for nearest-match, then black for what's left. */
  memset(scr_palette, 0, sizeof(scr_palette));
  memcpy(scr_palette, palette, sizeof(palette));
  {
    int r, g, b, n;

    n = NELEMS(palette);
    for (r = 0; r < 6; r++)
      for (g = 0; g < 6; g++)
        for (b = 0; b < 6; b++)
          scr_palette[n++] = colour_rgb(r * 0x33, g * 0x33, b * 0x33);
  }

  rc = bitmap_init(&bm, SIZE2D(scr_width, scr_height), fmt, rowbytes,
                   scr_palette, pixels);
  logf_info("wuss: bitmap_init -> rc=0x%X (%s)", rc, result_string(rc));
  if (rc != result_OK)
    goto Failure;

  bitmap_clear(&bm, palette[palette_PICO8_WHITE]);

  screen_for_bitmap(&scr, &bm);

  {
    wuss_config_t    config;
    wuss_font_desc_t descs[WUSS_MAIN_NFONTS]; /* slot classes/names for the
                                * picker menus -- [2] Symbols is chrome-only,
                                * never a text font choice */

    fill_chrome_config(&config, use_wimp16);

    for (i = 0; i < nfonts; i++)
    {
      descs[i].font       = fonts[i];
      descs[i].font_class = (i == 2) ? wuss_FONT_CLASS_SYSTEM
                                     : wuss_FONT_CLASS_NONE;
      descs[i].name       = names[i];
    }

    rc = wuss_create(&scr, descs, nfonts, palette, NELEMS(palette), &config,
                     NULL, &wuss);
    logf_info("wuss: wuss_create -> rc=0x%X (%s)", rc, result_string(rc));
    if (rc != result_OK)
      goto Failure;
  }

  g.wuss           = wuss;
  g.palette        = palette;
  g.npalette       = NELEMS(palette);
  g.resources      = resources;
  g.daydream_font  = fonts[0]; /* tasks draw with the regular weight */
  g.bold_font      = fonts[1];
  g.frontend       = frontend;
  g.bm             = &bm;

  {
    wuss_task_desc_t desc;

    desc.handle    = task_handle_event;
    desc.task_data = NULL;
    desc.name      = "menu";
    rc = wuss_task_create(wuss, &desc, &g.menu_task);
    logf_info("wuss: wuss_task_create(menu) -> rc=0x%X (%s)", rc,
              result_string(rc));
    if (rc != result_OK)
      goto Failure;
  }

  g.quit = false;

  wuss_redraw(wuss);

  {
    struct wuss_frame_ctx ctx;

    ctx.wuss          = wuss;
    ctx.frontend      = frontend;
    ctx.bm            = &bm;
    ctx.pixels        = pixels;
    ctx.rowbytes      = rowbytes;
    ctx.scr_width     = scr_width;
    ctx.scr_height    = scr_height;
    ctx.palette       = palette;
    ctx.npalette      = NELEMS(palette);

#ifdef __EMSCRIPTEN__
    /* the browser owns the loop; simulate_infinite_loop=1 means this call
     * never returns, so &ctx (a stack local) stays live and the teardown
     * below is unreachable -- fine, the page dies on navigation. fps=0 asks
     * for requestAnimationFrame pacing. */
    emscripten_set_main_loop_arg(wuss_frame, &ctx, 0, 1);
#else
    while (!g.quit)
      wuss_frame(&ctx);
#endif
  }

  /* ponytail: wuss_destroy() below force-closes every still-open window and
   * frees every registered task node, but not the per-instance task_data
   * block a spawn_* calloc'd, so any task window left open at quit leaks that
   * block. Harmless at process exit. */
  wuss_destroy(wuss); /* also sweeps g.menu_task and closes any open chain */

  for (i = 0; i < nfonts; i++)
    bmfont_destroy(fonts[i]);

  wuss_frontend_close(frontend);

  return result_TEST_PASSED;


Failure:

  printf("run_wuss: failed (rc=0x%X: %s)\n", rc, result_string(rc));

  return result_TEST_FAILED;
}

/* ----------------------------------------------------------------------- */

/* Parsed command-line options. Members are use-ordered to match run_wuss's
 * parameter list. */
typedef struct wuss_options
{
  const char *resources;    /* -r/--resources: fixture root */
  const char *palette_name; /* -p/--palette: startup *.hex leafname */
  int         depth;        /* -d/--depth: framebuffer bpp (1, 2, 4, 8 or 32) */
  int         scale;        /* -s/--scale: initial window zoom, 0 = default */
}
wuss_options_t;

static const char wuss_usage[] =
  "usage: wuss [-r|--resources DIR] [-p|--palette NAME] "
  "[-d|--depth 1|2|4|8|32] [-s|--scale N]\n";

#ifndef __riscos

/* Desktop: getopt_long. Accepts the short forms and the "--" long forms; the
 * historical single-dash long spellings (-resources) are no longer accepted.
 * Returns false and prints usage on an unknown option or missing argument. */
static bool parse_args(int argc, char *argv[], wuss_options_t *opts)
{
  static const struct option longopts[] =
  {
    { "resources", required_argument, NULL, 'r' },
    { "palette",   required_argument, NULL, 'p' },
    { "depth",     required_argument, NULL, 'd' },
    { "scale",     required_argument, NULL, 's' },
    { NULL,        0,                 NULL, 0   }
  };

  int c;

  for (;;)
  {
    c = getopt_long(argc, argv, "r:p:d:s:", longopts, NULL);
    if (c == -1)
      break;

    switch (c)
    {
    case 'r': opts->resources    = optarg;       break;
    case 'p': opts->palette_name = optarg;       break;
    case 'd': opts->depth        = atoi(optarg); break;
    case 's': opts->scale        = atoi(optarg); break;
    default:
      fputs(wuss_usage, stderr);
      return false;
    }
  }

  return true;
}

#else /* __riscos */

/* RISC OS: no getopt_long. Hand-rolled scan of the same options, single-dash
 * long spellings only (matches the pre-getopt behaviour). */
static bool parse_args(int argc, char *argv[], wuss_options_t *opts)
{
  int i;

  for (i = 1; i < argc; i++)
    if (strcmp(argv[i], "-resources") == 0 && i + 1 < argc)
      opts->resources = argv[++i];
    else if (strcmp(argv[i], "-palette") == 0 && i + 1 < argc)
      opts->palette_name = argv[++i];
    else if (strcmp(argv[i], "-depth") == 0 && i + 1 < argc)
      opts->depth = atoi(argv[++i]);
    else if (strcmp(argv[i], "-scale") == 0 && i + 1 < argc)
      opts->scale = atoi(argv[++i]);

  return true;
}

#endif /* __riscos */

int main(int argc, char *argv[])
{
  /* path_join_filename splices the root and each branch with the platform
   * separator, so the "here" root differs: "." on Unix, but on RISC OS the
   * currently-selected directory is "@" ("." there would give "..resources"). */
#ifdef __riscos
  const char *default_resources = "@";
#else
  const char *default_resources = ".";
#endif
  wuss_options_t opts;
  result_t       rc;

  opts.resources    = default_resources;
  opts.palette_name = "PICO-8";
  opts.depth        = 4;
  opts.scale        = 0; /* 0 = let the frontend pick its default */

  if (!parse_args(argc, argv, &opts))
    return EXIT_FAILURE;

  rc = run_wuss(opts.resources, opts.palette_name, opts.depth, opts.scale);

  return rc == result_TEST_PASSED ? EXIT_SUCCESS : EXIT_FAILURE;
}
