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
#include "wuss/window.h"
#include "wuss/menu.h"
#include "wuss/menu-desc.h"

#include "frontend.h"

#include "tasks/ball.h"
#include "tasks/blank.h"
#include "tasks/chars.h"
#include "tasks/checker.h"
#include "tasks/clock.h"
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

/* the launcher's spawn callbacks take no arguments, so the pieces they need
 * are stashed here instead; run_wuss runs at most once per
 * process, so a file-scope struct is as good as a passed-around context */
static struct
{
  wuss_t         *wuss;
  wuss_task_t    *menu_task; /* owns the menus and the hidden Details window */
  const colour_t *palette;
  int             npalette;
  const char     *resources;
  bmfont_t       *daydream_font;
  bool            quit; /* set by the "Quit Wuss" task-menu entry */
}
g;

/* Each spawn allocates a fresh per-instance task block so a task may run in
 * several windows at once; the block is owned by its window and freed by the
 * task's wuss_EVENT_QUIT handler. On any create failure X_create has already
 * torn down whatever it built and freed the block itself, so the only block
 * the spawner frees is the font-less chars case: create returns OK but opens
 * no window, leaving the block with no owner. */

static result_t spawn_ball(void)
{
  ball_task_t *t = calloc(1, sizeof(*t));
  result_t     rc;
  if (t == NULL) return result_OOM;
  rc = ball_create(g.wuss, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_text(void)
{
  text_task_t *t = calloc(1, sizeof(*t));
  result_t     rc;
  if (t == NULL) return result_OOM;
  rc = text_create(g.wuss, g.resources, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_blank(void)
{
  blank_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = blank_create(g.wuss, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_chars(void)
{
  chars_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = chars_create(g.wuss, g.resources, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_palette(void)
{
  palette_task_t *t = calloc(1, sizeof(*t));
  result_t        rc;
  if (t == NULL) return result_OOM;
  rc = palette_create(g.wuss, g.palette, g.npalette, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
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
                                 path_join_leafname("ninepatch", "png"));

  logf_info("wuss: image task loading \"%s\" + \"%s\"", buf, ninepatch);
  rc = image_create(g.wuss, buf, ninepatch, t);
  if (rc != result_OK)
    logf_error("wuss: image_create(\"%s\") failed, rc=0x%X (%s)", buf, rc,
               result_string(rc));
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_checker(void)
{
  checker_task_t *t = calloc(1, sizeof(*t));
  result_t        rc;
  if (t == NULL) return result_OOM;
  rc = checker_create(g.wuss, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_clock(void)
{
  clock_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = clock_create(g.wuss, g.daydream_font, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_curve(void)
{
  curve_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = curve_create(g.wuss, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_lissajous(void)
{
  lissajous_task_t *t = calloc(1, sizeof(*t));
  result_t          rc;
  if (t == NULL) return result_OOM;
  rc = lissajous_create(g.wuss, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_sofa(void)
{
  sofa_task_t *t = calloc(1, sizeof(*t));
  result_t     rc;
  if (t == NULL) return result_OOM;
  rc = sofa_create(g.wuss, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_gradient(void)
{
  gradient_task_t *t = calloc(1, sizeof(*t));
  result_t         rc;
  if (t == NULL) return result_OOM;
  rc = gradient_create(g.wuss, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_icons(void)
{
  icons_task_t *t = calloc(1, sizeof(*t));
  result_t      rc;
  if (t == NULL) return result_OOM;
  rc = icons_create(g.wuss, g.daydream_font, g.resources, t);
  if (rc != result_OK)
    logf_error("wuss: icons_create (resources \"%s\") failed, rc=0x%X (%s)",
               g.resources, rc, result_string(rc));
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_swatches(void)
{
  swatches_task_t *t = calloc(1, sizeof(*t));
  result_t         rc;
  if (t == NULL) return result_OOM;
  rc = swatches_create(g.wuss, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_porter_duff(void)
{
  porter_duff_task_t *t = calloc(1, sizeof(*t));
  result_t            rc;
  if (t == NULL) return result_OOM;
  rc = porter_duff_create(g.wuss, g.palette, g.daydream_font, g.resources, t);
  if (rc != result_OK)
    logf_error("wuss: porter_duff_create (resources \"%s\") failed, "
               "rc=0x%X (%s)", g.resources, rc, result_string(rc));
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
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

/* A caller-owned window wired as a menu item's `.window`: hovering "Details"
 * shows it where a submenu would open, and moving off the row (or dismissing
 * the menu) hides it again. Created once, lazily, by spawn_menu. */
static wuss_window_t *g_menu_details_window;

static wuss_menu_item_t g_menu_items[] =
{
  { "Open",      wuss_MENU_ITEM_NONE,   NULL,          NULL },
  { "Show grid", wuss_MENU_ITEM_TICKED, NULL,          NULL },
  { "Wireframe", wuss_MENU_ITEM_TICKED, NULL,          NULL },
  { "Export",    wuss_MENU_ITEM_NONE,   &g_menu_export, NULL },
  { "Details",   wuss_MENU_ITEM_NONE,   NULL,          NULL }, /* .window set in spawn_menu */
  { "Quit",      wuss_MENU_ITEM_DASHED, NULL,          NULL }
};

static const wuss_menu_t g_menu =
{
  "Display", g_menu_items, NELEMS(g_menu_items)
};

/* g_task_menu / g_launch_menu / g_test_menu picks are dispatched by index;
 * g_menu / g_menu_desc picks just print. Every menu is opened by g.menu_task,
 * so one handler sees every wuss_EVENT_MENU_SELECT and tells them apart by
 * data.menu_select.menu. Defined after the menus / spawn tables it needs. */
static result_t menu_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data);

/* index of the "Details" row in g_menu_items */
#define G_MENU_DETAILS_INDEX 4

static result_t spawn_menu(void)
{
  if (g_menu_details_window == NULL)
  {
    box_t    content;
    result_t rc;

    /* a small hidden window; wuss fills its background, no task needed */
    content.x0 = 0;
    content.y0 = 0;
    content.x1 = 180;
    content.y1 = 120;
    rc = wuss_window_create(g.menu_task, &content, "Details",
                            wuss_WINDOW_NO_CLOSE | wuss_WINDOW_NO_BACK
                            | wuss_WINDOW_NO_TOGGLE_SIZE
                            | wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL
                            | wuss_WINDOW_NO_RESIZE | wuss_WINDOW_HIDDEN,
                            wuss_BACKDROP_COLOUR(1),
                            SIZE2D(180, 120), SIZE2D(0, 0),
                            &g_menu_details_window);
    if (rc != result_OK)
      return rc;
    g_menu_items[G_MENU_DETAILS_INDEX].window = g_menu_details_window;
  }

  return wuss_menu_open(g.menu_task, &g_menu, wuss_get_pointer(g.wuss), NULL);
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

  return wuss_menu_open(g.menu_task, g_menu_desc, wuss_get_pointer(g.wuss),
                        NULL);
}

/* The task launcher is a MENU-button pop-up over the backdrop rather than a
 * window of buttons. Each leaf menu pairs a *_items table with a *_spawn table
 * in lock-step: picking row i of that menu calls its spawn[i]. */
typedef result_t (*task_spawn_fn_t)(void);

/* "Launch" submenu: the demo tasks. */
static const wuss_menu_item_t g_launch_items[] =
{
  { "Ball",        wuss_MENU_ITEM_NONE, NULL },
  { "Blank",       wuss_MENU_ITEM_NONE, NULL },
  { "Chars",       wuss_MENU_ITEM_NONE, NULL },
  { "Checker",     wuss_MENU_ITEM_NONE, NULL },
  { "Clock",       wuss_MENU_ITEM_NONE, NULL },
  { "Curve",       wuss_MENU_ITEM_NONE, NULL },
  { "Gradient",    wuss_MENU_ITEM_NONE, NULL },
  { "Icons",       wuss_MENU_ITEM_NONE, NULL },
  { "Image",       wuss_MENU_ITEM_NONE, NULL },
  { "Lissajous",   wuss_MENU_ITEM_NONE, NULL },
  { "Palette",     wuss_MENU_ITEM_NONE, NULL },
  { "Porter-Duff", wuss_MENU_ITEM_NONE, NULL },
  { "Sofa",        wuss_MENU_ITEM_NONE, NULL },
  { "Swatches",    wuss_MENU_ITEM_NONE, NULL },
  { "Text",        wuss_MENU_ITEM_NONE, NULL }
};

static const task_spawn_fn_t g_launch_spawn[] =
{
  spawn_ball, spawn_blank, spawn_chars, spawn_checker, spawn_clock, spawn_curve,
  spawn_gradient, spawn_icons, spawn_image, spawn_lissajous, spawn_palette,
  spawn_porter_duff, spawn_sofa, spawn_swatches, spawn_text
};

static const wuss_menu_t g_launch_menu =
{
  "Launch", g_launch_items, NELEMS(g_launch_items)
};

static result_t spawn_quit(void)
{
  g.quit = true;
  return result_OK;
}

/* "Test" submenu: the menu-system exercisers. */
static const wuss_menu_item_t g_test_items[] =
{
  { "Menu",        wuss_MENU_ITEM_NONE, NULL },
  { "Menu (desc)", wuss_MENU_ITEM_NONE, NULL }
};

static const task_spawn_fn_t g_test_spawn[] =
{
  spawn_menu, spawn_menu_desc
};

static const wuss_menu_t g_test_menu =
{
  "Test", g_test_items, NELEMS(g_test_items)
};

static const wuss_menu_item_t g_task_items[] =
{
  { "Launch",    wuss_MENU_ITEM_NONE,   &g_launch_menu, NULL },
  { "Test",      wuss_MENU_ITEM_DASHED, &g_test_menu,   NULL },
  { "Quit Wuss", wuss_MENU_ITEM_DASHED, NULL,           NULL }
};

static const task_spawn_fn_t g_task_spawn[] =
{
  NULL,        /* "Launch" -> submenu g_launch_menu */
  NULL,        /* "Test"   -> submenu g_test_menu */
  spawn_quit
};

static const wuss_menu_t g_task_menu =
{
  "Tasks", g_task_items, NELEMS(g_task_items)
};

static result_t menu_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  const wuss_menu_t *menu;
  int                index;

  NOT_USED(window);
  NOT_USED(task_data);

  if (event->kind != wuss_EVENT_MENU_SELECT)
    return result_OK;

  menu  = event->data.menu_select.menu;
  index = event->data.menu_select.index;

  if (menu == &g_launch_menu)
  {
    if (index >= 0 && index < (int) NELEMS(g_launch_spawn))
      (void) g_launch_spawn[index]();
    return result_OK;
  }

  if (menu == &g_test_menu)
  {
    if (index >= 0 && index < (int) NELEMS(g_test_spawn))
      (void) g_test_spawn[index]();
    return result_OK;
  }

  if (menu == &g_task_menu)
  {
    if (index >= 0 && index < (int) NELEMS(g_task_spawn) && g_task_spawn[index])
      (void) g_task_spawn[index]();
    return result_OK;
  }

  printf("menu: picked \"%s\"\n",
         menu->items[index].text ? menu->items[index].text : "(sep)");
  return result_OK;
}

/* Palettes the palette-cycle input steps through, in order. Each fills a
 * colour_t[16]. */
static void (*const g_palettes[])(colour_t *) =
{
  define_pico8_palette,
  define_wimp16_palette
};

/* Furniture/bevel/accent/backdrop colour indices, one row per palette. Same
 * field order as the assignments in run_wuss. */
static const wuss_colour_t g_chrome[2][15] =
{
  /* PICO-8 */
  { palette_PICO8_DARK_BLUE, palette_PICO8_WHITE, palette_PICO8_GREEN,
    palette_PICO8_RED, palette_PICO8_ORANGE, palette_PICO8_LAVENDER,
    palette_PICO8_BLUE, palette_PICO8_DARK_BLUE, palette_PICO8_LIGHT_GREY,
    palette_PICO8_WHITE, palette_PICO8_DARK_GREY, palette_PICO8_DARK_BLUE,
    palette_PICO8_WHITE, palette_PICO8_WHITE, palette_PICO8_LIGHT_GREY },
  /* RISC OS 16-colour Wimp */
  { palette_WIMP16_GREY_75, palette_WIMP16_BLACK, palette_WIMP16_GREEN,
    palette_WIMP16_RED, palette_WIMP16_ORANGE, palette_WIMP16_LIGHT_BLUE,
    palette_WIMP16_GREY_50, palette_WIMP16_GREY_62, palette_WIMP16_GREY_87,
    palette_WIMP16_WHITE, palette_WIMP16_GREY_50, palette_WIMP16_ORANGE,
    palette_WIMP16_BLACK, palette_WIMP16_GREY_50, palette_WIMP16_GREY_37 }
};

static void fill_chrome_config(wuss_config_t *config, int palette_index)
{
  const wuss_colour_t *c = g_chrome[palette_index];

  config->titlebar_height           = 0;
  config->furniture.title.bg        = c[0];
  config->furniture.title.fg        = c[1];
  config->furniture.back            = c[2];
  config->furniture.close           = c[3];
  config->furniture.toggle          = c[4];
  config->furniture.resize          = c[5];
  config->furniture.scroll.arrows   = c[6];
  config->furniture.scroll.wells    = c[7];
  config->furniture.scroll.sausages = c[8];
  config->bevel.light               = c[9];
  config->bevel.dark                = c[10];
  config->accent.bg                 = c[11];
  config->accent.fg                 = c[12];
  config->backdrop.colour           = c[13];
  config->backdrop.pattern          = screen_PATTERN_DOTS;
  config->backdrop.pattern_bg       = c[14];
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
  int              palette_index;
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

    case wuss_INPUT_PALETTE_CYCLE:
      /* rebuild the palette, push it into the framebuffer bitmap, tell the
       * backend (which owns any physical palette), then tell wuss, which
       * refreshes its own copy and pokes every task to recache */
      c->palette_index = (c->palette_index + 1) % (int) NELEMS(g_palettes);
      g_palettes[c->palette_index](c->palette);
      bitmap_set_palette(c->bm, c->palette);
      wuss_frontend_set_palette(c->frontend, c->palette, c->npalette);
      wuss_set_palette(c->wuss, c->palette, c->npalette);
      break;

    case wuss_INPUT_MOUSE_DOWN:
      {
        wuss_window_t *hit;

        wuss_mouse_click(c->wuss, ev.pos, ev.button, wuss_MOUSE_DOWN, &hit);

        /* MENU click on bare backdrop opens the task launcher there */
        if (hit == NULL && (ev.button & wuss_BUTTON_MENU))
          wuss_menu_open(g.menu_task, &g_task_menu, ev.pos, NULL);
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

/* click windows to bring to front, drag titlebars to move; the redraw-all
 * input redraws the whole screen, the pixel-stress input does it one pixel at
 * a time to catch tasks that misbehave under a 1x1 clip; the palette-cycle
 * input swaps the system palette live (wuss_set_palette); the quit input or
 * closing the window exits */
static result_t run_wuss(const char *resources)
{
  const int        scr_width  = 640;
  const int        scr_height = 480;

  result_t         rc;
  const char      *leafname;
  const char      *filename;
  bmfont_t        *fonts[2]; /* [0] regular (system font), [1] bold */
  int              nfonts;
  int              i;
  void            *pixels;
  int              rowbytes;
  pixelfmt_t       fmt;
  bitmap_t         bm;
  screen_t         scr;
  colour_t         palette[16];
  wuss_t          *wuss;
  wuss_frontend_t *frontend;
  bool             use_wimp16;
  int              palette_index;

  {
    /* "wimp16" selects the RISC OS 16-colour palette; default is PICO-8 */
    const char *palette_name = getenv("WUSS_PALETTE");

    use_wimp16 = (palette_name != NULL && strcmp(palette_name, "wimp16") == 0);
    palette_index = use_wimp16 ? 1 : 0;
    g_palettes[palette_index](palette);
  }

  logf_info("wuss: resources root = \"%s\"", resources);

  {
    static const char *const names[2] = { "Digits-Regular", "Digits-Bold" };

    nfonts = 0;
    for (i = 0; i < 2; i++)
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
    wuss_config_t config;

    fill_chrome_config(&config, use_wimp16 ? 1 : 0);

    rc = wuss_create(&scr, fonts, nfonts, palette, NELEMS(palette), &config,
                     NULL, &wuss);
    logf_info("wuss: wuss_create -> rc=0x%X (%s)", rc, result_string(rc));
    if (rc != result_OK)
      goto Failure;
  }

  g.wuss          = wuss;
  g.palette       = palette;
  g.npalette      = NELEMS(palette);
  g.resources     = resources;
  g.daydream_font = fonts[0]; /* tasks draw with the regular weight */

  {
    wuss_task_desc_t desc;

    desc.handle    = menu_handle;
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
    ctx.palette_index = palette_index;

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

  /* menus are only safe to free once wuss_destroy has torn down any chain
   * that was still borrowing them */
  wuss_menu_destroy(g_menu_desc);

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
