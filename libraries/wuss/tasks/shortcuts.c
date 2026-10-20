/* wuss/tasks/shortcuts.c -- system keyboard shortcut list task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"

#include "shortcuts.h"
#include "taskapp.h"

#define SHORTCUTS_LABEL_CHARS 20 /* label column width */
#define SHORTCUTS_COLUMNS      2 /* keys, then action */
#define SHORTCUTS_ROWS        16 /* window opens at most this many tall */

/* The chords handled ahead of any window's task: tasks_system_key, the main
 * loop's key case and tasks_launcher_key in apps/wuss, the core's drag and
 * menu cancels, and the writable field keys in wuss__writable_key. Keep in
 * step with those. */
static const struct
{
  const char *keys;
  const char *action;
}
shortcuts[] =
{
  { "F7",             "Next window"         },
  { "Shift-F7",       "Previous window"     },
  { "Ctrl-`",         "Next window"         },
  { "Ctrl-Shift-`",   "Previous window"     },
  { "F10",            "Next app"            },
  { "Shift-F10",      "Previous app"        },
  { "Ctrl-Tab",       "Next app"            },
  { "Ctrl-Shift-Tab", "Previous app"        },
  { "Ctrl-F10",       "App to front"        },
  { "F11",            "Hide app"            },
  { "Shift-F11",      "Hide other apps"     },
  { "Ctrl-H",         "Hide app"            },
  { "Ctrl-Shift-H",   "Hide other apps"     },
  { "Ctrl-F11",       "Show all apps"       },
  { "Ctrl-Shift-F",   "Toggle window size"  },
  { "Ctrl-F2",        "Close window"        },
  { "Ctrl-F9",        "Quit app"            },
  { "Ctrl-Q",         "Quit app"            },
  { "Ctrl-Shift-Esc", "Kill app"            },
#ifndef __EMSCRIPTEN__
  { "Ctrl-Shift-Q",   "Quit Wuss"           },
  { "Ctrl-Shift-3",   "Save screenshot"     },
#endif
  { "Ctrl-=",         "Zoom in"             },
  { "Ctrl--",         "Zoom out"            },
  { "Ctrl-0",         "Actual size"         },
  { "Escape",         "Cancel drag"         },
  { "Escape",         "Close menu"          },
  { "Ctrl-A",         "Select all text"     },
  { "Ctrl-C",         "Copy text"           },
  { "Ctrl-X",         "Cut text"            },
  { "Ctrl-V",         "Paste text"          },
  { "Insert",         "Paste text"          },
  { "Ctrl-Z",         "Clear selection"     },
  { "Ctrl-U",         "Clear field"         },
  { "Tab",            "Next field"          },
  { "Shift-Tab",      "Previous field"      },
  { "Shift-Left",     "Word left"           },
  { "Shift-Right",    "Word right"          },
  { "Ctrl-Left",      "Start of field"      },
  { "Ctrl-Right",     "End of field"        },
  { "Shift-F1",       "Debug: Redraw"       },
  { "Ctrl-Shift-F1",  "Debug: Garbage"      },
  { "Shift-F3",       "Debug: Pixel stress" }
};

/* wuss_gridview_item_fn_t: even cells are keys, odd cells actions */
static void shortcuts_item(int                   index,
                           void                 *opaque,
                           wuss_gridview_item_t *out)
{
  NOT_USED(opaque);

  out->glyph = NULL;
  out->label = (index & 1) ? shortcuts[index / 2].action
                           : shortcuts[index / 2].keys;
}

result_t shortcuts_create(wuss_t *wuss, shortcuts_task_t **out)
{
  result_t          rc;
  shortcuts_task_t *task;
  wuss_task_t      *delegate;
  wuss_task_desc_t  delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;

  delegate_desc.handle    = shortcuts_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "shortcuts";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_gridview_window_create(&task->gv, delegate, NULL, "Shortcuts",
                                   (int) NELEMS(shortcuts) * 2,
                                   shortcuts_item, SHORTCUTS_LABEL_CHARS,
                                   NULL, task, 0, &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* its QUIT frees the task block */
    return rc;
  }

  /* one or two columns, so each pair keeps to a row when there is room */
  wuss_gridview_set_layout(task->gv, SHORTCUTS_COLUMNS, 1, 0);
  wuss_gridview_set_draw(task->gv, NULL, 0);
  wuss_gridview_fit_window(task->gv, SHORTCUTS_COLUMNS, SHORTCUTS_ROWS);

  if (out)
    *out = task;

  return result_OK;
}

result_t shortcuts_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Shortcuts",
    "System keyboard shortcut list",
    (taskapp_create_fn_t) shortcuts_create
  };

  return taskapp_install(wuss, &desc);
}

void shortcuts_destroy(shortcuts_task_t *task)
{
  wuss_gridview_destroy(task->gv);
  free(task);
}

result_t shortcuts_handle(wuss_window_t      *window,
                          const wuss_event_t *event,
                          void               *task_data)
{
  shortcuts_task_t *st;

  st = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_CLOSE:
    if (window == st->window)
      st->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    shortcuts_destroy(st);
    return result_OK;

  default:
    if (st->gv != NULL)
      (void) wuss_gridview_handle_event(st->gv, event);
    return result_OK;
  }
}

#endif /* WUSS_APP */
