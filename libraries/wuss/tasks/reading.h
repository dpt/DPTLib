/* wuss/tasks/reading.h -- Reading task */

#ifndef TASKS_READING_H
#define TASKS_READING_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/fontmenu.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* one styled run within a task's markdown_text, produced by
 * reading__markdown_parse: [start, start+len) is drawn in colour instead of
 * the paragraph's normal foreground. Runs never overlap and are sorted by
 * start. */
typedef struct reading_span
{
  int      start;
  int      len;
  colour_t colour;
}
reading_span_t;

/* window B's task: flows a text file dropped on its icon or window
 * (DataLoad) over its wuss-filled background, one line per bmfont_draw call.
 * A MENU click on the window opens a top-level menu with three submenus --
 * "Font" (the shared wuss_fontmenu singleton over resources/bmfonts,
 * swapping the paragraph font in place),
 * "Spacing" (swaps the letter/word spacing), "Alignment" (Left, Centre or
 * Right per line) and "Colours" (a submenu of "Foreground"/"Background",
 * each retargeting the shared wuss_colourmenu singleton on hover) -- whose
 * "Background" row also offers a "None" chip that unsets the paragraph's
 * background colour so glyphs blend straight onto whatever is already
 * behind the window's content. */
typedef struct reading_task
{
  wuss_window_t      *window;
  int                 doc_wrap;    /* what the document extent was last */
  bmfont_t           *doc_font;    /* laid out for; reading_idle redoes it */
  int                 doc_spacing; /* when any of these change */
  const char         *doc_text;
  wuss_task_t        *delegate;   /* the wuss task backing this window */
  wuss_t             *wuss;       /* for wuss_get_pointer when opening the
                                   * menu */
  bmfont_t           *font;       /* currently shown; fonts[current] or
                                   * sysfont */
  bmfont_t          **fonts;      /* one slot per menu item, lazily loaded */
  int                 nfonts;     /* length of fonts[]; == menu item count */
  int                 current;    /* index into fonts[], or -1 for sysfont */
  wuss_colour_t      *colourmenu_target; /* &task->fg_index or
                                   * &task->bg_index: which field the open
                                   * colourmenu picks into, set by
                                   * reading_pre_submenu_open */
  wuss_menu_item_t    spacing_items[4]; /* "Spacing" submenu rows; per-
                                   * instance for the same reason -- each
                                   * carries its own tick state */
  wuss_menu_t         spacing_menu;
  wuss_menu_item_t    align_items[3]; /* "Alignment" submenu rows: Left,
                                   * Centre, Right */
  wuss_menu_t         align_menu;
  int                 align;      /* index into align_items[] in effect */
  wuss_menu_item_t    colours_items[2]; /* "Foreground"/"Background" rows;
                                   * both share the colourmenu singleton,
                                   * retargeted per hover in
                                   * reading_pre_submenu_open */
  wuss_menu_t         colours_menu;
  wuss_menu_item_t    top_items[4]; /* "Font", "Spacing", "Alignment",
                                   * "Colours", built once the
                                   * fontmenu exists so items[0].submenu can
                                   * borrow its live wuss_menu_t */
  wuss_menu_t         top_menu;   /* root menu passed to wuss_menu_open */
  wuss_menu_handle_t  menu_handle; /* chain handle from the last
                                   * wuss_menu_open, for the _live tick calls
                                   * when an ADJUST pick keeps the chain
                                   * open; NULL if closed */
  char               *text;       /* owned; what reading_redraw lays out and
                                   * draws: the dropped file's contents,
                                   * stripped of Markdown markers if it is
                                   * ".md"; NULL until the first DataLoad */
  reading_span_t        *markdown_spans; /* owned array of styled runs into
                                   * text, from reading__markdown_parse; NULL
                                   * when the current file isn't
                                   * Markdown */
  int                 markdown_nspans; /* length of markdown_spans */
  int                 spacing_idx; /* index into reading_spacing_presets[]
                                   * currently applied */
  bmfont_spacing_t    spacing;    /* reading_spacing_presets[spacing_idx],
                                   * passed to bmtext_layout/bmtext_draw */
  wuss_colour_t       fg_index;   /* system palette index picked from
                                   * "Foreground" */
  wuss_colour_t       bg_index;   /* system palette index picked from
                                   * "Background" */
  bool                bg_transparent; /* set from the "Background" menu's
                                   * "None" row; bg drawn transparent while
                                   * set, overriding bg_index */
}
reading_task_t;

wuss_window_fn_t reading_handle;

/* create an empty Reading window against the given wuss instance; it shows
 * text once a file is dropped on it. the font picker loads bmfonts from
 * wuss_get_resources(wuss)/resources/bmfonts/<family>/<style>.png. if out is
 * non-NULL, the task block is also returned through it */
result_t reading_create(wuss_t *wuss, reading_task_t **out);

/* install the icon bar app that opens Reading windows on demand */
result_t reading_app_create(wuss_t *wuss);

/* open a Reading window showing the file at path; the app must be installed
 * (reading_app_create) first */
result_t reading_open_file(wuss_t *wuss, const char *path);

/* free a task block allocated by reading_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void reading_destroy(reading_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_READING_H */
