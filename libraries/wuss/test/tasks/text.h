/* wuss/test/tasks/text.h -- sample-text task */

#ifndef TASKS_TEXT_H
#define TASKS_TEXT_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/fontmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* one styled run within a task's markdown_text, produced by
 * text__markdown_parse: [start, start+len) is drawn in colour instead of
 * the paragraph's normal foreground. Runs never overlap and are sorted by
 * start. */
typedef struct text_span
{
  int      start;
  int      len;
  colour_t colour;
}
text_span_t;

#define TEXT_MAX_SAMPLES 16 /* files in resources/text beyond this are
                             * ignored */

/* one row of the "Sample" submenu, found in resources/text */
typedef struct text_sample
{
  char leaf[64];  /* leafname, for loading */
  char name[64];  /* menu label: leaf less extension, '_' shown as ' ' */
  bool markdown;  /* leaf is ".md": parse with text__markdown_parse */
}
text_sample_t;

/* window B's task: flows a chosen sample string over its wuss-filled
 * background, one line per bmfont_draw call. A MENU click on the window opens
 * a top-level menu with three submenus -- "Font" (the shared wuss_fontmenu
 * singleton over resources/bmfonts, swapping the paragraph font in place),
 * "Sample" (swaps the shown string: one row per file in resources/text) and "Spacing" (swaps the letter/word spacing) and "Colours" (a
 * submenu of "Foreground"/"Background", each retargeting the shared
 * wuss_colourmenu singleton on hover) -- whose "Background" row also offers
 * a "None" chip that unsets the paragraph's background colour so glyphs
 * blend straight onto whatever is already behind the window's content. */
typedef struct text_task
{
  wuss_window_t      *window;
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
                                   * text_pre_submenu_open */
  text_sample_t       samples[TEXT_MAX_SAMPLES]; /* sorted by leaf */
  int                 nsamples;   /* used entries of samples[] */
  wuss_menu_item_t    sample_items[TEXT_MAX_SAMPLES]; /* "Sample" submenu
                                   * rows, one per samples[]; per-instance
                                   * so ticks/selection state can't bleed
                                   * across two Text windows */
  wuss_menu_t         sample_menu;
  wuss_menu_item_t    spacing_items[4]; /* "Spacing" submenu rows; per-
                                   * instance for the same reason -- each
                                   * carries its own tick state */
  wuss_menu_t         spacing_menu;
  wuss_menu_item_t    colours_items[2]; /* "Foreground"/"Background" rows;
                                   * both share the colourmenu singleton,
                                   * retargeted per hover in
                                   * text_pre_submenu_open */
  wuss_menu_t         colours_menu;
  wuss_menu_item_t    top_items[6]; /* "Info", "Font", "Sample", "Spacing",
                                   * "Colours", "Auto-size", built once the fontmenu
                                   * exists so items[1].submenu can borrow
                                   * its live wuss_menu_t */
  wuss_menu_t         top_menu;   /* root menu passed to wuss_menu_open */
  wuss_menu_handle_t  menu_handle; /* chain handle from the last
                                   * wuss_menu_open, for the _live tick calls
                                   * when an ADJUST pick keeps the chain
                                   * open; NULL if closed */
  int                 sample;     /* index into samples[] currently shown,
                                   * or -1 before the first is loaded */
  char               *text;       /* owned; what text_redraw lays out and
                                   * draws: samples[sample]'s file contents,
                                   * stripped of Markdown markers if
                                   * samples[sample].markdown is set */
  text_span_t        *markdown_spans; /* owned array of styled runs into
                                   * text, from text__markdown_parse; NULL
                                   * when the current sample isn't
                                   * Markdown */
  int                 markdown_nspans; /* length of markdown_spans */
  int                 spacing_idx; /* index into text_spacing_presets[]
                                   * currently applied */
  bmfont_spacing_t    spacing;    /* text_spacing_presets[spacing_idx],
                                   * passed to bmtext_layout/bmtext_draw */
  wuss_colour_t       fg_index;   /* system palette index picked from
                                   * "Foreground" */
  wuss_colour_t       bg_index;   /* system palette index picked from
                                   * "Background" */
  bool                bg_transparent; /* set from the "Background" menu's
                                   * "None" row; bg drawn transparent while
                                   * set, overriding bg_index */
  int                 base_width;  /* content width when the window was
                                   * made */
  int                 base_height; /* content height when the window was
                                   * made */
  int                 frame_count;
  bool                resizing;    /* off by default; toggled by the
                                   * "Auto-size" menu row or a content
                                   * click. text_idle only resizes the
                                   * window while this is true */
}
text_task_t;

wuss_window_fn_t text_handle;

/* create the sample-text window against the given wuss instance; the font
 * picker loads bmfonts from wuss_get_resources(wuss)/resources/bmfonts/
 * <name>.png. if out is non-NULL, the task block is also returned through
 * it */
result_t text_create(wuss_t *wuss, text_task_t **out);

/* free a task block allocated by text_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void text_destroy(text_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_TEXT_H */
