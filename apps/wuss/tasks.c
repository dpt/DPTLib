/* wuss/tasks.c -- Wuss demo task launcher and menu wiring */

#include <stdlib.h>
#include <string.h>

#include "base/debug.h"
#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "io/path.h"
#include "wuss/task.h"
#include "wuss/wuss.h"
#include "wuss/window.h"
#include "wuss/menu.h"

#include "frontend.h"
#include "tasks.h"

#include "tasks/ball.h"
#include "tasks/blank.h"
#include "tasks/chars.h"
#include "tasks/checker.h"
#include "tasks/clock.h"
#include "tasks/curve.h"
#include "tasks/gradient.h"
#include "tasks/greeble.h"
#include "tasks/icons.h"
#include "tasks/image.h"
#include "tasks/lissajous.h"
#include "tasks/minesweeper.h"
#include "tasks/palette.h"
#include "tasks/porter-duff.h"
#include "tasks/saturn.h"
#include "tasks/sofa.h"
#include "tasks/swatches.h"
#include "tasks/text.h"

/* ----------------------------------------------------------------------- */

struct wuss_app_tasks g;

void tasks_build_screen_palette(colour_t       *out,
                                int             nout,
                                const colour_t *ui,
                                int             nui)
{
  int r, g_, b, n;

  memset(out, 0, nout * sizeof(*out));

  n = MIN(nui, nout);
  memcpy(out, ui, n * sizeof(*out));

  for (r = 0; r < 6 && n < nout; r++)
    for (g_ = 0; g_ < 6 && n < nout; g_++)
      for (b = 0; b < 6 && n < nout; b++)
        out[n++] = colour_rgb(r * 0x33, g_ * 0x33, b * 0x33);
}

/* Each spawn is a thin passthrough to its module's X_create, which allocates
 * the per-instance task block itself; the block is owned by its window and
 * freed by X_destroy, called from the task's wuss_EVENT_QUIT handler (or,
 * for the font-less chars case, never allocated at all -- see
 * chars_create). None of these spawns need the task pointer X_create can
 * hand back, so they all pass NULL for it. */

static result_t spawn_ball(void)
{
  return ball_create(g.wuss, NULL);
}

static result_t spawn_text(void)
{
  return text_create(g.wuss, g.resources, NULL);
}

static result_t spawn_blank(void)
{
  return blank_create(g.wuss, NULL);
}

static result_t spawn_chars(void)
{
  return chars_create(g.wuss, g.resources, NULL);
}

static result_t spawn_palette(void)
{
  return palette_create(g.wuss, g.resources, g.palette_name, NULL);
}

static result_t spawn_image(void)
{
  result_t    rc;
  const char *leafname;
  const char *filename;
  char        buf[DPTLIB_MAXPATH];
  char        ninepatch[DPTLIB_MAXPATH];

  leafname = path_join_leafname("jessica", "png");
  filename = path_join_filename(g.resources, 3, "resources", "images", leafname);
  strcpy(buf, filename);
  /* path_join_filename returns a shared static buffer; copy before the next
   * call (image_create's own dirscan join) clobbers it */
  filename = path_join_filename(g.resources, 3, "resources", "wuss",
                                path_join_leafname("ninepatch", "png"));
  strcpy(ninepatch, filename);

  logf_info("wuss: image task loading \"%s\" + \"%s\"", buf, ninepatch);
  rc = image_create(g.wuss, g.resources, buf, ninepatch, NULL);
  if (rc != result_OK)
    logf_error("wuss: image_create(\"%s\") failed, rc=0x%X (%s)", buf, rc,
               result_string(rc));
  return rc;
}

static result_t spawn_checker(void)
{
  return checker_create(g.wuss, NULL);
}

static result_t spawn_clock(void)
{
  return clock_create(g.wuss, g.daydream_font, NULL);
}

static result_t spawn_curve(void)
{
  return curve_create(g.wuss, NULL);
}

static result_t spawn_lissajous(void)
{
  return lissajous_create(g.wuss, NULL);
}

static result_t spawn_minesweeper(void)
{
  return minesweeper_create(g.wuss, g.bold_font, NULL);
}

static result_t spawn_saturn(void)
{
  return saturn_create(g.wuss, NULL, NULL);
}

static result_t spawn_sofa(void)
{
  return sofa_create(g.wuss, NULL);
}

static result_t spawn_gradient(void)
{
  return gradient_create(g.wuss, NULL);
}

static result_t spawn_greeble(void)
{
  return greeble_create(g.wuss, NULL);
}

static result_t spawn_icons(void)
{
  result_t rc;

  rc = icons_create(g.wuss, g.daydream_font, g.resources, NULL);
  if (rc != result_OK)
    logf_error("wuss: icons_create (resources \"%s\") failed, rc=0x%X (%s)",
               g.resources, rc, result_string(rc));
  return rc;
}

static result_t spawn_swatches(void)
{
  return swatches_create(g.wuss, NULL);
}

