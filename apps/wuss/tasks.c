/* wuss/tasks.c -- Wuss demo task launcher and menu wiring */

#include <stdlib.h>
#include <string.h>

#include "base/debug.h"
#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "wuss/task.h"
#include "wuss/wuss.h"
#include "wuss/window.h"
#include "wuss/menu.h"
#include "wuss/component/proginfo.h"

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

struct wuss_app_tasks g_tasks;

void tasks_build_screen_palette(colour_t       *out,
                                int             nout,
                                const colour_t *ui,
                                int             nui)
{
  int r, g, b, n;

  memset(out, 0, nout * sizeof(*out));

  n = MIN(nui, nout);
  memcpy(out, ui, n * sizeof(*out));

  for (r = 0; r < 6 && n < nout; r++)
    for (g = 0; g < 6 && n < nout; g++)
      for (b = 0; b < 6 && n < nout; b++)
        out[n++] = colour_rgb(r * 0x33, g * 0x33, b * 0x33);
}

/* Every demo task's X_create now shares one shape: (wuss_t *, X_task_t **).
 * The task block it allocates is owned by its window and freed by
 * X_destroy, called from the task's wuss_EVENT_QUIT handler; callers here
 * never need the block back, so every X_create is invoked with NULL for it,
 * which is type-compatible however the second parameter is spelt. That lets
 * a single generic spawn walk a table of {name, X_create} instead of one
 * hand-written wrapper per task. */
typedef result_t (*task_create_fn_t)(wuss_t *wuss, void **out);

static result_t spawn_task(const char *name, task_create_fn_t create)
{
  result_t rc;

  rc = create(g_tasks.wuss, NULL);
  if (rc != result_OK)
    logf_error("wuss: %s create failed, rc=0x%X (%s)", name, rc,
               result_string(rc));
  return rc;
}

/* The task launcher is a MENU-button pop-up over the backdrop rather than a
 * window of buttons. Each leaf menu pairs a *_items table with a *_spawn table
 * in lock-step: picking row i of that menu calls its spawn[i]. */
typedef result_t (*task_spawn_fn_t)(void);

/* Category submenus, hung directly off the top-level task menu (see
 * g_task_items below): g_*_items[i] and g_*_tasks[i] are picked by the same
 * menu row index i -- keep each pair's tables in that order. Each category
 * menu is dispatched in task_handle_event by matching
 * event->data.menu_select.menu against the category's g_*_menu address. */
static const struct
{
  const char      *name;
  task_create_fn_t create;
}
g_games_tasks[] =
{
  { "Ball",        (task_create_fn_t) ball_create        },
  { "Minesweeper", (task_create_fn_t) minesweeper_create }
},
g_tests_tasks[] =
{
  { "Checker",     (task_create_fn_t) checker_create     },
  { "Curve",       (task_create_fn_t) curve_create       },
  { "Greeble",     (task_create_fn_t) greeble_create     },
  { "Icons",       (task_create_fn_t) icons_create       },
  { "Porter-Duff", (task_create_fn_t) porter_duff_create },
  { "Sofa",        (task_create_fn_t) sofa_create        },
  { "Text",        (task_create_fn_t) text_create        }
},
g_utilities_tasks[] =
{
  { "Chars",       (task_create_fn_t) chars_create       },
  { "Clock",       (task_create_fn_t) clock_create       },
  { "Palette",     (task_create_fn_t) palette_create     },
  { "Swatches",    (task_create_fn_t) swatches_create    }
},
g_visuals_tasks[] =
{
  { "Blank",       (task_create_fn_t) blank_create       },
  { "Gradient",    (task_create_fn_t) gradient_create    },
  { "Image",       (task_create_fn_t) image_create       },
  { "Lissajous",   (task_create_fn_t) lissajous_create   },
  { "Saturn",      (task_create_fn_t) saturn_create      }
};

static const wuss_menu_item_t g_games_items[] =
{
  { "Ball",        wuss_MENU_ITEM_NONE, NULL },
  { "Minesweeper", wuss_MENU_ITEM_NONE, NULL }
};

static const wuss_menu_item_t g_tests_items[] =
{
  { "Checker",     wuss_MENU_ITEM_NONE, NULL },
  { "Curve",       wuss_MENU_ITEM_NONE, NULL },
  { "Greeble",     wuss_MENU_ITEM_NONE, NULL },
  { "Icons",       wuss_MENU_ITEM_NONE, NULL },
  { "Porter-Duff", wuss_MENU_ITEM_NONE, NULL },
  { "Sofa",        wuss_MENU_ITEM_NONE, NULL },
  { "Text",        wuss_MENU_ITEM_NONE, NULL }
};

static const wuss_menu_item_t g_utilities_items[] =
{
  { "Chars",       wuss_MENU_ITEM_NONE, NULL },
  { "Clock",       wuss_MENU_ITEM_NONE, NULL },
  { "Palette",     wuss_MENU_ITEM_NONE, NULL },
  { "Swatches",    wuss_MENU_ITEM_NONE, NULL }
};

static const wuss_menu_item_t g_visuals_items[] =
{
  { "Blank",       wuss_MENU_ITEM_NONE, NULL },
  { "Gradient",    wuss_MENU_ITEM_NONE, NULL },
  { "Image",       wuss_MENU_ITEM_NONE, NULL },
  { "Lissajous",   wuss_MENU_ITEM_NONE, NULL },
  { "Saturn",      wuss_MENU_ITEM_NONE, NULL }
};

