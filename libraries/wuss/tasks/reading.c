/* wuss/tasks/reading.c -- Reading task for Markdown and text files */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>

#include <math.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/bmfontfamily.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/point.h"
#include "io/path.h"
#include "text/bmtext.h"
#include "wuss/menu.h"
#include "wuss/message.h"
#include "wuss/component/error.h"

#include "reading.h"
#include "taskapp.h"
#include "common.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* "Colours" leads to a submenu with one row per task->fg_index/bg_index;
 * both rows share the colourmenu singleton, retargeted per hover by
 * reading_pre_submenu_open (wuss_EVENT_PRE_SUBMENU_OPEN) so one instance
 * serves either row -- see saturn.c's SATURN_COLOURS_MENU_* for the same
 * pattern. */
enum { READING_COLOURS_MENU_FOREGROUND = 0, READING_COLOURS_MENU_BACKGROUND };

/* one entry per row of the "Spacing" submenu */
typedef struct reading_spacing_preset
{
  const char *name;
  int         letter_spacing;
  int         word_spacing;
}
reading_spacing_preset_t;

static const reading_spacing_preset_t reading_spacing_presets[] =
{
  { "Normal",      0, 0 },
  { "Letter",      1, 0 },
  { "Word",        0, 1 },
  { "Letter+Word", 1, 1 }
};

#define READING_DEFAULT_SPACING 0 /* "Normal" */

/* fixed tint colours reading__markdown_parse assigns to styled runs -- palette-
 * independent since bmfont has no bold/italic glyph variants to switch to,
 * so emphasis is shown by colour instead */
#define READING_MD_HEADER_COLOUR colour_rgb(0x20, 0x60, 0xd0)
#define READING_MD_BOLD_COLOUR   colour_rgb(0xd0, 0x20, 0x20)
#define READING_MD_ITALIC_COLOUR colour_rgb(0x20, 0x90, 0x40)
#define READING_MD_CODE_COLOUR   colour_rgb(0x90, 0x40, 0xc0)

#define READING_MD_MAX_SPANS 128 /* styled runs are few; overflow is
                               * dropped */

/* is c the start of an ATX header line ("#" through "######" then a
 * space), given p points at the first "#" and is known to be at a line
 * start? advances *p past the "# " prefix and returns true if so. */
static bool reading__markdown_header_prefix(const char **p, const char *end)
{
  const char *q;

  q = *p;
  while (q < end && *q == '#')
    q++;
  if (q == *p || q >= end || *q != ' ')
    return false;

  *p = q + 1;
  return true;
}

/* scan an inline emphasis/code marker starting at *p (which must point at
 * "**", "*", "_" or "`"), find its matching close on the same line, and if
 * found append the run's text (marker stripped) to out/outlen with colour
 * span colour, advance *p past the closing marker, and return true. false
 * (with *p unchanged) if no matching close exists before newline or end. */
static bool reading__markdown_emphasis(const char    **p,
                                       const char     *end,
                                       char           *out,
                                       int            *outlen,
                                       reading_span_t *spans,
                                       int            *nspans,
                                       int             maxspans)
{
  const char *start;
  const char *marker;
  int         markerlen;
  colour_t    colour;
  const char *close;
  const char *run;
  int         runlen;

  start = *p;
  if (start[0] == '*' && start[1] == '*')
  {
    markerlen = 2;
    colour    = READING_MD_BOLD_COLOUR;
  }
  else if (start[0] == '*' || start[0] == '_')
  {
    markerlen = 1;
    colour    = READING_MD_ITALIC_COLOUR;
  }
  else /* '`' */
  {
    markerlen = 1;
    colour    = READING_MD_CODE_COLOUR;
  }
  marker = start;
  run    = start + markerlen;

  close = run;
  for (;;)
  {
    if (close + markerlen > end || *close == '\n')
      return false;
    if (memcmp(close, marker, (size_t) markerlen) == 0)
      break;
    close++;
  }

  runlen = (int) (close - run);
  memcpy(out + *outlen, run, (size_t) runlen);
  if (*nspans < maxspans)
  {
    spans[*nspans].start  = *outlen;
    spans[*nspans].len    = runlen;
    spans[*nspans].colour = colour;
    (*nspans)++;
  }
  *outlen += runlen;
  *p = close + markerlen;
  return true;
}

/* reading__markdown_parse's per-character state: which kind of line the
 * cursor is currently in. LINE_START only ever lasts for the header-prefix
 * check on the first character of a line; every other character on a line
 * is scanned in HEADER or BODY. */
