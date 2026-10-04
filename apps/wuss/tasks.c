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
#include "tasks/chasehq.h"
#include "tasks/greatescape.h"
#include "tasks/chars.h"
#include "tasks/checker.h"
#include "tasks/clock.h"
#include "tasks/common.h"
#include "tasks/config.h"
#include "tasks/curve.h"
#include "tasks/dithering.h"
#include "tasks/display.h"
#include "tasks/doughnut.h"
#include "tasks/effects.h"
#include "tasks/filer.h"
#include "tasks/gradient.h"
#include "tasks/greeble.h"
#include "tasks/iconbar-demo.h"
#include "tasks/icons.h"
#include "tasks/image.h"
#include "tasks/keys.h"
#include "tasks/lissajous.h"
#include "tasks/minesweeper.h"
#include "tasks/palette.h"
#include "tasks/particles.h"
#include "tasks/patterns.h"
#include "tasks/porter-duff.h"
#include "tasks/saturn.h"
#include "tasks/sofa.h"
#include "tasks/spheroid.h"
#include "tasks/text.h"
#include "tasks/tiling.h"

/* ----------------------------------------------------------------------- */

struct wuss_app_tasks g_tasks;

void tasks_build_screen_palette(colour_t       *out,
                                int             nout,
                                const colour_t *ui,
                                int             nui)
{
  int r, g, b, n;

  /* 1bpp and 2bpp screens can't hold the UI colours, so they get a fixed
   * grey ramp instead and the UI colours match onto it */
  if (nout == 2 || nout == 4)
  {
    for (n = 0; n < nout; n++)
    {
      g      = n * 0xFF / (nout - 1);
      out[n] = colour_rgb(g, g, g);
    }
    return;
  }

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

/* The backdrop MENU-button pop-up now holds only system actions; every task
 * is launched from AppsFiler, which walks g_task_categories below. */

/* One launchable task: g_*_tasks[i] below. */
typedef struct
{
  const char      *name;
  task_create_fn_t create;
}
task_entry_t;

static const task_entry_t
g_games_tasks[] =
{
  { "Ball",        (task_create_fn_t) ball_create        },
#ifdef WUSS_CHASEHQ
  { "Chase H.Q.",  (task_create_fn_t) chasehq_create     },
#endif
#ifdef WUSS_GREATESCAPE
  { "The Great Escape", (task_create_fn_t) greatescape_create },
#endif
  { "Minesweeper", (task_create_fn_t) minesweeper_create }
},
g_tests_tasks[] =
{
#ifdef WUSS_ICONBAR
  { "Icon Bar",    (task_create_fn_t) iconbar_demo_create },
#endif
  { "Icons",       (task_create_fn_t) icons_create       },
  { "Keys",        (task_create_fn_t) keys_create        },
  { "Porter-Duff", (task_create_fn_t) porter_duff_create },
  { "Text",        (task_create_fn_t) text_create        }
},
g_utilities_tasks[] =
{
  { "Chars",       (task_create_fn_t) chars_create       },
  { "Clock",       (task_create_fn_t) clock_create       }
},
g_visuals_tasks[] =
{
  { "Checker",     (task_create_fn_t) checker_create     },
  { "Curve",       (task_create_fn_t) curve_create       },
  { "Dithering",   (task_create_fn_t) dithering_create   },
  { "Effects",     (task_create_fn_t) effects_create     },
  { "Gradient",    (task_create_fn_t) gradient_create    },
  { "Greeble",     (task_create_fn_t) greeble_create     },
  { "Image",       (task_create_fn_t) image_create       },
  { "Lissajous",   (task_create_fn_t) lissajous_create   },
  { "Patterns",    (task_create_fn_t) patterns_create    },
  { "Tiling",      (task_create_fn_t) tiling_create      }
},
g_toys_tasks[] =
{
  { "Doughnut",    (task_create_fn_t) doughnut_create    },
  { "Particles",   (task_create_fn_t) particles_create   },
  { "Saturn",      (task_create_fn_t) saturn_create      },
  { "Sofa",        (task_create_fn_t) sofa_create        },
  { "Spheroid",    (task_create_fn_t) spheroid_create    }
},
g_system_tasks[] =
{
  { "Configure",   (task_create_fn_t) config_create      },
  { "Display",     (task_create_fn_t) display_create     },
  { "Palette",     (task_create_fn_t) palette_create     },
#ifdef WUSS_ICONBAR
  { "Filer",       (task_create_fn_t) filer_create        }
#endif
};

/* Driver debugging aids, picked by index in task_handle_event -- keep in
 * step with the switch there */
enum
{
  DEBUG_ITEM_REDRAW,
  DEBUG_ITEM_GARBAGE,
  DEBUG_ITEM_PIXEL_STRESS
};

static const wuss_menu_item_t g_debug_items[] =
{
  { "Redraw",           wuss_MENU_ITEM_NONE, NULL, NULL, 0, "F1"                 },
  { "Garbage",          wuss_MENU_ITEM_NONE, NULL, NULL, 0, WUSS_MENU_SHIFT "F1" },
  { "Pixel Stress",     wuss_MENU_ITEM_NONE, NULL, NULL, 0, "F3"                 }
};

static const wuss_menu_t g_debug_menu =
{
  "Debug", g_debug_items, NELEMS(g_debug_items)
};

/* Top-level backdrop menu rows, picked by index in task_handle_event. Every
 * task is launched from AppsFiler now, so only system actions remain. */
enum
{
  TASK_ITEM_INFO,
  TASK_ITEM_ZOOM_IN,
  TASK_ITEM_ZOOM_OUT,
  TASK_ITEM_CRT,
  TASK_ITEM_POINTER,
  TASK_ITEM_DEBUG
#ifndef __EMSCRIPTEN__
  ,
  TASK_ITEM_QUIT
#endif
};

/* not const: the "Info" row's .window is filled in by tasks_open_launcher,
 * retargeting the shared proginfo singleton at g_tasks.menu_task each time
 * -- the table itself cannot name it at compile time. The CRT and Software
 * pointer rows' ticks track g_tasks.crt and g_tasks.pointer. */
static wuss_menu_item_t g_task_items[] =
{
  { "Info",             wuss_MENU_ITEM_PRE_OPEN, NULL, NULL                          },
  { "Zoom In",          wuss_MENU_ITEM_DASHED,   NULL, NULL, 0, "F2"                 },
  { "Zoom Out",         wuss_MENU_ITEM_NONE,     NULL, NULL, 0, WUSS_MENU_SHIFT "F2" },
  { "CRT Effect",       wuss_MENU_ITEM_NONE,     NULL, NULL, 0, "F5"                 },
  { "Software Pointer", wuss_MENU_ITEM_NONE,     NULL, NULL, 0, "F6"                 },
  { "Debug",            wuss_MENU_ITEM_DASHED,   &g_debug_menu                       }
#ifndef __EMSCRIPTEN__
  ,
  { "Quit Wuss",        wuss_MENU_ITEM_DASHED,   NULL, NULL, 0, "F4"                 }
#endif
};

static const wuss_menu_t g_task_menu =
{
  "Wuss", g_task_items, NELEMS(g_task_items)
};

void tasks_set_crt(bool on)
{
  g_tasks.crt = wuss_frontend_set_crt(g_tasks.frontend, on);
  if (g_tasks.crt)
    g_task_items[TASK_ITEM_CRT].flags |= wuss_MENU_ITEM_TICKED;
  else
    g_task_items[TASK_ITEM_CRT].flags &= ~wuss_MENU_ITEM_TICKED;
}

void tasks_set_pointer(bool on)
{
  g_tasks.pointer = app_set_pointer(on);
  if (g_tasks.pointer)
    g_task_items[TASK_ITEM_POINTER].flags |= wuss_MENU_ITEM_TICKED;
  else
    g_task_items[TASK_ITEM_POINTER].flags &= ~wuss_MENU_ITEM_TICKED;
}

/* g_task_menu picks are dispatched by index and g_debug_menu picks likewise.
 * Every menu is opened by g.menu_task, so one handler sees every
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

  if (menu == &g_debug_menu)
  {
    switch (index)
    {
    case DEBUG_ITEM_REDRAW:
      wuss_redraw(g_tasks.wuss);
      g_tasks.debug_redraw_all = true;
      break;
    case DEBUG_ITEM_GARBAGE:
      g_tasks.debug_garbage = true;
      break;
    case DEBUG_ITEM_PIXEL_STRESS:
      g_tasks.debug_pixel_stress = true;
      break;
    default:
      break;
    }
    return result_OK;
  }

  if (menu == &g_task_menu)
  {
    switch (index)
    {
    case TASK_ITEM_ZOOM_IN:
      wuss_frontend_zoom(g_tasks.frontend, 1);
      break;
    case TASK_ITEM_ZOOM_OUT:
      wuss_frontend_zoom(g_tasks.frontend, -1);
      break;
    case TASK_ITEM_CRT:
      tasks_set_crt(!g_tasks.crt);
      /* the new surface starts empty: present the whole frame */
      g_tasks.debug_redraw_all = true;
      break;
    case TASK_ITEM_POINTER:
      tasks_set_pointer(!g_tasks.pointer);
      break;
#ifndef __EMSCRIPTEN__
    case TASK_ITEM_QUIT:
      g_tasks.quit = true;
      break;
#endif
    default:
      break;
    }
    return result_OK;
  }

  return result_OK;
}

