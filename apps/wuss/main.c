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

#include "tasks/config.h"  /* config_create at startup */
#include "tasks/palette.h" /* palette_load_hex for the startup *.hex */

/* ----------------------------------------------------------------------- */

/* run_wuss's framebuffer bitmap and the screen_t wrapping it for wuss:
 * file-scope, like g_tasks, since run_wuss runs at most once per process
 * and app_set_mode (called from the Display task, well after run_wuss's own
 * locals have gone out of scope) needs to reallocate and re-derive them. */
static bitmap_t g_bm;
static screen_t g_scr;

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
    config->furniture.title.focus_bg  = palette_WIMP16_CREAM;
    config->furniture.outline         = palette_WIMP16_BLACK;
    config->furniture.back            = palette_WIMP16_GREEN;
    config->furniture.close           = palette_WIMP16_RED;
    config->furniture.toggle          = palette_WIMP16_ORANGE;
    config->furniture.resize          = palette_WIMP16_LIGHT_BLUE;
    config->furniture.scroll.arrows   = palette_WIMP16_GREY_50;
    config->furniture.scroll.wells    = palette_WIMP16_GREY_62;
    config->furniture.scroll.sausages = palette_WIMP16_GREY_87;
    config->furniture.pressed         = palette_WIMP16_ORANGE;
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
    config->slider.track              = palette_WIMP16_WHITE;
    config->slider.value              = palette_WIMP16_GREY_50;
    config->slider.surround           = wuss_NO_BACKGROUND;
  }
  else
  {
    config->furniture.title.bg        = palette_PICO8_DARK_BLUE;
    config->furniture.title.fg        = palette_PICO8_WHITE;
    config->furniture.title.focus_bg  = palette_PICO8_BLUE;
    config->furniture.outline         = palette_PICO8_BLACK;
    config->furniture.back            = palette_PICO8_GREEN;
    config->furniture.close           = palette_PICO8_RED;
    config->furniture.toggle          = palette_PICO8_ORANGE;
    config->furniture.resize          = palette_PICO8_LAVENDER;
    config->furniture.scroll.arrows   = palette_PICO8_BLUE;
    config->furniture.scroll.wells    = palette_PICO8_DARK_BLUE;
    config->furniture.scroll.sausages = palette_PICO8_LIGHT_GREY;
    config->furniture.pressed         = palette_PICO8_ORANGE;
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
    config->slider.track              = palette_PICO8_WHITE;
    config->slider.value              = palette_PICO8_DARK_GREY;
    config->slider.surround           = wuss_NO_BACKGROUND;
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

/* ----------------------------------------------------------------------- */

/* The optional software pointer (--pointer, Debug > Software pointer): an
 * arrow drawn into the framebuffer itself, so it scales with the window zoom
 * and goes through the CRT shader like everything else. wuss never learns of
 * it: each frame puts back the pixels under it before wuss can draw (a window
 * drag's screen_copy_rect would otherwise smear it along), then saves them
 * afresh and redraws it once wuss is done. */
static struct
{
  bitmap_t       image;     /* the arrow; hotspot at its top-left */
  bool           loaded;
  unsigned char *under;     /* framebuffer bytes beneath the drawn arrow */
  box_t          drawn;     /* where it was drawn, if is_drawn */
  bool           is_drawn;
  point_t        pos;       /* last reported mouse position */
  bool           in_window; /* false until a mouse event, or after a leave */
}
g_pointer;

/* Copy the framebuffer bytes spanning box to (save) or from g_pointer.under.
 * Whole bytes, so a sub-byte format needs no masking: the stray pixels
 * either side go back exactly as they were saved. */
static void pointer_copy_under(const box_t *box, bool save)
{
  int            log2bpp;
  int            bx0, nbytes;
  unsigned char *under;
  int            y;
  unsigned char *row;

  log2bpp = pixelfmt_log2bpp(g_bm.format);
  bx0     = (box->x0 << log2bpp) >> 3;
  nbytes  = (((box->x1 << log2bpp) + 7) >> 3) - bx0;
  under   = g_pointer.under;
  for (y = box->y0; y < box->y1; y++)
  {
    row = (unsigned char *) g_bm.base + y * g_bm.rowbytes + bx0;
    if (save)
      memcpy(under, row, nbytes);
    else
      memcpy(row, under, nbytes);
    under += nbytes;
  }
}

/* Put back the pixels under the pointer, if drawn. Returns whether it was,
 * setting *box to where, for the caller to present. */
static bool pointer_undraw(box_t *box)
{
  if (!g_pointer.is_drawn)
    return false;

  pointer_copy_under(&g_pointer.drawn, false);
  g_pointer.is_drawn = false;
  *box = g_pointer.drawn;
  return true;
}

/* Save what's under the pointer's position and draw it there, if it's on
 * and over the window. Returns whether it drew, setting *box to where. */
static bool pointer_draw(box_t *box)
{
  box_t    arrow;
  box_t    screen;
  screen_t scr;

  if (!g_tasks.pointer || !g_pointer.in_window)
    return false;

  arrow.x0  = g_pointer.pos.x;
  arrow.y0  = g_pointer.pos.y;
  arrow.x1  = g_pointer.pos.x + g_pointer.image.size.w;
  arrow.y1  = g_pointer.pos.y + g_pointer.image.size.h;
  screen.x0 = 0;
  screen.y0 = 0;
  screen.x1 = g_bm.size.w;
  screen.y1 = g_bm.size.h;
  if (box_intersection(&arrow, &screen, box))
    return false;

  pointer_copy_under(box, true);

  /* a private screen_t: wuss's own may be left clipped to its last redraw */
  screen_for_bitmap(&scr, &g_bm);
  (void) screen_copy_bitmap(&scr, g_pointer.pos.x, g_pointer.pos.y,
                            &g_pointer.image);

  g_pointer.drawn    = *box;
  g_pointer.is_drawn = true;
  return true;
}

bool app_set_pointer(bool on)
{
  return wuss_frontend_hide_pointer(g_tasks.frontend, on && g_pointer.loaded);
}

/* ----------------------------------------------------------------------- */

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

/* file scope so app_set_mode (called from the Display task, long after
 * run_wuss's own locals are gone) can update pixels/rowbytes/scr_width/
 * scr_height on the very instance the main loop below is reading */
static struct wuss_frame_ctx g_frame_ctx;

static void wuss_frame(void *arg)
{
  struct wuss_frame_ctx *c = arg;
  wuss_input_t ev;
  bool         garbage;
  bool         stress;
  bool         redraw_all;
  box_t        old_pointer;
  bool         had_pointer;
  box_t        new_pointer;
  bool         has_pointer;

  /* before anything below can draw: see g_pointer */
  had_pointer = pointer_undraw(&old_pointer);

  while (wuss_frontend_poll(c->frontend, &ev))
  {
    switch (ev.kind)
    {
    case wuss_INPUT_MOUSE_DOWN:
    case wuss_INPUT_MOUSE_UP:
    case wuss_INPUT_MOUSE_MOVE:
    case wuss_INPUT_WHEEL:
      g_pointer.pos       = ev.pos;
      g_pointer.in_window = true;
      break;
    case wuss_INPUT_MOUSE_LEAVE:
      g_pointer.in_window = false;
      break;
    default:
      break;
    }

    switch (ev.kind)
    {
    case wuss_INPUT_QUIT:
      g_tasks.quit = true;
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

    case wuss_INPUT_KEY:
      {
        int claimed;

        /* the focused window first, then the launcher menu's shortcuts */
        wuss_key(c->wuss, ev.key, ev.mods, &claimed);
        if (!claimed)
          (void) tasks_launcher_key(ev.key, ev.mods);
      }
      break;

    default:
      break;
    }
  }

  wuss_idle(c->wuss);

  /* the launcher's Debug picks only set flags; act on them once per frame */
  garbage    = g_tasks.debug_garbage;
  stress     = g_tasks.debug_pixel_stress;
  redraw_all = g_tasks.debug_redraw_all;
  g_tasks.debug_garbage      = false;
  g_tasks.debug_pixel_stress = false;
  g_tasks.debug_redraw_all   = false;

  if (garbage)
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

    (void) pointer_draw(&new_pointer);
    wuss_frontend_present(c->frontend, c->bm, NULL);
  }
  else if (stress)
  {
    pixel_stress(c->wuss, c->scr_width, c->scr_height);
    (void) pointer_draw(&new_pointer);
    wuss_frontend_present(c->frontend, c->bm, NULL);
  }
  else
  {
    box_t dirty, region, touched;
    int   ndirty, i, have_touched, have_any;

    ndirty = wuss_get_dirty_count(c->wuss);
    dirty  = (box_t) BOX_INIT;
    for (i = 0; i < ndirty; i++)
    {
      wuss_get_dirty(c->wuss, i, &region);
      box_union(&dirty, &region, &dirty);
    }

    /* Pixels a fast blit path (window move/resize, scroll) slid to a new
     * position without a repaint: wuss_redraw_dirty won't touch them, but
     * present() still has to re-upload them -- fold them into the same
     * bounding box before wuss_clear_touched drops them. */
    have_touched = wuss_get_touched_extent(c->wuss, &touched);
    have_any     = (ndirty > 0) || have_touched;
    if (have_touched)
      box_union(&dirty, &touched, &dirty);

    wuss_redraw_dirty(c->wuss);
    wuss_clear_touched(c->wuss);

    /* where the pointer was and now is need presenting too */
    has_pointer = pointer_draw(&new_pointer);
    if (had_pointer)
      box_union(&dirty, &old_pointer, &dirty);
    if (has_pointer)
      box_union(&dirty, &new_pointer, &dirty);
    have_any = have_any || had_pointer || has_pointer;

    /* a Debug > Redraw earlier this frame repainted the whole pixel
     * buffer; any dirty/touched region collected afterwards (e.g. a mouse
     * move) is narrower than that and must not shrink the present rect
     * below full-screen, or the frontend only re-uploads the narrow rect
     * and leaves the rest of the previous frame's pixels on screen. */
    wuss_frontend_present(c->frontend, c->bm,
                          (have_any && !redraw_all) ? &dirty : NULL);
  }

#ifdef __EMSCRIPTEN__
  if (g_tasks.quit)
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
                         int         scale,
                         int         scr_width,
                         int         scr_height,
                         const char *tasks)
{
  static const char *const names[WUSS_MAIN_NFONTS] =
    { "DPT-Digits-Regular", "DPT-Digits-Bold", "Symbols" };

  result_t    rc;
  const char *filename;
  bmfont_t   *fonts[WUSS_MAIN_NFONTS];
  int         nfonts;
  int         i;
  void       *pixels;
  int         rowbytes;
  pixelfmt_t  fmt;
  bitmap_t    logo; /* desktop backdrop image; left unset (have_logo false)
                     * if resources/wuss/wuss.png fails to load */
  bool     have_logo;
  colour_t palette[wuss_SYSTEM_PALETTE_LENGTH]; /* the fixed-size UI palette */
  colour_t scr_palette[256]; /* palette[] padded out to whatever
                                        * count the chosen depth's bitmap
                                        * needs (p8 reads all 256) */
  wuss_t          *wuss;
  wuss_frontend_t *frontend;
  bool             use_wimp16;
  int              palette_index;
  int              scr_nentries;

  /* everything the Failure path frees, so an early goto frees nothing */
  nfonts    = 0;
  frontend  = NULL;
  have_logo = false;
  wuss      = NULL;

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
  }

  logf_info("wuss: resources root = \"%s\"", resources);

  {
    nfonts = 0;
    for (i = 0; i < WUSS_MAIN_NFONTS; i++)
    {
      filename = pathf("%s/resources/bmfonts/%s.png", resources, names[i]);
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
   * needs (up to 256 for p8); pad scr_palette with palette's 16 UI colours,
   * then the web-safe 216 (so a p8 screen has real range beyond the UI
   * colours for nearest-match), then black for what's left. Also done on
   * a live palette change; see task_handle_event. 1/2bpp get a grey ramp
   * instead. Unpaletted formats ignore it, so fill the lot. */
  scr_nentries = pixelfmt_paletted_nentries(fmt);
  if (scr_nentries <= 0)
    scr_nentries = NELEMS(scr_palette);
  tasks_build_screen_palette(scr_palette, scr_nentries,
                             palette, NELEMS(palette));

  rc = bitmap_init(&g_bm, SIZE2D(scr_width, scr_height), fmt, rowbytes,
                   scr_palette, pixels);
  logf_info("wuss: bitmap_init -> rc=0x%X (%s)", rc, result_string(rc));
  if (rc != result_OK)
    goto Failure;

  bitmap_clear(&g_bm, palette[palette_PICO8_WHITE]);

  screen_for_bitmap(&g_scr, &g_bm);

  filename = pathf("%s/resources/wuss/wuss.png", resources);
  logf_info("wuss: loading backdrop image \"%s\"", filename);
  rc = bitmap_load_png(&logo, filename);
  have_logo = (rc == result_OK);
  if (!have_logo)
    logf_error("wuss: bitmap_load_png(\"%s\") failed, rc=0x%X (%s) -- "
              "backdrop drawn without it", filename, rc, result_string(rc));

  /* under holds the bytes beneath the arrow at any depth: at most 4 per
   * pixel, which also covers a sub-byte format's extra partial byte */
  filename = pathf("%s/resources/wuss/pointer.png", resources);
  rc = bitmap_load_png(&g_pointer.image, filename);
  if (rc == result_OK)
  {
    g_pointer.under = malloc((size_t) g_pointer.image.size.w * 4 *
                             g_pointer.image.size.h);
    g_pointer.loaded = (g_pointer.under != NULL);
    if (!g_pointer.loaded)
    {
      free(g_pointer.image.base);
      free(g_pointer.image.palette);
    }
  }
  else
  {
    logf_error("wuss: bitmap_load_png(\"%s\") failed, rc=0x%X (%s) -- "
              "software pointer unavailable", filename, rc, result_string(rc));
  }

  {
    wuss_config_t    config;
    wuss_font_desc_t descs[WUSS_MAIN_NFONTS]; /* slot classes/names for the
                                * picker menus -- [2] Symbols is chrome-only,
                                * never a text font choice */

    fill_chrome_config(&config, use_wimp16);
    if (have_logo)
      config.backdrop.image = &logo;

    for (i = 0; i < nfonts; i++)
    {
      descs[i].font       = fonts[i];
      descs[i].font_class = (i == 2) ? wuss_FONT_CLASS_SYSTEM
                                     : wuss_FONT_CLASS_NONE;
      descs[i].name       = names[i];
    }

    rc = wuss_create(&g_scr, descs, nfonts, palette, NELEMS(palette), &config,
                     NULL, resources, &wuss);
    logf_info("wuss: wuss_create -> rc=0x%X (%s)", rc, result_string(rc));
    if (rc != result_OK)
      goto Failure;
  }

  g_tasks.wuss           = wuss;
  g_tasks.frontend       = frontend;
  g_tasks.bm             = &g_bm;

  /* --crt: the frontend opens plain; switch over now so the Debug menu's
   * tick agrees */
  if (g_tasks.crt)
    tasks_set_crt(true);

  /* --pointer likewise */
  if (g_tasks.pointer)
    tasks_set_pointer(true);

  {
    wuss_task_desc_t desc;

    desc.handle    = task_handle_event;
    desc.task_data = NULL;
    desc.name      = "menu";
    rc = wuss_task_create(wuss, &desc, &g_tasks.menu_task);
    logf_info("wuss: wuss_task_create(menu) -> rc=0x%X (%s)", rc,
              result_string(rc));
    if (rc != result_OK)
      goto Failure;
  }

  g_tasks.quit = false;

#ifdef __EMSCRIPTEN__
  /* browser users can't pass options, so show Configure up front */
  rc = config_create(wuss, NULL);
  logf_info("wuss: config_create -> rc=0x%X (%s)", rc, result_string(rc));
  if (rc != result_OK)
    goto Failure;
#endif

  tasks_spawn(tasks);

  wuss_redraw(wuss);
  wuss_frontend_present(frontend, &g_bm, NULL);

  g_frame_ctx.wuss          = wuss;
  g_frame_ctx.frontend      = frontend;
  g_frame_ctx.bm            = &g_bm;
  g_frame_ctx.pixels        = pixels;
  g_frame_ctx.rowbytes      = rowbytes;
  g_frame_ctx.scr_width     = scr_width;
  g_frame_ctx.scr_height    = scr_height;
  g_frame_ctx.palette       = palette;
  g_frame_ctx.npalette      = NELEMS(palette);

#ifdef __EMSCRIPTEN__
  /* the browser owns the loop; simulate_infinite_loop=1 means this call
   * never returns -- fine, the page dies on navigation. fps=0 asks for
   * requestAnimationFrame pacing. */
  emscripten_set_main_loop_arg(wuss_frame, &g_frame_ctx, 0, 1);
#else
  while (!g_tasks.quit)
    wuss_frame(&g_frame_ctx);
#endif

  rc = result_OK;

  /* Shared by the normal exit and every early failure: each resource is
   * either NULL/unset (see above) or live. */
Failure:

  if (rc != result_OK)
    printf("run_wuss: failed (rc=0x%X: %s)\n", rc, result_string(rc));

  /* ponytail: wuss_destroy() force-closes every still-open window and frees
   * every registered task node, but not the per-instance task_data block a
   * spawn_* calloc'd, so any task window left open at quit leaks that block.
   * Harmless at process exit. */
  wuss_destroy(wuss); /* also sweeps g.menu_task and closes any open chain,
                       * including the shared proginfo singleton's window */

  if (have_logo) /* after wuss_destroy: the backdrop points at it */
  {
    free(logo.base);
    free(logo.palette);
  }

  if (g_pointer.loaded)
  {
    free(g_pointer.under);
    free(g_pointer.image.base);
    free(g_pointer.image.palette);
  }

  for (i = 0; i < nfonts; i++)
    bmfont_destroy(fonts[i]);

  wuss_frontend_close(frontend);

  return (rc == result_OK) ? result_TEST_PASSED : result_TEST_FAILED;
}

/* ----------------------------------------------------------------------- */

result_t app_set_mode(size2d_t size, int depth)
{
  result_t        rc;
  void           *pixels;
  int             rowbytes;
  pixelfmt_t      fmt;
  const colour_t *palette;
  int             npalette;
  colour_t        scr_palette[256];
  int             scr_nentries;

  rc = wuss_frontend_resize(g_tasks.frontend, size.w, size.h, depth,
                            &pixels, &rowbytes, &fmt);
  if (rc != result_OK)
    return rc;

  /* drop the old palette buffer: it's sized for the old format, and
   * bitmap_init would otherwise copy out of it (overrunning if the new
   * format has more entries) and leak it. Rebuilt below. */
  bitmap_set_palette(&g_bm, NULL);
  rc = bitmap_init(&g_bm, size, fmt, rowbytes, NULL, pixels);
  if (rc != result_OK)
    return rc;

  palette      = wuss_get_palette(g_tasks.wuss, &npalette);
  scr_nentries = pixelfmt_paletted_nentries(g_bm.format);
  if (scr_nentries > 0)
  {
    tasks_build_screen_palette(scr_palette, scr_nentries, palette, npalette);
    bitmap_set_palette(&g_bm, scr_palette);
  }

  screen_for_bitmap(&g_scr, &g_bm);

  rc = wuss_resize(g_tasks.wuss, &g_scr);
  if (rc != result_OK)
    return rc;

  g_frame_ctx.pixels     = pixels;
  g_frame_ctx.rowbytes   = rowbytes;
  g_frame_ctx.scr_width  = size.w;
  g_frame_ctx.scr_height = size.h;

  return result_OK;
}

int app_get_depth(void)
{
  switch (g_bm.format)
  {
  case pixelfmt_rgbx5551: return 15;
  case pixelfmt_rgb565:   return 16;
  default:                return 1 << pixelfmt_log2bpp(g_bm.format);
  }
}

/* ----------------------------------------------------------------------- */

/* Parsed command-line options. Members are use-ordered to match run_wuss's
 * parameter list. */
typedef struct wuss_options
{
  const char *resources;    /* -r/--resources: fixture root */
  const char *palette_name; /* -p/--palette: startup *.hex leafname */
  int         depth;        /* -d/--depth: framebuffer bpp (1, 2, 4, 8, 15, 16 or 32) */
  int         scale;        /* -s/--scale: initial window zoom, 0 = default */
  int         res_width;    /* --res WIDTHxHEIGHT: screen size in pixels */
  int         res_height;
  const char *tasks;        /* -t/--tasks: comma-separated launcher task
                             * names to auto-open at startup, or "all";
                             * default "" opens none */
}
wuss_options_t;

static const char wuss_usage[] =
  "usage: wuss [-r|--resources DIR] [-p|--palette NAME] "
  "[-d|--depth 1|2|4|8|15|16|32] [-s|--scale N] [--res WIDTHxHEIGHT] "
  "[-t|--tasks all|NAME[,NAME...]] [--crt] [--pointer]\n";

/* Parses "WIDTHxHEIGHT" (e.g. "1024x768") into w and h. Returns false,
 * leaving them untouched, on anything else -- a missing 'x', a non-positive
 * dimension, or trailing junk after the height. */
static bool parse_res(const char *s, int *w, int *h)
{
  char *end;
  long  width, height;

  width = strtol(s, &end, 10);
  if (end == s || *end != 'x')
    return false;

  height = strtol(end + 1, &end, 10);
  if (*end != '\0' || width <= 0 || height <= 0)
    return false;

  *w = (int) width;
  *h = (int) height;
  return true;
}

#ifndef __riscos

/* --res, --crt and --pointer have no short form, so they are given
 * longopt-only codes past the ASCII range getopt_long uses for short
 * options. */
enum { OPT_RES = 256, OPT_CRT, OPT_POINTER };

/* Desktop: getopt_long. Accepts the short forms and the "--" long forms; the
 * historical single-dash long spellings (-resources) are no longer accepted.
 * Returns false and prints usage on an unknown option or missing argument. */
static bool parse_args(int argc, char *argv[], wuss_options_t *opts)
{
  static const struct option longopts[] =
  {
    { "resources", required_argument, NULL, 'r'         },
    { "palette",   required_argument, NULL, 'p'         },
    { "depth",     required_argument, NULL, 'd'         },
    { "scale",     required_argument, NULL, 's'         },
    { "res",       required_argument, NULL, OPT_RES     },
    { "tasks",     required_argument, NULL, 't'         },
    { "crt",       no_argument,       NULL, OPT_CRT     },
    { "pointer",   no_argument,       NULL, OPT_POINTER },
    { NULL,        0,                 NULL, 0           }
  };

  int c;

  for (;;)
  {
    c = getopt_long(argc, argv, "r:p:d:s:t:", longopts, NULL);
    if (c == -1)
      break;

    switch (c)
    {
    case 'r': opts->resources    = optarg;       break;
    case 'p': opts->palette_name = optarg;       break;
    case 'd': opts->depth        = atoi(optarg); break;
    case 's': opts->scale        = atoi(optarg); break;
    case 't': opts->tasks        = optarg;       break;
    case OPT_CRT:
      g_tasks.crt = true;
      break;
    case OPT_POINTER:
      g_tasks.pointer = true;
      break;
    case OPT_RES:
      if (!parse_res(optarg, &opts->res_width, &opts->res_height))
      {
        fprintf(stderr, "wuss: --res expects WIDTHxHEIGHT, got \"%s\"\n", optarg);
        return false;
      }
      break;
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
    else if (strcmp(argv[i], "-res") == 0 && i + 1 < argc)
      parse_res(argv[++i], &opts->res_width, &opts->res_height);
    else if (strcmp(argv[i], "-tasks") == 0 && i + 1 < argc)
      opts->tasks = argv[++i];

  return true;
}

#endif /* __riscos */

int main(int argc, char *argv[])
{
  /* pathf splices the root and each branch with the platform separator, so
   * the "here" root differs: "." on Unix, but on RISC OS the
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
  opts.res_width    = 640;
  opts.res_height   = 480;
  opts.tasks        = "";

  if (!parse_args(argc, argv, &opts))
    return EXIT_FAILURE;

  rc = run_wuss(opts.resources, opts.palette_name, opts.depth, opts.scale,
               opts.res_width, opts.res_height, opts.tasks);

  return rc == result_TEST_PASSED ? EXIT_SUCCESS : EXIT_FAILURE;
}