typedef enum reading_md_state
{
  READING_MD_LINE_START,
  READING_MD_HEADER,
  READING_MD_BODY
}
reading_md_state_t;

/* emit the pending header span for the line running [line_start, textlen)
 * into text, if state says it's a header and the line isn't empty */
static void reading__markdown_emit_header(reading_md_state_t state,
                                          int                line_start,
                                          int                textlen,
                                          reading_span_t    *spans,
                                          int               *nspans)
{
  if (state != READING_MD_HEADER || line_start >= textlen ||
      *nspans >= READING_MD_MAX_SPANS)
    return;

  spans[*nspans].start  = line_start;
  spans[*nspans].len    = textlen - line_start;
  spans[*nspans].colour = READING_MD_HEADER_COLOUR;
  (*nspans)++;
}

/* parse Markdown source into a freshly malloc'd plain-text buffer (headers'
 * "#" prefixes and inline "**"/"*"/"_"/"`" markers stripped) plus a
 * freshly malloc'd array of styled runs (headers and inline emphasis/code)
 * into that buffer. *out_text and *out_spans are NULL on failure (OOM);
 * caller frees both with free(). Headers get a run for their whole
 * (marker-stripped) line; inline markers must close on the same line as
 * they open or are left as literal text. */
static result_t reading__markdown_parse(const char      *md,
                                        int              mdlen,
                                        char           **out_text,
                                        reading_span_t **out_spans,
                                        int             *out_nspans)
{
  char              *text;
  reading_span_t    *spans;
  int                nspans;
  int                textlen;
  const char        *p;
  const char        *end;
  reading_md_state_t state;
  int                line_start;

  text = malloc((size_t) mdlen + 1);
  if (text == NULL)
    goto oom;

  spans = malloc(READING_MD_MAX_SPANS * sizeof(*spans));
  if (spans == NULL)
  {
    free(text);
    goto oom;
  }

  nspans     = 0;
  textlen    = 0;
  p          = md;
  end        = md + mdlen;
  state      = READING_MD_LINE_START;
  line_start = 0;

  while (p < end)
  {
    switch (state)
    {
    case READING_MD_LINE_START:
      if (*p == '#' && reading__markdown_header_prefix(&p, end))
        state = READING_MD_HEADER;
      else
        state = READING_MD_BODY;
      break;

    case READING_MD_HEADER:
    case READING_MD_BODY:
      if (*p == '\n')
      {
        reading__markdown_emit_header(state, line_start, textlen, spans,
                                   &nspans);
        text[textlen++] = *p++;
        line_start      = textlen;
        state           = READING_MD_LINE_START;
      }
      else if ((*p == '*' || *p == '_' || *p == '`') &&
              reading__markdown_emphasis(&p, end, text, &textlen, spans,
                                      &nspans, READING_MD_MAX_SPANS))
      {
        /* state unchanged: stay in HEADER or BODY */
      }
      else
      {
        text[textlen++] = *p++;
      }
      break;
    }
  }

  reading__markdown_emit_header(state, line_start, textlen, spans, &nspans);

  text[textlen] = '\0';

  *out_text   = text;
  *out_spans  = spans;
  *out_nspans = nspans;
  return result_OK;

oom:
  *out_text   = NULL;
  *out_spans  = NULL;
  *out_nspans = 0;
  return result_OOM;
}

/* index into task->top_items[] of the "Font" leaf */
#define READING_MENU_FONT 0

/* index into task->top_items[] of the "Auto-size" leaf */
#define READING_MENU_AUTOSIZE 4

/* "Alignment" submenu rows */
enum { READING_ALIGN_LEFT = 0, READING_ALIGN_CENTRE, READING_ALIGN_RIGHT };

/* ----------------------------------------------------------------------- */

/* the shared fontmenu singleton, retargeted at task->wuss's bmfonts dir --
 * cheap to call repeatedly since wuss_fontmenu_menu only rebuilds when the
 * dir or wuss_t actually changes */
static const wuss_menu_t *reading_fontmenu(reading_task_t *task)
{
  const char *resources;
  const char *bmfonts_dir;

  resources   = wuss_get_resources(task->wuss);
  bmfonts_dir = pathf("%s/resources/bmfonts", resources);
  return wuss_fontmenu_menu(bmfonts_dir, "Font", task->wuss);
}

/* load fonts[idx] if not already in hand; returns it or NULL on failure.
 * name is the menu label for that row -- "Family Style". */