result_t tasks_open_launcher(point_t pos)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Wuss demo", "Window manager test environment");

  wuss_proginfo_set_desc(&desc);
  g_task_items[TASK_ITEM_INFO].window = wuss_proginfo_window(g_tasks.menu_task);

  return wuss_menu_open(g_tasks.menu_task, &g_task_menu, pos, NULL);
}

result_t tasks_launcher_key(int code, wuss_key_modifiers_t modifiers)
{
  wuss_event_t key;

  key.kind               = wuss_EVENT_KEY;
  key.data.key.code      = code;
  key.data.key.modifiers = modifiers;

  return wuss_menu_dispatch_shortcut(g_tasks.menu_task, &g_task_menu, &key);
}

/* tasks_spawn walks every category's table by name, regardless of which
 * category a task lives in. */
static const struct
{
  const char         *name;
  const task_entry_t *tasks;
  int                  count;
}
g_task_categories[] =
{
  { "Games", g_games_tasks,     NELEMS(g_games_tasks)     },
  { "Tests", g_tests_tasks,     NELEMS(g_tests_tasks)     },
  { "Toys", g_toys_tasks,      NELEMS(g_toys_tasks)      },
  { "System", g_system_tasks,    NELEMS(g_system_tasks)    },
  { "Utilities", g_utilities_tasks, NELEMS(g_utilities_tasks) },
  { "Visuals", g_visuals_tasks,   NELEMS(g_visuals_tasks)   }
};