static result_t spawn_porter_duff(void)
{
  result_t rc;

  rc = porter_duff_create(g.wuss, g.palette, g.daydream_font, g.resources, NULL);
  if (rc != result_OK)
    logf_error("wuss: porter_duff_create (resources \"%s\") failed, "
               "rc=0x%X (%s)", g.resources, rc, result_string(rc));
  return rc;
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
  { "Greeble",     wuss_MENU_ITEM_NONE, NULL },
  { "Icons",       wuss_MENU_ITEM_NONE, NULL },
  { "Image",       wuss_MENU_ITEM_NONE, NULL },
  { "Lissajous",   wuss_MENU_ITEM_NONE, NULL },
  { "Minesweeper", wuss_MENU_ITEM_NONE, NULL },
  { "Palette",     wuss_MENU_ITEM_NONE, NULL },
  { "Porter-Duff", wuss_MENU_ITEM_NONE, NULL },
  { "Saturn",      wuss_MENU_ITEM_NONE, NULL },
  { "Sofa",        wuss_MENU_ITEM_NONE, NULL },
  { "Swatches",    wuss_MENU_ITEM_NONE, NULL },
  { "Text",        wuss_MENU_ITEM_NONE, NULL }
};

static const task_spawn_fn_t g_launch_spawn[] =
{
  spawn_ball, spawn_blank, spawn_chars, spawn_checker, spawn_clock, spawn_curve,
  spawn_gradient, spawn_greeble, spawn_icons, spawn_image, spawn_lissajous,
  spawn_minesweeper, spawn_palette,
  spawn_porter_duff, spawn_saturn, spawn_sofa, spawn_swatches,
  spawn_text
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

/* Indices into g_task_items / g_task_spawn -- keep both tables in this
 * order. */
enum
{
  TASK_ITEM_INFO,
  TASK_ITEM_LAUNCH,
  TASK_ITEM_QUIT
};

/* g_task_items' "Info" row's .window is filled in by tasks_open_launcher
 * (built from g.proginfo, which does not exist until run_wuss creates it) --
 * the table itself cannot name it at compile time. */
static wuss_menu_item_t g_task_items[] =
{
  { "Info",      wuss_MENU_ITEM_NONE, NULL,           NULL },
  { "Launch",    wuss_MENU_ITEM_NONE, &g_launch_menu, NULL },
  { "Quit Wuss", wuss_MENU_ITEM_NONE, NULL,           NULL }
};

static const task_spawn_fn_t g_task_spawn[] =
{
  NULL,        /* "Info" -> wuss_menu_item_t.window leaf, no spawn */
  NULL,        /* "Launch" -> submenu g_launch_menu */
  spawn_quit
};

static const wuss_menu_t g_task_menu =
{
  "Tasks", g_task_items, NELEMS(g_task_items)
};

/* g_task_menu / g_launch_menu picks are dispatched by index. Every menu is
 * opened by g.menu_task, so one handler sees every wuss_EVENT_MENU_SELECT and
 * tells them apart by data.menu_select.menu. */
result_t task_handle_event(wuss_window_t      *window,
                           const wuss_event_t *event,
                           void               *task_data)
{
  const wuss_menu_t *menu;
  int                index;

  NOT_USED(task_data);

  if (event->kind == wuss_EVENT_PALETTE)
  {
    /* wuss_set_palette already updated wuss's own copy and re-cached
     * furniture; read the new array back and push it on to the framebuffer
     * bitmap and any physical palette, so callers (e.g. the palette picker)
     * never need to know the frontend exists. bitmap_set_palette reads
     * every entry g.bm's own format needs (up to 256 for p8), not just
     * wuss's fixed-size UI palette, so pad out to match -- same as the
     * startup array run_wuss builds. */
    const colour_t *palette;
    int             npalette;
    colour_t        scr_palette[256];
    int             scr_nentries;

    palette      = wuss_get_palette(g.wuss, &npalette);
    scr_nentries = pixelfmt_paletted_nentries(g.bm->format);
    if (scr_nentries > 0)
    {
      tasks_build_screen_palette(scr_palette, scr_nentries, palette, npalette);
      bitmap_set_palette(g.bm, scr_palette);
    }
    wuss_frontend_set_palette(g.frontend, palette, npalette);
    return result_OK;
  }

  if (event->kind == wuss_EVENT_PRE_SHOW)
  {
    if (window == wuss_proginfo_window(g.proginfo))
      return wuss_proginfo_handle_pre_show(g.proginfo);
    return result_OK;
  }

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

  if (menu == &g_task_menu)
  {
    if (index >= 0 && index < (int) NELEMS(g_task_spawn) && g_task_spawn[index])
      (void) g_task_spawn[index]();
    return result_OK;
  }

  return result_OK;
}

result_t tasks_open_launcher(point_t pos)
{
  g_task_items[TASK_ITEM_INFO].window = wuss_proginfo_window(g.proginfo);

  return wuss_menu_open(g.menu_task, &g_task_menu, pos, NULL);
}