static bmfont_t *reading_load_font(reading_task_t *task,
                                   const char     *resources,
                                   int             idx,
                                   const char     *name)
{
  result_t    rc;
  const char *dir;
  char        filename[512];
  bmfont_t   *font;

  if (task->fonts[idx] != NULL)
    return task->fonts[idx];

  dir = pathf("%s/resources/bmfonts", resources);

  rc = bmfontfamily_label_path(dir, name, filename, sizeof(filename));
  if (rc != result_OK)
    return NULL;

  rc = bmfontcache_acquire(wuss_get_font_cache(task->wuss), filename, &font);
  if (rc != result_OK)
    return NULL;

  task->fonts[idx] = font;
  return font;
}

/* switch the paragraph to the font at menu row idx (label name) */
static result_t reading_set_font(reading_task_t *task,
                                 int             idx,
                                 const char     *name)
{
  bmfont_t *font;

  if (idx < 0 || idx >= task->nfonts || idx == task->current)
    return result_OK;

  font = reading_load_font(task, wuss_get_resources(task->wuss), idx, name);
  if (font == NULL)
    return result_OK; /* leave the current font in place */

  task->font    = font;
  task->current = idx;

  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* read the whole of path into a freshly malloc'd NUL-terminated buffer,
 * less any trailing newlines; caller frees *out_text */
static result_t reading__load_file(const char *path,
                                   char      **out_text,
                                   int        *out_len)
{
  result_t rc;
  FILE    *f;
  long     len;
  char    *text;

  *out_text = NULL;
  *out_len  = 0;

  f = fopen(path, "rb");
  if (f == NULL)
    return result_FILE_NOT_FOUND;

  rc = result_FILE_NOT_FOUND;
  if (fseek(f, 0, SEEK_END) != 0)
    goto exit;

  len = ftell(f);
  if (len < 0 || fseek(f, 0, SEEK_SET) != 0)
    goto exit;

  rc   = result_OOM;
  text = malloc((size_t) len + 1);
  if (text == NULL)
    goto exit;

  if (fread(text, 1, (size_t) len, f) != (size_t) len)
  {
    free(text);
    rc = result_FILE_NOT_FOUND;
    goto exit;
  }

  /* a NUL byte means binary data, not text */
  if (memchr(text, '\0', (size_t) len) != NULL)
  {
    free(text);
    rc = result_BAD_ARG;
    goto exit;
  }

  while (len > 0 && text[len - 1] == '\n')
    len--;
  text[len] = '\0';

  *out_text = text;
  *out_len  = (int) len;
  rc        = result_OK;

exit:
  fclose(f);
  return rc;
}

/* load the file at path into task->text, parsing it as Markdown into
 * task->text/markdown_spans if markdown is set, and freeing the previous
 * buffers; on failure the previous text stays shown */
static result_t reading__load_text(reading_task_t *task,
                                   const char     *path,
                                   bool            markdown)
{
  result_t        rc;
  char           *raw;
  int             rawlen;
  char           *text;
  reading_span_t *spans;
  int             nspans;

  rc = reading__load_file(path, &raw, &rawlen);
  if (rc != result_OK)
    return rc;

  text   = raw;
  spans  = NULL;
  nspans = 0;
  if (markdown)
  {
    rc = reading__markdown_parse(raw, rawlen, &text, &spans, &nspans);
    free(raw);
    if (rc != result_OK)
      return rc;
  }

  free(task->text);
  free(task->markdown_spans);
  task->text            = text;
  task->markdown_spans  = spans;
  task->markdown_nspans = nspans;
  return result_OK;
}

/* show the file at path (e.g. dropped from the Filer): parsed as Markdown if
 * named ".md", else shown as plain text. Binary files are refused. */
static result_t reading_show(reading_task_t *task, const char *path)
{
  result_t    rc;
  const char *leaf;
  char        name[64];
  char        message[128];
  char        glyph[TASKAPP_GLYPH_NAME_MAX];
  bool        markdown;

  leaf = path_leaf(path);
  /* ponytail: on RISC OS every leaf matches ".md", so any file is parsed as
   * Markdown -- check the filetype if that matters */
  markdown = path_leaf_strip_ext(leaf, ".md", name, sizeof(name));

  rc = reading__load_text(task, path, markdown);
  if (rc != result_OK)
  {
    logf_warning("reading: cannot load \"%s\" (rc=0x%X)", path, rc);

    if (rc == result_BAD_ARG)
      snprintf(message, sizeof(message), "%s is not a text file", leaf);
    else
      snprintf(message, sizeof(message), "Cannot read %s", leaf);
    taskapp_glyph_name("Reading", glyph);
    (void) wuss_error_report(task->wuss, glyph, "Reading", message,
                             wuss_ERROR_BUTTONS_OK, NULL, NULL);

    return rc;
  }

  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* a file dropped on the app icon opened a fresh instance: if the file will
 * not load, close that instance rather than leave an empty window */
static result_t reading_drop(reading_task_t *task, const char *path)
{
  result_t rc;

  rc = reading_show(task, path);
  if (rc != result_OK)
    wuss_task_destroy(task->delegate); /* its QUIT frees task */

  return rc;
}

/* switch the paragraph's foreground to system palette index "picked" */
static result_t reading_set_fg(reading_task_t *task, wuss_colour_t picked)
{
  task->fg_index = picked;
  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* switch the paragraph's background to system palette index "picked", or to
 * none (glyphs blend straight onto whatever is behind the window's content)
 * for wuss_NO_BACKGROUND, the colourmenu's "None" row */
static result_t reading_set_bg(reading_task_t *task, wuss_colour_t picked)
{
  task->bg_transparent = (picked == wuss_NO_BACKGROUND);
  if (!task->bg_transparent)
    task->bg_index = picked;
  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* switch letter/word spacing to reading_spacing_presets[idx] */
static result_t reading_set_spacing(reading_task_t *task, int idx)
{
  if (idx < 0 || idx >= NELEMS(reading_spacing_presets) || idx == task->spacing_idx)
    return result_OK;

  task->spacing_idx           = idx;
  task->spacing.letter_spacing = reading_spacing_presets[idx].letter_spacing;
  task->spacing.word_spacing   = reading_spacing_presets[idx].word_spacing;

  wuss_menu_tick_exclusive(&task->spacing_menu, idx);
  wuss_menu_tick_exclusive_live(task->menu_handle, &task->spacing_menu, idx);
  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* align each line to the left, centre or right of the wrap width */
static result_t reading_set_align(reading_task_t *task, int idx)
{
  if (idx < 0 || idx >= NELEMS(task->align_items) || idx == task->align)
    return result_OK;

  task->align = idx;

  wuss_menu_tick_exclusive(&task->align_menu, idx);
  wuss_menu_tick_exclusive_live(task->menu_handle, &task->align_menu, idx);
  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

static result_t reading_open_menu(reading_task_t *task)
{
  return wuss_menu_open_at_pointer(task->delegate, &task->top_menu,
                                   &task->menu_handle);
}

/* Every submenu leaf fires wuss_EVENT_PRE_SUBMENU_OPEN, not just the
 * Foreground/Background rows -- "Font"/"Spacing" are each one too
 * (their submenu never changes, so they just open unchanged). Only the
 * Foreground/Background level needs to retarget the shared colourmenu
 * singleton before opening it -- see saturn.c's saturn_pre_submenu_open for
 * the same pattern. */
static result_t reading_pre_submenu_open(reading_task_t     *task,
                                         const wuss_event_t *event)
{
  wuss_menu_handle_t handle;
  int                index;

  handle = event->data.pre_submenu_open.handle;
  index  = event->data.pre_submenu_open.index;

  if (wuss_menu_handle_menu(handle) != &task->colours_menu)
  {
    if (index == READING_MENU_FONT)
    {
      const wuss_menu_t *fontmenu;

      /* the fontmenu singleton may have been rebuilt (at a new address) by
       * another task since top_items[READING_MENU_FONT].submenu was cached in
       * reading_create -- re-fetch instead of handing the spawner a stale
       * pointer, and before ticking, as a rebuild would drop the tick */
      fontmenu = reading_fontmenu(task);
      wuss_fontmenu_set_ticked(task->current);
      return wuss_menu_open_submenu_now(handle, index, fontmenu);
    }

    return wuss_menu_open_submenu_now(handle, index,
                                      task->top_items[index].submenu);
  }

  if (index == READING_COLOURS_MENU_FOREGROUND)
  {
    task->colourmenu_target = &task->fg_index;
    wuss_colourmenu_set_none(0);
    wuss_colourmenu_set_title("Foreground");
  }
  else
  {
    task->colourmenu_target = &task->bg_index;
    wuss_colourmenu_set_none(1);
    wuss_colourmenu_set_title("Background");
  }
  wuss_colourmenu_set_ticked(*task->colourmenu_target);

  return wuss_menu_open_submenu_now(handle, index,
                                    wuss_colourmenu_menu(task->wuss));
}

/* ----------------------------------------------------------------------- */

result_t reading_create(wuss_t *wuss, reading_task_t **out)
{
  result_t           rc;
  reading_task_t    *task;
  wuss_task_t       *delegate;
  wuss_task_desc_t   delegate_desc;
  const wuss_menu_t *menu;
  const char        *sysname;
  int                i;
  size2d_t           sz;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->font        = wuss_get_font(wuss);
  task->current     = -1; /* until the system font's row is found below */
  task->spacing_idx = READING_DEFAULT_SPACING;
  task->spacing.letter_spacing = reading_spacing_presets[READING_DEFAULT_SPACING].letter_spacing;
  task->spacing.word_spacing   = reading_spacing_presets[READING_DEFAULT_SPACING].word_spacing;
  task->fg_index    = wuss_nearest_colour(wuss, 0x00, 0x00, 0x00);
  task->bg_index    = wuss_nearest_colour(wuss, 0xFF, 0xFF, 0xFF);
  task->bg_transparent = true; /* "None": blend onto the window backdrop */
  task->colourmenu_target = NULL;
  task->frame_count = 0;
  task->resizing    = false;

  /* the shared picker: every ".png" font under resources/bmfonts, sorted,
   * less any SYSTEM-class font (e.g. the one wuss draws menu ticks/arrows
   * from) */
  menu = reading_fontmenu(task);
  if (menu == NULL)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return result_OOM;
  }

  task->nfonts = menu->nitems;
  task->fonts  = calloc((size_t) task->nfonts, sizeof(*task->fonts));
  if (task->nfonts > 0 && task->fonts == NULL)
  {
    free(task);
    return result_OOM;
  }

  /* tick the system font's row, if the picker lists it. fonts[current]
   * stays NULL: the system font is wuss's, not ours to release. */
  sysname = wuss_get_font_name_n(wuss, 0);
  if (sysname != NULL)
    for (i = 0; i < task->nfonts; i++)
      if (strcmp(menu->items[i].text, sysname) == 0)
        task->current = i;

  if (wuss_colourmenu_menu(wuss) == NULL)
  {
    free(task->fonts);
    free(task);
    return result_OOM;
  }

  task->spacing_items[0].text    = "Normal";
  task->spacing_items[0].flags   = wuss_MENU_ITEM_TICKED;
  task->spacing_items[0].submenu = NULL;
  task->spacing_items[0].window  = NULL;
  task->spacing_items[1].text    = "Letter";
  task->spacing_items[1].flags   = wuss_MENU_ITEM_NONE;
  task->spacing_items[1].submenu = NULL;
  task->spacing_items[1].window  = NULL;
  task->spacing_items[2].text    = "Word";
  task->spacing_items[2].flags   = wuss_MENU_ITEM_NONE;
  task->spacing_items[2].submenu = NULL;
  task->spacing_items[2].window  = NULL;
  task->spacing_items[3].text    = "Letter+Word";
  task->spacing_items[3].flags   = wuss_MENU_ITEM_NONE;
  task->spacing_items[3].submenu = NULL;
  task->spacing_items[3].window  = NULL;

  WUSS_MENU_TITLE(task->spacing_menu, "Spacing", task->spacing_items,
                 NELEMS(task->spacing_items));

  WUSS_MENU_ITEM(task->align_items, READING_ALIGN_LEFT, "Left",
                 wuss_MENU_ITEM_TICKED);
  WUSS_MENU_ITEM(task->align_items, READING_ALIGN_CENTRE, "Centre",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->align_items, READING_ALIGN_RIGHT, "Right",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->align_menu, "Alignment", task->align_items,
                 NELEMS(task->align_items));

  /* Both rows' .submenu just need to be non-NULL to draw an arrow and
   * become hoverable; which menu they name doesn't matter since
   * reading_pre_submenu_open always supplies the menu to open (see saturn.c's
   * SATURN_COLOURS_MENU_* for the same pattern). */
  WUSS_MENU_ITEM_MENU(task->colours_items, READING_COLOURS_MENU_FOREGROUND,
                      "Foreground", wuss_MENU_ITEM_PRE_OPEN,
                      wuss_colourmenu_menu(wuss));
  WUSS_MENU_ITEM_MENU(task->colours_items, READING_COLOURS_MENU_BACKGROUND,
                      "Background", wuss_MENU_ITEM_PRE_OPEN,
                      wuss_colourmenu_menu(wuss));

  WUSS_MENU_TITLE(task->colours_menu, "Colours", task->colours_items,
                 NELEMS(task->colours_items));

  /* top-level menu: "Font" borrows the fontmenu's own live wuss_menu_t (so
   * ticks and wuss_fontmenu_selected keep working), "Spacing"/"Alignment"/
   * "Colours" are the per-instance menus built above */
  task->top_items[0].text    = "Font";
  task->top_items[0].flags   = wuss_MENU_ITEM_PRE_OPEN;
  task->top_items[0].submenu = menu;
  task->top_items[0].window  = NULL;
  task->top_items[1].text    = "Spacing";
  task->top_items[1].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[1].submenu = &task->spacing_menu;
  task->top_items[1].window  = NULL;
  task->top_items[2].text    = "Alignment";
  task->top_items[2].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[2].submenu = &task->align_menu;
  task->top_items[2].window  = NULL;
  task->top_items[3].text    = "Colours";
  task->top_items[3].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[3].submenu = &task->colours_menu;
  task->top_items[3].window  = NULL;
  task->top_items[4].text    = "Auto-size";
  task->top_items[4].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[4].submenu = NULL;
  task->top_items[4].window  = NULL;

  WUSS_MENU_TITLE(task->top_menu, "Reading", task->top_items,
                 NELEMS(task->top_items));

  delegate_desc.handle    = reading_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "reading";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task->fonts);
    free(task->text);
    free(task->markdown_spans);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  sz                = SIZE2D(220, 220);
  task->base_width  = sz.w;
  task->base_height = sz.h;

  rc = wuss_window_create_at(delegate,
                             &task_place,
                             sz,
                             "Reading",
                             wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT, /* paragraph reflows across the whole window, so a resize must redraw all of it, not just the newly (un)covered edge */
                             wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                             SIZE2D(sz.w, 220 * 4),
                             SIZE2D(0, 0),
                             &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  if (out)
    *out = task;

  return result_OK;
}

result_t reading_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Reading",
    "Read Markdown and text files",
    (taskapp_create_fn_t) reading_create,
    (taskapp_data_load_fn_t) reading_drop,
    true
  };

  return taskapp_install(wuss, &desc);
}

void reading_destroy(reading_task_t *task)
{
  int i;

  for (i = 0; i < task->nfonts; i++)
    if (task->fonts[i] != NULL)
      bmfontcache_release(wuss_get_font_cache(task->wuss), task->fonts[i]);
  free(task->fonts);
  free(task->text);
  free(task->markdown_spans);
  free(task);
}

#define READING_INSET 4
#define LEADING    2
#define MAX_LINES  64 /* text is short; overflow is dropped */

/* like bmtext_draw, but each line is split against tcx->markdown_spans (byte
 * offsets into tcx->text) so a span's run is drawn in its own colour
 * instead of fg; used only when tcx->markdown_nspans > 0. Line i starts
 * dx[i] pixels right of origin. */
static void reading__draw_styled(reading_task_t      *tcx,
                                 screen_t            *scr,
                                 const bmtext_line_t *lines,
                                 int                  nlines,
                                 const int           *dx,
                                 colour_t             fg,
                                 colour_t             bg,
                                 point_t              origin)
{
  int     font_height, font_ascent;
  point_t pos;
  int     i;

  bmfont_get_info(tcx->font, NULL, &font_height, &font_ascent, NULL);

  pos   = origin;
  pos.y += font_ascent;
  for (i = 0; i < nlines; i++)
  {
    const char *line_str;
    int         line_len;
    int         line_off; /* byte offset of line_str within tcx->text */
    int         cursor;    /* bytes of the line already drawn */

    line_str = lines[i].str;
    line_len = lines[i].len;
    line_off = (int) (line_str - tcx->text);
    cursor   = 0;
    pos.x    = origin.x + dx[i];

    while (cursor < line_len)
    {
      int                   abs_pos;
      const reading_span_t *span;
      int                   j;
      int                   run_start;
      int                   run_len;
      colour_t              run_colour;

      abs_pos = line_off + cursor;
      span    = NULL;
      for (j = 0; j < tcx->markdown_nspans; j++)
      {
        const reading_span_t *s;

        s = &tcx->markdown_spans[j];
        if (abs_pos >= s->start && abs_pos < s->start + s->len)
        {
          span = s;
          break;
        }
      }

      run_start = cursor;
      if (span != NULL)
      {
        run_len    = MIN(line_len - cursor, span->start + span->len - abs_pos);
        run_colour = span->colour;
      }
      else
      {
        /* run up to the start of the next span that begins within this
         * line, or to the end of the line if none does */
        run_len = line_len - cursor;
        for (j = 0; j < tcx->markdown_nspans; j++)
        {
          const reading_span_t *s;
          int                   rel;

          s   = &tcx->markdown_spans[j];
          rel = s->start - line_off;
          if (rel > cursor && rel - cursor < run_len)
            run_len = rel - cursor;
        }
        run_colour = fg;
      }

      bmfont_draw(tcx->font, scr, line_str + run_start, run_len, run_colour,
                 bg, &tcx->spacing, &pos, &pos);
      cursor += run_len;
    }

    pos.y += font_height + LEADING;
  }
}

/* bmtext_layout doesn't treat '\n' specially -- it wraps at spaces only, so
 * a raw call over tcx->text would swallow every newline as if it were
 * whitespace and run headers/paragraphs/list items together on one line.
 * Split text into '\n'-separated segments first and lay out each
 * separately, appending into lines[] -- a blank segment still
 * produces one empty bmtext_line_t, so blank-line paragraph gaps survive
 * as a blank output line). Returns the number of lines written, capped at
 * max. */
static int reading__layout_lines(bmfont_t               *font,
                                 const char             *text,
                                 int                     wrap_width,
                                 const bmfont_spacing_t *spacing,
                                 bmtext_line_t          *lines,
                                 int                     max)
{
  int         nlines;
  const char *seg;

  nlines = 0;
  seg    = text;

  while (nlines < max)
  {
    const char *nl;
    int         seglen;
    int         n;

    nl     = strchr(seg, '\n');
    seglen = nl != NULL ? (int) (nl - seg) : (int) strlen(seg);

    if (seglen == 0)
    {
      lines[nlines].str = seg;
      lines[nlines].len = 0;
      nlines++;
    }
    else
    {
      n = bmtext_layout(font, seg, seglen, wrap_width, spacing,
                        lines + nlines, max - nlines);
      nlines += n;
    }

    if (nl == NULL)
      break;
    seg = nl + 1;
  }

  return nlines;
}

static result_t reading_redraw(const wuss_event_t *event, void *task_data)
{
  reading_task_t *tcx;
  screen_t       *scr;
  const box_t    *bounds;
  int             sx, sy;
  const colour_t *palette;
  colour_t        fg, bg;
  int             wrap_width;
  bmtext_line_t   lines[MAX_LINES];
  int             nlines;
  int             dx[MAX_LINES];
  int             i;
  point_t         origin;
  int             font_height;

  tcx = task_data;
  if (tcx->text == NULL)
    return result_OK; /* nothing dropped yet; the backdrop shows */

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  palette = wuss_get_palette(tcx->wuss, NULL);
  fg      = palette[tcx->fg_index];
  bg      = tcx->bg_transparent ? colour_rgba(0, 0, 0, 0) : palette[tcx->bg_index];

  wrap_width = (bounds->x1 - READING_INSET) - (bounds->x0 + READING_INSET);
  nlines     = reading__layout_lines(tcx->font,
                                  tcx->text,
                                  wrap_width,
                                  &tcx->spacing,
                                  lines,
                                  MAX_LINES);

  /* per-line indent for the alignment; a line wider than the wrap width (a
   * single unbreakable word) measures as the wrap width, so it stays at the
   * left edge */
  for (i = 0; i < nlines; i++)
  {
    int            split;
    bmfont_width_t width;

    dx[i] = 0;
    if (tcx->align == READING_ALIGN_LEFT)
      continue;

    if (bmfont_measure(tcx->font, lines[i].str, lines[i].len, &tcx->spacing,
                       wrap_width, &split, &width) != result_OK)
      continue;

    dx[i] = wrap_width - width;
    if (tcx->align == READING_ALIGN_CENTRE)
      dx[i] /= 2;
  }

  origin.x = bounds->x0 - sx + READING_INSET;
  origin.y = bounds->y0 - sy + READING_INSET;

  if (tcx->markdown_nspans > 0)
  {
    reading__draw_styled(tcx, scr, lines, nlines, dx, fg, bg, origin);
    return result_OK;
  }

  /* one bmtext_draw per line so each can take its own indent */
  bmfont_get_info(tcx->font, NULL, &font_height, NULL, NULL);
  for (i = 0; i < nlines; i++)
  {
    point_t line_origin;

    line_origin.x = origin.x + dx[i];
    line_origin.y = origin.y + i * (font_height + LEADING);
    bmtext_draw(tcx->font, scr, &lines[i], 1, fg, bg, LEADING, line_origin,
               &tcx->spacing);
  }

  return result_OK;
}

/* toggle the window's automatic width swing, keeping the "Auto-size" tick
 * in step */
static result_t reading_toggle_resizing(reading_task_t *tcx)
{
  tcx->resizing = !tcx->resizing;

  wuss_menu_tick_item(&tcx->top_menu, READING_MENU_AUTOSIZE, tcx->resizing);
  wuss_menu_tick_item_live(tcx->menu_handle, &tcx->top_menu,
                           READING_MENU_AUTOSIZE, tcx->resizing);
  return result_OK;
}

#define READING_RESIZE_PERIOD_FRAMES 300 /* one full swing every 5s at 60fps */
#define READING_RESIZE_AMPLITUDE     50  /* +/-50px either side of base_width */

static result_t reading_idle(void *task_data)
{
  reading_task_t *tcx;
  int             height, width;
  double          angle;
  result_t        rc;

  tcx = task_data;

  /* the proginfo dialogue is a second window on this same (autoclose)
   * delegate, so closing the main window alone never empties task->windows
   * and the task lingers until the dialogue closes too -- guard against the
   * dangling window in the meantime */
  if (tcx->window == NULL)
    return result_OK;

  if (!tcx->resizing)
    return result_OK;

  height = tcx->base_height;

  tcx->frame_count++;
  angle = tcx->frame_count * (2.0 * M_PI / READING_RESIZE_PERIOD_FRAMES);
  width = tcx->base_width + (int) (READING_RESIZE_AMPLITUDE * sin(angle));

  rc = wuss_window_resize(tcx->window, SIZE2D(width, height));
  if (rc != result_OK)
    logf_warning("reading_idle: wuss_window_resize(%d, %d) failed", width, height);

  return rc;
}

/* A DataLoad on the reading window: show the file it names. */
static result_t reading_message(reading_task_t     *tcx,
                                wuss_window_t      *window,
                                const wuss_event_t *event)
{
  if (tcx->window == NULL || window != tcx->window)
    return result_OK;

  return taskapp_window_data_load(tcx->wuss, tcx->delegate, tcx,
                                  (taskapp_data_load_fn_t) reading_show,
                                  event->data.message);
}

result_t reading_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  reading_task_t *tcx;

  tcx = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return reading_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != tcx->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return reading_open_menu(tcx);
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_SELECT))
      return result_OK;
    return reading_toggle_resizing(tcx);

  case wuss_EVENT_MENU_SELECT:
    {
      result_t      rc;
      const char   *name;
      wuss_colour_t picked;
      int           mine;

      name = wuss_fontmenu_selected(event);
      if (name != NULL)
      {
        rc = reading_set_font(tcx, event->data.menu_select.index, name);

        /* ADJUST keeps the chain open, so retick the still-open font level
         * in place (a no-op once a SELECT pick has closed it). Use the
         * event's own menu, not a fresh reading_fontmenu(): that can rebuild
         * the singleton under the open chain. */
        wuss_menu_tick_exclusive_live(tcx->menu_handle,
                                      event->data.menu_select.menu,
                                      tcx->current);
        return rc;
      }
      if (event->data.menu_select.menu == &tcx->spacing_menu)
        return reading_set_spacing(tcx, event->data.menu_select.index);
      if (event->data.menu_select.menu == &tcx->align_menu)
        return reading_set_align(tcx, event->data.menu_select.index);
      if (wuss_menu_select_is(event, &tcx->top_menu, READING_MENU_AUTOSIZE))
        return reading_toggle_resizing(tcx);
      if (tcx->colourmenu_target == NULL)
        return result_OK;
      picked = wuss_colourmenu_selected(event, &mine);
      if (!mine)
        return result_OK;
      if (tcx->colourmenu_target == &tcx->fg_index)
        return reading_set_fg(tcx, picked);
      return reading_set_bg(tcx, picked);
    }
    return result_OK;

  case wuss_EVENT_MESSAGE:
    return reading_message(tcx, window, event);

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return reading_pre_submenu_open(tcx, event);

  case wuss_EVENT_MENU_CLOSED:
    tcx->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == tcx->window)
      tcx->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    reading_destroy(tcx);
    return result_OK;

  case wuss_EVENT_IDLE:
    return reading_idle(task_data);

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