int tasks_category_count(void)
{
  return (int) NELEMS(g_task_categories);
}

const char *tasks_category_name(int category)
{
  return g_task_categories[category].name;
}

int tasks_entry_count(int category)
{
  return g_task_categories[category].count;
}

const char *tasks_entry_name(int category, int index)
{
  return g_task_categories[category].tasks[index].name;
}

result_t tasks_entry_spawn(int category, int index)
{
  return spawn_task(g_task_categories[category].tasks[index].name,
                    g_task_categories[category].tasks[index].create);
}

/* Spawns every task in every category, in table order. */
static void tasks_spawn_all(void)
{
  int c, i;

  for (c = 0; c < (int) NELEMS(g_task_categories); c++)
    for (i = 0; i < g_task_categories[c].count; i++)
      (void) spawn_task(g_task_categories[c].tasks[i].name,
                        g_task_categories[c].tasks[i].create);
}

/* Finds and spawns the single task named name (case-sensitive, matching the
 * launcher menu's spelling) across every category. Returns false, having
 * logged an error, if no category has a task by that name. */
static bool tasks_spawn_one(const char *name)
{
  int c, i;

  for (c = 0; c < (int) NELEMS(g_task_categories); c++)
    for (i = 0; i < g_task_categories[c].count; i++)
      if (strcmp(g_task_categories[c].tasks[i].name, name) == 0)
      {
        (void) spawn_task(name, g_task_categories[c].tasks[i].create);
        return true;
      }

  logf_error("wuss: -tasks: no such task \"%s\"", name);
  return false;
}

void tasks_spawn(const char *names)
{
  const char *p;

  if (strcmp(names, "all") == 0)
  {
    tasks_spawn_all();
    return;
  }

  p = names;
  while (*p != '\0')
  {
    const char *comma;
    size_t      len;
    char        name[32];

    comma = strchr(p, ',');
    len   = comma ? (size_t) (comma - p) : strlen(p);
    if (len >= sizeof(name))
      len = sizeof(name) - 1;
    memcpy(name, p, len);
    name[len] = '\0';

    if (name[0] != '\0')
      (void) tasks_spawn_one(name);

    p += len;
    if (*p == ',')
      p++;
  }
}
