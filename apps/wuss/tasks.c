/* wuss/tasks.c -- Wuss demo task launcher and menu wiring */

#include <stdlib.h>
#include <string.h>

#include "base/debug.h"
#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
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
  rc = palette_create(g.wuss, g.resources, g.palette_name, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_image(void)
{
  result_t      rc;
  image_task_t *t;
  const char   *leafname;
  const char   *filename;
  char          buf[DPTLIB_MAXPATH];
  char          ninepatch[DPTLIB_MAXPATH];

  t = calloc(1, sizeof(*t));
  if (t == NULL) return result_OOM;

  leafname = path_join_leafname("jessica", "png");
  filename = path_join_filename(g.resources, 3, "resources", "images", leafname);
  strcpy(buf, filename);
  /* path_join_filename returns a shared static buffer; copy before the next
   * call (image_create's own dirscan join) clobbers it */
  filename = path_join_filename(g.resources, 3, "resources", "wuss",
                                path_join_leafname("ninepatch", "png"));
  strcpy(ninepatch, filename);

  logf_info("wuss: image task loading \"%s\" + \"%s\"", buf, ninepatch);
  rc = image_create(g.wuss, g.resources, buf, ninepatch, t);
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

static result_t spawn_minesweeper(void)
{
  minesweeper_task_t *t = calloc(1, sizeof(*t));
  result_t             rc;
  if (t == NULL) return result_OOM;
  rc = minesweeper_create(g.wuss, g.bold_font, t);
  if (rc != result_OK) return rc;
  if (t->window == NULL) { free(t); return rc; }
  return result_OK;
}

static result_t spawn_saturn(void)
{
  saturn_task_t *t = calloc(1, sizeof(*t));
  result_t       rc;
  if (t == NULL) return result_OOM;
  rc = saturn_create(g.wuss, t);
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

static result_t spawn_greeble(void)
{
  greeble_task_t *t = calloc(1, sizeof(*t));
  result_t        rc;
  if (t == NULL) return result_OOM;
  rc = greeble_create(g.wuss, t);
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
  spawn_porter_duff, spawn_saturn, spawn_sofa, spawn_swatches, spawn_text
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

static const wuss_menu_item_t g_task_items[] =
{
  { "Launch",    wuss_MENU_ITEM_NONE,   &g_launch_menu, NULL },
  { "Quit Wuss", wuss_MENU_ITEM_DASHED, NULL,           NULL }
};

static const task_spawn_fn_t g_task_spawn[] =
{
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

  NOT_USED(window);
  NOT_USED(task_data);

  if (event->kind == wuss_EVENT_PALETTE)
  {
    /* wuss_set_palette already updated wuss's own copy and re-cached
     * furniture; read the new array back and push it on to the framebuffer
     * bitmap and any physical palette, so callers (e.g. the palette picker)
     * never need to know the frontend exists */
    const colour_t *palette;
    int              npalette;

    palette = wuss_get_palette(g.wuss, &npalette);
    bitmap_set_palette(g.bm, palette);
    wuss_frontend_set_palette(g.frontend, palette, npalette);
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
  return wuss_menu_open(g.menu_task, &g_task_menu, pos, NULL);
}
