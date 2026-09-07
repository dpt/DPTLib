/* wuss/main.c -- Wuss - interactive minimal window manager demo */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Furniture/bevel/accent/backdrop/body colour indices, one row per palette.
 * Same field order as the assignments in fill_chrome_config. */
static const wuss_colour_t g_chrome[2][18] =
{
  /* PICO-8 */
  {
    palette_PICO8_DARK_BLUE,
    palette_PICO8_WHITE,
    palette_PICO8_DARK_BLUE,
    palette_PICO8_GREEN,
    palette_PICO8_RED,
    palette_PICO8_ORANGE,
    palette_PICO8_LAVENDER,
    palette_PICO8_BLUE,
    palette_PICO8_DARK_BLUE,
    palette_PICO8_LIGHT_GREY,
    palette_PICO8_WHITE,
    palette_PICO8_DARK_GREY,
    palette_PICO8_ORANGE,
    palette_PICO8_WHITE,
    palette_PICO8_WHITE,
    palette_PICO8_LIGHT_GREY,
    palette_PICO8_LIGHT_GREY,
    palette_PICO8_WHITE
  },

  /* RISC OS 16-colour Wimp */
  {
    palette_WIMP16_GREY_75,
    palette_WIMP16_BLACK,
    palette_WIMP16_BLACK,
    palette_WIMP16_GREEN,
    palette_WIMP16_RED,
    palette_WIMP16_ORANGE,
    palette_WIMP16_LIGHT_BLUE,
    palette_WIMP16_GREY_50,
    palette_WIMP16_GREY_62,
    palette_WIMP16_GREY_87,
    palette_WIMP16_WHITE,
    palette_WIMP16_GREY_50,
    palette_WIMP16_ORANGE,
    palette_WIMP16_BLACK,
    palette_WIMP16_GREY_50,
    palette_WIMP16_GREY_37,
    palette_WIMP16_GREY_87,
    palette_WIMP16_WHITE
  }
};

static void fill_chrome_config(wuss_config_t *config, int palette_index)
{
  const wuss_colour_t *c = g_chrome[palette_index];

  config->titlebar_height           = 0;
  config->furniture.title.bg        = c[0];
  config->furniture.title.fg        = c[1];
  config->furniture.outline         = c[2];
  config->furniture.back            = c[3];
  config->furniture.close           = c[4];
  config->furniture.toggle          = c[5];
  config->furniture.resize          = c[6];
  config->furniture.scroll.arrows   = c[7];
  config->furniture.scroll.wells    = c[8];
  config->furniture.scroll.sausages = c[9];
  config->bevel.light               = c[10];
  config->bevel.dark                = c[11];
  config->accent.bg                 = c[12];
  config->accent.fg                 = c[13];
  config->backdrop.colour           = c[14];
  config->backdrop.pattern          = screen_PATTERN_DOTS;
  config->backdrop.pattern_bg       = c[15];
  config->body.window               = c[16];
  config->body.menu                 = c[17];
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
static result_t run_wuss(const char *resources)
{
  const int        scr_width  = 640;
  const int        scr_height = 480;

  result_t           rc;
  const char        *leafname;
  const char        *filename;
  bmfont_t          *fonts[WUSS_MAIN_NFONTS];
  static const char *const names[WUSS_MAIN_NFONTS] =
    { "Digits-Regular", "Digits-Bold", "Symbols" };
  int                nfonts;
  int                i;
  void              *pixels;
  int                rowbytes;
  pixelfmt_t         fmt;
  bitmap_t           bm;
  screen_t           scr;
  colour_t           palette[16];
  wuss_t            *wuss;
  wuss_frontend_t   *frontend;
  bool               use_wimp16;
  int                palette_index;

  {
    /* WUSS_PALETTE names a *.hex file under resources/palettes (extension
     * stripped, e.g. "RISC-OS"); default is PICO-8. Chrome was never derived
     * from *.hex content (see fill_chrome_config), so it stays keyed by
     * whether the startup file is "RISC-OS" specifically, not by whatever
     * the picker menu later loads. */
    const char *palette_name = getenv("WUSS_PALETTE");

    if (palette_name == NULL)
      palette_name = "PICO-8";
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
                          &pixels, &rowbytes, &fmt, &frontend);
  logf_info("wuss: wuss_frontend_open -> rc=0x%X (%s)", rc, result_string(rc));
  if (rc != result_OK)
    goto Failure;

  rc = bitmap_init(&bm, SIZE2D(scr_width, scr_height), fmt, rowbytes, palette,
                   pixels);
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

    fill_chrome_config(&config, use_wimp16 ? 1 : 0);

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

int main(int argc, char *argv[])
{
  /* path_join_filename splices the root and each branch with the platform
   * separator, so the "here" root differs: "." on Unix, but on RISC OS the
   * currently-selected directory is "@" ("." there would give "..resources"). */
#ifdef __riscos
  const char *resources = "@";
#else
  const char *resources = ".";
#endif
  int         i;
  result_t    rc;

  for (i = 1; i < argc; i++)
    if (strcmp(argv[i], "-resources") == 0 && i + 1 < argc)
      resources = argv[++i];

  rc = run_wuss(resources);

  return rc == result_TEST_PASSED ? EXIT_SUCCESS : EXIT_FAILURE;
}
