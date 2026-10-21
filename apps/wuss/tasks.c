/* wuss/tasks.c -- Wuss demo task launcher and menu wiring */

#include <stdlib.h>
#include <string.h>

#include "base/debug.h"
#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "wuss/iconbar.h"
#include "wuss/task.h"
#include "wuss/wuss.h"
#include "wuss/window.h"
#include "wuss/menu.h"
#include "wuss/component/proginfo.h"

#include "frontend.h"
#include "tasks.h"

#include "tasks/ball/ball.h"
#include "tasks/pooling/pooling.h"
#include "tasks/chasehq/chasehq.h"
#include "tasks/greatescape/greatescape.h"
#include "tasks/chars/chars.h"
#include "tasks/checker/checker.h"
#include "tasks/clock/clock.h"
#include "tasks/common/common.h"
#include "tasks/configuring/configuring.h"
#include "tasks/curving/curving.h"
#include "tasks/dithering/dithering.h"
#include "tasks/display/display.h"
#include "tasks/doughnut/doughnut.h"
#include "tasks/effecting/effecting.h"
#include "tasks/filing/filing.h"
#include "tasks/gradient/gradient.h"
#include "tasks/greebling/greebling.h"
#include "tasks/icons/icons.h"
#include "tasks/resizing/resizing.h"
#include "tasks/viewing/viewing.h"
#include "tasks/fern/fern.h"
#include "tasks/keys/keys.h"
#include "tasks/oscillating/oscillating.h"
#include "tasks/minesweeping/minesweeping.h"
#include "tasks/painting/painting.h"
#include "tasks/palette/palette.h"
#include "tasks/fireworks/fireworks.h"
#include "tasks/patterns/patterns.h"
#include "tasks/compositing/compositing.h"
#include "tasks/saturn/saturn.h"
#include "tasks/shortcuts/shortcuts.h"
#include "tasks/sofa/sofa.h"
#include "tasks/sudoku/sudoku.h"
#include "tasks/orbing/orbing.h"
#include "tasks/reading/reading.h"
#include "tasks/common/taskapp.h"
#include "tasks/tiling/tiling.h"

/* ----------------------------------------------------------------------- */

struct wuss_app_tasks g_tasks;

