/* wuss/tasks/shortcuts/shortcuts.c -- system keyboard shortcut list task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "wuss/icon.h"

#include "shortcuts.h"
#include "../common/taskapp.h"
#include "../common/common.h"

#define SHORTCUTS_LABEL_CHARS 20 /* label column width */
#define SHORTCUTS_COLUMNS      2 /* keys, then action */
#define SHORTCUTS_ROWS        16 /* window opens at most this many tall */

/* The chords handled ahead of any window's task: tasks_system_key, the main
 * loop's key case and tasks_launcher_key in apps/wuss, the core's drag and
 * menu cancels, and the writable field keys in wuss__writable_key. Keep in
 * step with those. A row with no action is a group heading. */
static const struct
{
  const char *keys;
  const char *action;
}
shortcuts[] =
{
  { "Windows",        NULL                  },
  { "F7",             "Next window"         },
  { "Shift-F7",       "Previous window"     },
  { "Ctrl-`",         "Next window"         },
  { "Ctrl-Shift-`",   "Previous window"     },
  { "Ctrl-Shift-F",   "Toggle window size"  },
  { "Ctrl-F2",        "Close window"        },
  { "Apps",           NULL                  },
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
  { "Ctrl-F9",        "Quit app"            },
  { "Ctrl-Q",         "Quit app"            },
  { "Ctrl-Shift-Esc", "Kill app"            },
  { "Wuss",           NULL                  },
#ifndef __EMSCRIPTEN__
  { "Ctrl-Shift-Q",   "Quit Wuss"           },
  { "Ctrl-Shift-3",   "Save screenshot"     },
#endif
  { "Ctrl-=",         "Zoom in"             },
  { "Ctrl--",         "Zoom out"            },
  { "Ctrl-0",         "Actual size"         },
  { "Escape",         "Cancel drag"         },
  { "Escape",         "Close menu"          },
  { "Clipboard",      NULL                  },
  { "Ctrl-A",         "Select all text"     },
  { "Ctrl-C",         "Copy text"           },
  { "Ctrl-X",         "Cut text"            },
  { "Ctrl-V",         "Paste text"          },
  { "Insert",         "Paste text"          },
  { "Field",          NULL                  },
  { "Ctrl-Z",         "Clear selection"     },
  { "Ctrl-U",         "Clear field"         },
  { "Tab",            "Next field"          },
  { "Shift-Tab",      "Previous field"      },
  { "Caret",          NULL                  },
  { "Shift-Left",     "Word left"           },
  { "Shift-Right",    "Word right"          },
  { "Ctrl-Left",      "Start of field"      },
  { "Ctrl-Right",     "End of field"        },
  { "Debug",          NULL                  },
  { "Shift-F1",       "Redraw"              },
  { "Ctrl-Shift-F1",  "Garbage"             },
  { "Shift-F3",       "Pixel stress"        }
};

/* wuss_gridview_item_fn_t: even cells are keys, odd cells actions; used
 * only to measure the cell, since shortcuts_draw_cell does the drawing */
static void shortcuts_item(int                   index,
                           void                 *opaque,
                           wuss_gridview_item_t *out)
{
  NOT_USED(opaque);

  out->glyph = NULL;
  out->label = (index & 1) ? shortcuts[index / 2].action
                           : shortcuts[index / 2].keys;
}

/* the system palette's nearest entry to an RGB value */
static colour_t shortcuts_colour(const shortcuts_task_t *task,
                                 int                     r,
                                 int                     g,
                                 int                     b)
{
  return wuss_get_palette(task->wuss, NULL)[wuss_nearest_colour(task->wuss,
                                                                r, g, b)];
}

/* wuss_gridview_draw_fn_t: the cell's text left-aligned, a group heading in
 * the bold weight (font slot 1) when one was supplied. The gridview has
 * already filled the background. */
static void shortcuts_draw_cell(wuss_gridview_t    *gv,
                                const wuss_event_t *event,
                                int                 index,
                                const box_t        *screen_box,
                                void               *opaque)
{
  shortcuts_task_t *task;
  const char       *text;
  int               slot;
  int               height, ascent;
  point_t           pos;

  NOT_USED(gv);

  task = opaque;
  text = (index & 1) ? shortcuts[index / 2].action
                     : shortcuts[index / 2].keys;
  if (text == NULL)
    return;

  slot = (shortcuts[index / 2].action == NULL &&
          wuss_get_font_n(task->wuss, 1) != NULL) ? 1 : 0;
  bmfont_get_info(wuss_get_font_n(task->wuss, slot), NULL, &height, &ascent,
                  NULL);
  pos.x = screen_box->x0 + wuss_STD_GAP;
  pos.y = screen_box->y0 + (screen_box->y1 - screen_box->y0 - height) / 2 +
          ascent;
  (void) wuss_text_draw(task->wuss, slot, event->data.redraw.scr, text,
                        (int) strlen(text),
                        shortcuts_colour(task, 0x00, 0x00, 0x00),
                        shortcuts_colour(task, 0xFF, 0xFF, 0xFF), &pos, NULL);
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

  rc = wuss_gridview_window_create(&task->gv, delegate, &task_place,
                                   "Shortcuts",
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
  wuss_gridview_set_draw(task->gv, shortcuts_draw_cell, 0);
  wuss_gridview_set_background(task->gv,
                               shortcuts_colour(task, 0xFF, 0xFF, 0xFF));
  wuss_gridview_fit_window(task->gv, SHORTCUTS_COLUMNS, SHORTCUTS_ROWS);
  (void) wuss_window_place(task->window, &task_place);

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

  case wuss_EVENT_PALETTE:
    if (st->gv != NULL)
    {
      wuss_gridview_set_background(st->gv,
                                   shortcuts_colour(st, 0xFF, 0xFF, 0xFF));
      (void) wuss_gridview_handle_event(st->gv, event);
    }
    return result_OK;

  default:
    if (st->gv != NULL)
      (void) wuss_gridview_handle_event(st->gv, event);
    return result_OK;
  }
}

#endif /* WUSS_APP */