static const wuss_menu_t g_games_menu =
{
  "Games", g_games_items, NELEMS(g_games_items)
};

static const wuss_menu_t g_tests_menu =
{
  "Tests", g_tests_items, NELEMS(g_tests_items)
};

static const wuss_menu_t g_utilities_menu =
{
  "Utilities", g_utilities_items, NELEMS(g_utilities_items)
};

static const wuss_menu_t g_visuals_menu =
{
  "Visuals", g_visuals_items, NELEMS(g_visuals_items)
};

static result_t spawn_quit(void)
{
  g_tasks.quit = true;
  return result_OK;
}

/* Indices into g_task_items / g_task_spawn -- keep both tables in this
 * order. */
enum
{
  TASK_ITEM_INFO,
  TASK_ITEM_GAMES,
  TASK_ITEM_TESTS,
  TASK_ITEM_UTILITIES,
  TASK_ITEM_VISUALS,
  TASK_ITEM_QUIT
};

/* g_task_items' "Info" row's .window is filled in by tasks_open_launcher,
 * retargeting the shared proginfo singleton at g_tasks.menu_task each time
 * -- the table itself cannot name it at compile time. */
static wuss_menu_item_t g_task_items[] =
{
  { "Info",      wuss_MENU_ITEM_PRE_OPEN, NULL,           NULL },
  { "Games",     wuss_MENU_ITEM_NONE, &g_games_menu,      NULL },
  { "Tests",     wuss_MENU_ITEM_NONE, &g_tests_menu,      NULL },
  { "Utilities", wuss_MENU_ITEM_NONE, &g_utilities_menu,  NULL },
  { "Visuals",   wuss_MENU_ITEM_NONE, &g_visuals_menu,    NULL },
  { "Quit Wuss", wuss_MENU_ITEM_NONE, NULL,               NULL }
};

static const task_spawn_fn_t g_task_spawn[] =
{
  NULL,        /* "Info" -> wuss_menu_item_t.window leaf, no spawn */
  NULL,        /* "Games" -> submenu g_games_menu */
  NULL,        /* "Tests" -> submenu g_tests_menu */
  NULL,        /* "Utilities" -> submenu g_utilities_menu */
  NULL,        /* "Visuals" -> submenu g_visuals_menu */
  spawn_quit
};

static const wuss_menu_t g_task_menu =
{
  "Wuss", g_task_items, NELEMS(g_task_items)
};

/* g_task_menu picks are dispatched by index; g_games_menu/g_tests_menu/
 * g_utilities_menu/g_visuals_menu picks by pointer identity below. Every
 * menu is opened by g.menu_task, so one handler sees every
 * wuss_EVENT_MENU_SELECT and tells them apart by data.menu_select.menu. */
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

    palette      = wuss_get_palette(g_tasks.wuss, &npalette);
    scr_nentries = pixelfmt_paletted_nentries(g_tasks.bm->format);
    if (scr_nentries > 0)
    {
      tasks_build_screen_palette(scr_palette, scr_nentries, palette, npalette);
      bitmap_set_palette(g_tasks.bm, scr_palette);
    }
    wuss_frontend_set_palette(g_tasks.frontend, palette, npalette);
    return result_OK;
  }

  if (event->kind == wuss_EVENT_PRE_SHOW)
  {
    result_t rc;

    if (window == g_task_items[TASK_ITEM_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;

    if (event->data.pre_show.handle == NULL)
      return result_OK; /* plain window reveal, not a flagged menu leaf:
                          * already proceeding by default */

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  if (event->kind != wuss_EVENT_MENU_SELECT)
    return result_OK;

  menu  = event->data.menu_select.menu;
  index = event->data.menu_select.index;

  if (menu == &g_games_menu)
  {
    if (index >= 0 && index < (int) NELEMS(g_games_tasks))
      (void) spawn_task(g_games_tasks[index].name, g_games_tasks[index].create);
    return result_OK;
  }

  if (menu == &g_tests_menu)
  {
    if (index >= 0 && index < (int) NELEMS(g_tests_tasks))
      (void) spawn_task(g_tests_tasks[index].name, g_tests_tasks[index].create);
    return result_OK;
  }

  if (menu == &g_utilities_menu)
  {
    if (index >= 0 && index < (int) NELEMS(g_utilities_tasks))
      (void) spawn_task(g_utilities_tasks[index].name,
                        g_utilities_tasks[index].create);
    return result_OK;
  }

  if (menu == &g_visuals_menu)
  {
    if (index >= 0 && index < (int) NELEMS(g_visuals_tasks))
      (void) spawn_task(g_visuals_tasks[index].name,
                        g_visuals_tasks[index].create);
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
  static const wuss_proginfo_desc_t desc =
  {
    "Wuss demo",
    "Window manager test environment",
    "(c) DPTLib contributors",
    "1.0 (" __DATE__ ")"
  };

  wuss_proginfo_set_desc(&desc);
  g_task_items[TASK_ITEM_INFO].window = wuss_proginfo_window(g_tasks.menu_task);

  return wuss_menu_open(g_tasks.menu_task, &g_task_menu, pos, NULL);
}