void tasks_build_screen_palette(colour_t       *out,
                                int             nout,
                                const colour_t *ui,
                                int             nui)
{
  int r, g, b, n;

  /* 1bpp and 2bpp screens can't hold the UI colours, so they get a fixed
   * grey ramp instead and the UI colours match onto it; so does 8bpp in
   * grey mode */
  if (nout == 2 || nout == 4 || (nout == 256 && g_tasks.grey))
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

/* One launchable task: g_*_tasks[i] below. install adds the task's app to
 * the icon bar (its X_app_create) without opening any window. */
typedef result_t (*task_install_fn_t)(wuss_t *wuss);

typedef struct task_entry
{
  const char       *name;
  task_install_fn_t install;
}
task_entry_t;

static result_t spawn_task(task_entry_t *entry)
{
  result_t rc;

  rc = entry->install(g_tasks.wuss);
  if (rc != result_OK)
    logf_error("wuss: %s install failed, rc=0x%X (%s)", entry->name, rc,
               result_string(rc));
  return rc;
}

/* The Wuss menu (opened from the TaskManager icon) holds only system actions;
 * every task is launched from AppsFiler, which walks g_task_categories
 * below. */


static task_entry_t
g_games_tasks[] =
{
  { "Ball", ball_app_create },
#ifdef WUSS_CHASEHQ
  { "Chase H.Q.", chasehq_app_create },
#endif
#ifdef WUSS_GREATESCAPE
  { "The Great Escape", greatescape_app_create },
#endif
  { "Minesweeping", minesweeping_app_create },
  { "Sudoku", sudoku_app_create }
},
g_image_tasks[] =
{
  { "Dithering", dithering_app_create },
  { "Effecting", effecting_app_create },
  { "Painting", painting_app_create },
  { "Resizing", resizing_app_create },
  { "Viewing", viewing_app_create },
  { "Tiling", tiling_app_create }
},
g_tests_tasks[] =
{
  { "Icons", icons_app_create },
  { "Keys", keys_app_create },
  { "Compositing", compositing_app_create }
},
g_text_tasks[] =
{
  { "Reading", reading_app_create }
},
g_utilities_tasks[] =
{
  { "Chars", chars_app_create },
  { "Clock", clock_app_create },
  { "Pooling", pooling_app_create },
  { "Shortcuts", shortcuts_app_create }
},
g_visuals_tasks[] =
{
  { "Checker", checker_app_create },
  { "Curving", curving_app_create },
  { "Gradient", gradient_app_create },
  { "Greebling", greebling_app_create },
  { "Oscillating", oscillating_app_create },
  { "Patterns", patterns_app_create }
},
g_toys_tasks[] =
{
  { "Doughnut", doughnut_app_create },
  { "Fern", fern_app_create },
  { "Fireworks", fireworks_app_create },
  { "Saturn", saturn_app_create },
  { "Sofa", sofa_app_create },
  { "Orbing", orbing_app_create }
},
g_system_tasks[] =
{
  { "Configuring", configuring_app_create },
  { "Display", display_app_create },
  { "Palette", palette_app_create },
#ifdef WUSS_ICONBAR
  { "Filing", filing_app_create }
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
  { "Redraw",           wuss_MENU_ITEM_NONE, NULL, NULL, 0, WUSS_MENU_SHIFT "F1"     },
  { "Garbage",          wuss_MENU_ITEM_NONE, NULL, NULL, 0, "^" WUSS_MENU_SHIFT "F1" },
  { "Pixel Stress",     wuss_MENU_ITEM_NONE, NULL, NULL, 0, WUSS_MENU_SHIFT "F3"     }
};

static const wuss_menu_t g_debug_menu =
{
  "Debug", g_debug_items, NELEMS(g_debug_items)
};

/* Top-level Wuss menu rows, picked by index in task_handle_event. Every
 * task is launched from AppsFiler now, so only system actions remain. */
enum
{
  TASK_ITEM_INFO,
  TASK_ITEM_DEBUG
#ifndef __EMSCRIPTEN__
  ,
  TASK_ITEM_QUIT
#endif
};

/* not const: the "Info" row's .window is filled in by tasks_open_launcher,
 * retargeting the shared proginfo singleton at g_tasks.menu_task each time
 * -- the table itself cannot name it at compile time. */
static wuss_menu_item_t g_task_items[] =
{
  { "Info",             wuss_MENU_ITEM_PRE_OPEN, NULL, NULL                          },
  { "Debug",            wuss_MENU_ITEM_DASHED,   &g_debug_menu                       }
#ifndef __EMSCRIPTEN__
  ,
  { "Quit Wuss",        wuss_MENU_ITEM_DASHED,   NULL, NULL, 0, "^" WUSS_MENU_SHIFT "Q" }
#endif
};

static const wuss_menu_t g_task_menu =
{
  "Wuss", g_task_items, NELEMS(g_task_items)
};

void tasks_set_crt(bool on)
{
  g_tasks.crt = wuss_frontend_set_crt(g_tasks.frontend, on);
}

void tasks_set_pointer(bool on)
{
  g_tasks.pointer = app_set_pointer(on);
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

#ifdef WUSS_ICONBAR
result_t tasks_open_menu(void)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Wuss", "Graphical User Interface");

  wuss_proginfo_set_desc(&desc);
  g_task_items[TASK_ITEM_INFO].window = wuss_proginfo_window(g_tasks.menu_task);

  return wuss_iconbar_menu_open(g_tasks.menu_task, &g_task_menu, NULL);
}
#endif

int tasks_system_key(int code, wuss_key_modifiers_t modifiers)
{
  wuss_window_t *focus;
  wuss_task_t   *task;
  int            shift;
  int            ctrl;

  if (modifiers & wuss_KEY_MOD_ALT)
    return 0;

  focus = wuss_get_focus(g_tasks.wuss);
  task  = (focus != NULL) ? wuss_window_get_task(focus) : NULL;
  shift = (modifiers & wuss_KEY_MOD_SHIFT) != 0;
  ctrl  = (modifiers & wuss_KEY_MOD_CTRL) != 0;

  /* F7 next / Shift-F7 previous window, F10 / Shift-F10 app, F11 hide,
   * Shift-F11 hide others, Ctrl-F11 show all, Ctrl-F10 app to front,
   * Ctrl-F9 quit app: the function-key forms of the chords below */
  if (code == wuss_KEY_F1 + 6 && !ctrl)
  {
    if (task != NULL)
      (void) wuss_task_cycle_windows(g_tasks.wuss, task, shift ? -1 : 1);
    return 1;
  }

  if (code == wuss_KEY_F1 + 9)
  {
    if (ctrl)
    {
      if (task != NULL)
        (void) wuss_task_front(g_tasks.wuss, task);
    }
    else
    {
      (void) wuss_cycle_tasks(g_tasks.wuss, shift ? -1 : 1);
    }
    return 1;
  }

  if (code == wuss_KEY_F1 + 10)
  {
    if (ctrl)
      (void) wuss_task_show_all(g_tasks.wuss);
    else if (task != NULL)
      (void) (shift ? wuss_task_hide_others(g_tasks.wuss, task)
                    : wuss_task_hide_app(g_tasks.wuss, task));
    return 1;
  }

  if (code == wuss_KEY_F1 + 8 && ctrl && !shift)
  {
    if (task != NULL)
      (void) wuss_task_close_windows(g_tasks.wuss, task);
    return 1;
  }

  if (!ctrl)
    return 0;

  /* Ctrl-` / Ctrl-Shift-`: next / previous window of the app. Shift-` types
   * ~ on a US keyboard. Ctrl-Tab / Ctrl-Shift-Tab: next / previous app.
   * Ctrl-H hides the app, Ctrl-Shift-H the others. */
  if (code == '`' || code == '~')
  {
    if (task != NULL)
      (void) wuss_task_cycle_windows(g_tasks.wuss, task,
                                     (shift || code == '~') ? -1 : 1);
    return 1;
  }

  if (code == 9)
  {
    (void) wuss_cycle_tasks(g_tasks.wuss, shift ? -1 : 1);
    return 1;
  }

  /* Ctrl-Shift-Esc: kill the focused app, without unsaved-data prompts */
  if (code == wuss_KEY_ESCAPE && shift)
  {
    if (task != NULL)
      wuss_task_destroy(wuss_task_get_root(task));
    return 1;
  }

  /* Ctrl-Shift-F: toggle the focused window's size */
  if ((code == 'f' || code == 'F') && shift)
  {
    wuss_window_toggle_size(focus);
    return 1;
  }

  if (code == 'h' || code == 'H')
  {
    if (task != NULL)
      (void) (shift ? wuss_task_hide_others(g_tasks.wuss, task)
                    : wuss_task_hide_app(g_tasks.wuss, task));
    return 1;
  }

  /* Ctrl-F2: close window, as its close icon does */
  if (code == wuss_KEY_F1 + 1 && !(modifiers & wuss_KEY_MOD_SHIFT))
  {
    (void) wuss_window_try_close(focus);
    return 1;
  }

  if (code == 'q' || code == 'Q')
  {
#ifndef __EMSCRIPTEN__
    /* Ctrl-Shift-Q: quit Wuss */
    if (modifiers & wuss_KEY_MOD_SHIFT)
    {
      g_tasks.quit = true;
      return 1;
    }
#endif

    /* Ctrl-Q: quit the focused app, with unsaved-data prompts */
    if (focus != NULL)
      (void) wuss_task_close_windows(g_tasks.wuss, wuss_window_get_task(focus));
    return 1;
  }

  return 0;
}

result_t tasks_launcher_key(int code, wuss_key_modifiers_t modifiers)
{
  wuss_event_t key;

  /* zoom stays on Ctrl-= / Ctrl--, now that the menu rows are gone; the CRT
   * and pointer options live only in the Display window, whose ticks a key
   * could not keep current. Ctrl-Plus is Ctrl-Shift-= on most keyboards. */
  if ((modifiers & wuss_KEY_MOD_CTRL) &&
      (code == '=' || code == '+' || code == '-'))
  {
    wuss_frontend_zoom(g_tasks.frontend, code == '-' ? -1 : 1);
    return result_OK;
  }

  /* Ctrl-0: actual size */
  if ((modifiers & wuss_KEY_MOD_CTRL) && code == '0')
  {
    while (wuss_frontend_get_zoom(g_tasks.frontend) > 1)
      wuss_frontend_zoom(g_tasks.frontend, -1);
    return result_OK;
  }

  key.kind               = wuss_EVENT_KEY;
  key.data.key.code      = code;
  key.data.key.modifiers = modifiers;

  return wuss_menu_dispatch_shortcut(g_tasks.menu_task, &g_task_menu, &key);
}

/* tasks_spawn walks every category's table by name, regardless of which
 * category a task lives in. */
static struct
{
  const char   *name;
  task_entry_t *tasks;
  int           count;
}
g_task_categories[] =
{
  { "Games",     g_games_tasks,     NELEMS(g_games_tasks)     },
  { "Image",     g_image_tasks,     NELEMS(g_image_tasks)     },
  { "Tests",     g_tests_tasks,     NELEMS(g_tests_tasks)     },
  { "Toys",      g_toys_tasks,      NELEMS(g_toys_tasks)      },
  { "System",    g_system_tasks,    NELEMS(g_system_tasks)    },
  { "Text",      g_text_tasks,      NELEMS(g_text_tasks)      },
  { "Utilities", g_utilities_tasks, NELEMS(g_utilities_tasks) },
  { "Visuals",   g_visuals_tasks,   NELEMS(g_visuals_tasks)   }
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
  return spawn_task(&g_task_categories[category].tasks[index]);
}

/* Spawns every task in every category, in table order. */
static void tasks_spawn_all(void)
{
  int c, i;

  for (c = 0; c < (int) NELEMS(g_task_categories); c++)
    for (i = 0; i < g_task_categories[c].count; i++)
      (void) spawn_task(&g_task_categories[c].tasks[i]);
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
        (void) spawn_task(&g_task_categories[c].tasks[i]);
        return true;
      }

  logf_error("wuss: -tasks: no such task \"%s\"", name);
  return false;
}

/* The app that opens each filetype when no running app claims a file's
 * DataOpen: wuss's Alias$@RunType_XXX. Names are as in the tables above. */
static const struct
{
  unsigned short filetype;
  const char    *name;
}
g_run_types[] =
{
  { filetype_RISCOS_PNG,      "Viewing" },
  { filetype_RISCOS_TEXT,     "Reading" },
  { filetype_RISCOS_MARKDOWN, "Reading" }
};

result_t tasks_run_type(filetype_t type)
{
  size_t i;

  for (i = 0; i < NELEMS(g_run_types); i++)
    if (g_run_types[i].filetype == type.riscos)
      return tasks_spawn_one(g_run_types[i].name) ? result_OK
                                                   : result_NOT_FOUND;

  return result_NOT_FOUND;
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

void tasks_reap_apps(void)
{
  taskapp_reap();
}
