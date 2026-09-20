/* wuss/test/tasks/text.c -- sample-text task with font and sample pickers */

#ifdef WUSS_APP

#include <stdlib.h>

#include <math.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/point.h"
#include "io/path.h"
#include "text/bmtext.h"
#include "wuss/menu.h"

#include "text.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* "Colours" leads to a submenu with one row per task->fg_index/bg_index;
 * both rows share the colourmenu singleton, retargeted per hover by
 * text_pre_submenu_open (wuss_EVENT_PRE_SUBMENU_OPEN) so one instance
 * serves either row -- see saturn.c's SATURN_COLOURS_MENU_* for the same
 * pattern. */
enum { TEXT_COLOURS_MENU_FOREGROUND = 0, TEXT_COLOURS_MENU_BACKGROUND };

/* one entry per row of the "Sample" submenu; name is the menu label, text
 * what text_redraw lays out. Pangrams first, lorem ipsum last (and default,
 * matching this task's original fixed paragraph). */
typedef struct text_sample
{
  const char *name;
  const char *text;
  bool        markdown; /* if set, task->text is Markdown source and must be
                         * run through text__markdown_parse before layout,
                         * rather than laid out as plain text */
}
text_sample_t;

static const text_sample_t text_samples[] =
{
  { "Markdown Demo",
    "# Markdown Demo\n"
    "\n"
    "This paragraph has **bold**, *italic*, and `code` runs mixed into plain text, to show inline styling survives word-wrap.\n"
    "\n"
    "## Lists\n"
    "\n"
    "- First item\n"
    "- Second item with **bold** in it\n"
    "- Third item\n"
    "\n"
    "### Smaller Heading\n"
    "\n"
    "A closing paragraph after a bullet list and a sub-heading, to check spacing between block types looks right.",
    true },
  { "Lorem Ipsum",
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed eros lacus, imperdiet eget finibus ac, tempus vel risus. Donec scelerisque, elit quis pretium imperdiet, orci magna varius tellus, sit amet sodales ante orci nec nibh. Pellentesque placerat eu diam vitae pharetra. Nunc aliquet ante mi, vulputate commodo dui placerat eu. Morbi velit ex, scelerisque vel mi elementum, tristique viverra enim. Integer a interdum eros, id fringilla nunc. Nunc non felis nisi. Aliquam nec ullamcorper tellus. Maecenas sed aliquam diam. Duis pretium aliquet metus. Suspendisse rhoncus turpis vel dui euismod fringilla. Vivamus efficitur leo vel metus condimentum, tempor bibendum augue vehicula. Nunc eleifend sagittis tortor eget pretium. Fusce interdum tortor eget sapien blandit consectetur. Aliquam vestibulum euismod eros a luctus. Etiam nec nisl et diam imperdiet lobortis. Sed in eros sed tellus commodo bibendum. Sed ipsum velit, sodales a pulvinar non, pharetra ut est. Curabitur eu odio id magna posuere eleifend non id erat. Pellentesque commodo blandit mauris, ac consequat nisi dapibus eget. Mauris sollicitudin molestie urna, sit amet ornare turpis tincidunt bibendum. Vivamus interdum bibendum luctus. Suspendisse in arcu velit. Aenean eget bibendum dolor. Quisque tristique porta purus ornare tincidunt. Etiam hendrerit nunc tellus, et tempus ligula laoreet eu. Quisque pellentesque malesuada tempor. Mauris eu lectus ut neque fringilla hendrerit. Sed scelerisque laoreet felis a eleifend. Fusce sit amet mauris tellus. Sed orci ipsum, consectetur vitae blandit ut, egestas vehicula dolor. Integer ullamcorper, metus a vulputate mattis, elit orci accumsan ligula, eget elementum nibh tortor sit amet mauris. In aliquet nibh at scelerisque suscipit. Sed elit purus, sagittis eu accumsan ultricies, lobortis a odio. Duis libero sem, tempus in fringilla nec, bibendum eu metus. Nulla eget justo metus. In luctus ante massa, pellentesque commodo lorem pretium sed. Cras ultricies est lacus, ut dictum lorem gravida sed. Ut augue mauris, dignissim a pulvinar eget, pharetra ac turpis. Sed ultricies nulla mauris, id dictum ex scelerisque sit amet. Morbi et placerat enim. Phasellus arcu nisl, tempor vitae lacinia et, finibus quis justo. Fusce ipsum mi, porttitor nec faucibus at, fermentum ut massa. Cras faucibus molestie mauris. Sed et metus eget lectus luctus cursus a sit amet eros. Sed enim ligula, gravida eget magna eu, suscipit fermentum lorem. Maecenas vestibulum mollis lacus nec accumsan. Nullam molestie justo eu turpis facilisis tempus quis quis velit. Phasellus gravida mollis condimentum. Nam fringilla mollis dolor, quis posuere quam iaculis ac. Suspendisse ac maximus mi. Pellentesque aliquam ante ante, sed facilisis sapien dictum ac. Nullam pulvinar ante vitae dictum rhoncus. Praesent in pretium justo. Quisque pellentesque at sapien at pulvinar. Aenean a lorem at sapien molestie ullamcorper in dignissim ante. Nunc sagittis mi at dolor accumsan laoreet. Ut id congue elit, ut semper metus. Praesent tellus orci, feugiat suscipit diam sit amet, malesuada efficitur metus. Phasellus condimentum justo ipsum, et lobortis mi ultrices a. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nunc vestibulum volutpat laoreet. Vestibulum convallis lectus at accumsan imperdiet. Aliquam suscipit, justo condimentum sodales iaculis, tellus enim fermentum lacus, quis volutpat sapien tortor quis enim. Vestibulum vehicula turpis eu lorem gravida, nec volutpat massa dapibus. Mauris egestas accumsan mattis. Nullam ex risus, imperdiet ut vestibulum a, malesuada at odio. Praesent congue, nulla a eleifend dignissim, ligula arcu tincidunt tortor, vel vestibulum quam ipsum vitae metus. Nullam lacinia interdum enim id bibendum. Praesent quis elit id turpis cursus auctor. Etiam turpis massa, finibus sed odio quis, dapibus tristique magna. Morbi quis commodo tortor. Pellentesque hendrerit non libero non pretium. Aenean nunc est, aliquet eget tincidunt id, auctor ultrices orci velit." },
  { "Quick Brown Fox",
    "The quick brown fox jumps over the lazy dog." },
  { "Pangram (Cwm Fjord)",
    "Cwm fjord bank glyphs vext quiz." },
  { "Pangram (Waltz)",
    "Waltz, bad nymph, for quick jigs vex." }
};

#define TEXT_DEFAULT_SAMPLE (1) /* "Lorem Ipsum" */

/* one entry per row of the "Spacing" submenu */
typedef struct text_spacing_preset
{
  const char *name;
  int         letter_spacing;
  int         word_spacing;
}
text_spacing_preset_t;

static const text_spacing_preset_t text_spacing_presets[] =
{
  { "Normal",      0, 0 },
  { "Letter",      1, 0 },
  { "Word",        0, 1 },
  { "Letter+Word", 1, 1 }
};

#define TEXT_DEFAULT_SPACING 0 /* "Normal" */

/* fixed tint colours text__markdown_parse assigns to styled runs -- palette-
 * independent since bmfont has no bold/italic glyph variants to switch to,
 * so emphasis is shown by colour instead */
#define TEXT_MD_HEADER_COLOUR colour_rgb(0x20, 0x60, 0xd0)
#define TEXT_MD_BOLD_COLOUR   colour_rgb(0xd0, 0x20, 0x20)
#define TEXT_MD_ITALIC_COLOUR colour_rgb(0x20, 0x90, 0x40)
#define TEXT_MD_CODE_COLOUR   colour_rgb(0x90, 0x40, 0xc0)

#define TEXT_MD_MAX_SPANS 128 /* demo text is short and fixed; overflow is
                               * dropped */

/* is c the start of an ATX header line ("#" through "######" then a
 * space), given p points at the first "#" and is known to be at a line
 * start? advances *p past the "# " prefix and returns true if so. */
static bool text__markdown_header_prefix(const char **p, const char *end)
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
static bool text__markdown_emphasis(const char **p,
                                    const char  *end,
                                    char        *out,
                                    int         *outlen,
                                    text_span_t *spans,
                                    int         *nspans,
                                    int          maxspans)
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
    colour    = TEXT_MD_BOLD_COLOUR;
  }
  else if (start[0] == '*' || start[0] == '_')
  {
    markerlen = 1;
    colour    = TEXT_MD_ITALIC_COLOUR;
  }
  else /* '`' */
  {
    markerlen = 1;
    colour    = TEXT_MD_CODE_COLOUR;
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

/* text__markdown_parse's per-character state: which kind of line the
 * cursor is currently in. LINE_START only ever lasts for the header-prefix
 * check on the first character of a line; every other character on a line
 * is scanned in HEADER or BODY. */
typedef enum text_md_state
{
  TEXT_MD_LINE_START,
  TEXT_MD_HEADER,
  TEXT_MD_BODY
}
text_md_state_t;

/* emit the pending header span for the line running [line_start, textlen)
 * into text, if state says it's a header and the line isn't empty */
static void text__markdown_emit_header(text_md_state_t state,
                                       int             line_start,
                                       int             textlen,
                                       text_span_t    *spans,
                                       int            *nspans)
{
  if (state != TEXT_MD_HEADER || line_start >= textlen ||
      *nspans >= TEXT_MD_MAX_SPANS)
    return;

  spans[*nspans].start  = line_start;
  spans[*nspans].len    = textlen - line_start;
  spans[*nspans].colour = TEXT_MD_HEADER_COLOUR;
  (*nspans)++;
}

/* parse Markdown source into a freshly malloc'd plain-text buffer (headers'
 * "#" prefixes and inline "**"/"*"/"_"/"`" markers stripped) plus a
 * freshly malloc'd array of styled runs (headers and inline emphasis/code)
 * into that buffer. *out_text and *out_spans are NULL on failure (OOM);
 * caller frees both with free(). Headers get a run for their whole
 * (marker-stripped) line; inline markers must close on the same line as
 * they open or are left as literal text. */
static result_t text__markdown_parse(const char   *md,
                                     int           mdlen,
                                     char        **out_text,
                                     text_span_t **out_spans,
                                     int          *out_nspans)
{
  char           *text;
  text_span_t    *spans;
  int             nspans;
  int             textlen;
  const char     *p;
  const char     *end;
  text_md_state_t state;
  int             line_start;

  text = malloc((size_t) mdlen + 1);
  if (text == NULL)
    goto oom;

  spans = malloc(TEXT_MD_MAX_SPANS * sizeof(*spans));
  if (spans == NULL)
  {
    free(text);
    goto oom;
  }

  nspans     = 0;
  textlen    = 0;
  p          = md;
  end        = md + mdlen;
  state      = TEXT_MD_LINE_START;
  line_start = 0;

  while (p < end)
  {
    switch (state)
    {
    case TEXT_MD_LINE_START:
      if (*p == '#' && text__markdown_header_prefix(&p, end))
        state = TEXT_MD_HEADER;
      else
        state = TEXT_MD_BODY;
      break;

    case TEXT_MD_HEADER:
    case TEXT_MD_BODY:
      if (*p == '\n')
      {
        text__markdown_emit_header(state, line_start, textlen, spans,
                                   &nspans);
        text[textlen++] = *p++;
        line_start      = textlen;
        state           = TEXT_MD_LINE_START;
      }
      else if ((*p == '*' || *p == '_' || *p == '`') &&
              text__markdown_emphasis(&p, end, text, &textlen, spans,
                                      &nspans, TEXT_MD_MAX_SPANS))
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

  text__markdown_emit_header(state, line_start, textlen, spans, &nspans);

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

/* index into task->top_items[] of the "Info" leaf */
#define TEXT_MENU_INFO 0

/* index into task->top_items[] of the "Font" leaf */
#define TEXT_MENU_FONT 1

/* ----------------------------------------------------------------------- */

/* the shared fontmenu singleton, retargeted at task->wuss's bmfonts dir --
 * cheap to call repeatedly since wuss_fontmenu_menu only rebuilds when the
 * dir or wuss_t actually changes */
static const wuss_menu_t *text_fontmenu(text_task_t *task)
{
  const char *resources;
  const char *bmfonts_dir;

  resources   = wuss_get_resources(task->wuss);
  bmfonts_dir = pathf("%s/resources/bmfonts", resources);
  return wuss_fontmenu_menu(bmfonts_dir, "Font", task->wuss);
}

/* load fonts[idx] if not already in hand; returns it or NULL on failure.
 * name is the menu label for that row -- the font's leafname sans ".png". */
static bmfont_t *text_load_font(text_task_t *task,
                                const char  *resources,
                                int          idx,
                                const char  *name)
{
  result_t    rc;
  const char *filename;
  bmfont_t   *font;

  if (task->fonts[idx] != NULL)
    return task->fonts[idx];

  filename = pathf("%s/resources/bmfonts/%s.png", resources, name);

  rc = bmfont_create(filename, &font);
  if (rc != result_OK)
    return NULL;

  task->fonts[idx] = font;
  return font;
}

/* switch the paragraph to the font at menu row idx (label name) */
static result_t text_set_font(text_task_t *task, int idx, const char *name)
{
  bmfont_t *font;

  if (idx < 0 || idx >= task->nfonts || idx == task->current)
    return result_OK;

  font = text_load_font(task, wuss_get_resources(task->wuss), idx, name);
  if (font == NULL)
    return result_OK; /* leave the current font in place */

  task->font    = font;
  task->current = idx;

  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* (re)point task->text at text_samples[idx], parsing it as Markdown into
 * task->markdown_text/markdown_spans first if that sample is marked
 * markdown, freeing any previous markdown buffers either way; idx must
 * already be range-checked */
static result_t text__apply_sample(text_task_t *task, int idx)
{
  result_t rc;

  free(task->markdown_text);
  free(task->markdown_spans);
  task->markdown_text   = NULL;
  task->markdown_spans  = NULL;
  task->markdown_nspans = 0;

  if (text_samples[idx].markdown)
  {
    rc = text__markdown_parse(text_samples[idx].text,
                              (int) strlen(text_samples[idx].text),
                              &task->markdown_text,
                              &task->markdown_spans,
                              &task->markdown_nspans);
    if (rc != result_OK)
      return rc;
    task->text = task->markdown_text;
  }
  else
  {
    task->text = text_samples[idx].text;
  }

  task->sample = idx;
  return result_OK;
}

/* switch the shown text to text_samples[idx] */
static result_t text_set_sample(text_task_t *task, int idx)
{
  result_t rc;

  if (idx < 0 || idx >= NELEMS(text_samples) || idx == task->sample)
    return result_OK;

  rc = text__apply_sample(task, idx);
  if (rc != result_OK)
    return rc;

  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* switch the paragraph's foreground to system palette index "picked" */
static result_t text_set_fg(text_task_t *task, wuss_colour_t picked)
{
  task->fg_index = picked;
  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* switch the paragraph's background to system palette index "picked", or to
 * none (glyphs blend straight onto whatever is behind the window's content)
 * for wuss_NO_BACKGROUND, the colourmenu's "None" row */
static result_t text_set_bg(text_task_t *task, wuss_colour_t picked)
{
  task->bg_transparent = (picked == wuss_NO_BACKGROUND);
  if (!task->bg_transparent)
    task->bg_index = picked;
  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* switch letter/word spacing to text_spacing_presets[idx] */
static result_t text_set_spacing(text_task_t *task, int idx)
{
  if (idx < 0 || idx >= NELEMS(text_spacing_presets) || idx == task->spacing_idx)
    return result_OK;

  task->spacing_idx           = idx;
  task->spacing.letter_spacing = text_spacing_presets[idx].letter_spacing;
  task->spacing.word_spacing   = text_spacing_presets[idx].word_spacing;

  wuss_menu_tick_exclusive(&task->spacing_menu, idx);
  wuss_menu_tick_exclusive_live(task->menu_handle, &task->spacing_menu, idx);
  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

static result_t text_open_menu(text_task_t *task)
{
  static const wuss_proginfo_desc_t desc =
  {
    "Text",
    "Sample-text layout with font/colour/spacing pickers",
    "(c) DPTLib contributors",
    "1.0 (" __DATE__ ")"
  };

  wuss_proginfo_set_desc(&desc);
  task->top_items[TEXT_MENU_INFO].window = wuss_proginfo_window(task->delegate);

  return wuss_menu_open(task->delegate,
                        &task->top_menu,
                        wuss_get_pointer(task->wuss), &task->menu_handle);
}

/* Every submenu leaf fires wuss_EVENT_PRE_SUBMENU_OPEN, not just the
 * Foreground/Background rows -- "Font"/"Sample"/"Spacing" are each one too
 * (their submenu never changes, so they just open unchanged). Only the
 * Foreground/Background level needs to retarget the shared colourmenu
 * singleton before opening it -- see saturn.c's saturn_pre_submenu_open for
 * the same pattern. */
static result_t text_pre_submenu_open(text_task_t        *task,
                                      const wuss_event_t *event)
{
  wuss_menu_handle_t handle;
  int                index;

  handle = event->data.pre_submenu_open.handle;
  index  = event->data.pre_submenu_open.index;

  if (wuss_menu_handle_menu(handle) != &task->colours_menu)
  {
    if (index == TEXT_MENU_FONT)
      /* the fontmenu singleton may have been rebuilt (at a new address) by
       * another task since top_items[TEXT_MENU_FONT].submenu was cached in
       * text_create -- re-fetch instead of handing the spawner a stale
       * pointer */
      return wuss_menu_open_submenu_now(handle, index, text_fontmenu(task));

    return wuss_menu_open_submenu_now(handle, index,
                                      task->top_items[index].submenu);
  }

  if (index == TEXT_COLOURS_MENU_FOREGROUND)
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

  return wuss_menu_open_submenu_now(handle, index,
                                    wuss_colourmenu_menu(task->wuss));
}

/* ----------------------------------------------------------------------- */

result_t text_create(wuss_t *wuss, text_task_t **out)
{
  result_t           rc;
  text_task_t       *task;
  wuss_task_t       *delegate;
  wuss_task_desc_t   delegate_desc;
  const wuss_menu_t *menu;
  size2d_t           sz;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->font        = wuss_get_font(wuss);
  task->current     = -1; /* the wuss system font is none of the picker's */
  task->spacing_idx = TEXT_DEFAULT_SPACING;
  task->spacing.letter_spacing = text_spacing_presets[TEXT_DEFAULT_SPACING].letter_spacing;
  task->spacing.word_spacing   = text_spacing_presets[TEXT_DEFAULT_SPACING].word_spacing;
  task->fg_index    = 0; /* wuss__default_palette: 0 is black */
  task->bg_index    = 7; /* wuss__default_palette: 7 is white */
  task->colourmenu_target = NULL;
  task->frame_count = 0;
  task->resizing    = true;

  /* the shared picker: every ".png" font under resources/bmfonts, sorted,
   * less any SYSTEM-class font (e.g. the one wuss draws menu ticks/arrows
   * from) */
  menu = text_fontmenu(task);
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

  if (wuss_colourmenu_menu(wuss) == NULL)
  {
    free(task->fonts);
    free(task);
    return result_OOM;
  }

  rc = text__apply_sample(task, TEXT_DEFAULT_SAMPLE);
  if (rc != result_OK)
  {
    free(task->fonts);
    free(task);
    return rc;
  }

  task->sample_items[0].text    = "Markdown Demo";
  task->sample_items[0].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[0].submenu = NULL;
  task->sample_items[0].window  = NULL;
  task->sample_items[1].text    = "Lorem Ipsum";
  task->sample_items[1].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[1].submenu = NULL;
  task->sample_items[1].window  = NULL;
  task->sample_items[2].text    = "Quick Brown Fox";
  task->sample_items[2].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[2].submenu = NULL;
  task->sample_items[2].window  = NULL;
  task->sample_items[3].text    = "Pangram (Cwm Fjord)";
  task->sample_items[3].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[3].submenu = NULL;
  task->sample_items[3].window  = NULL;
  task->sample_items[4].text    = "Pangram (Waltz)";
  task->sample_items[4].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[4].submenu = NULL;
  task->sample_items[4].window  = NULL;

  WUSS_MENU_TITLE(task->sample_menu, "Sample", task->sample_items,
                 NELEMS(task->sample_items));

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

  /* Both rows' .submenu just need to be non-NULL to draw an arrow and
   * become hoverable; which menu they name doesn't matter since
   * text_pre_submenu_open always supplies the menu to open (see saturn.c's
   * SATURN_COLOURS_MENU_* for the same pattern). */
  WUSS_MENU_ITEM_MENU(task->colours_items, TEXT_COLOURS_MENU_FOREGROUND,
                      "Foreground", wuss_MENU_ITEM_PRE_OPEN,
                      wuss_colourmenu_menu(wuss));
  WUSS_MENU_ITEM_MENU(task->colours_items, TEXT_COLOURS_MENU_BACKGROUND,
                      "Background", wuss_MENU_ITEM_PRE_OPEN,
                      wuss_colourmenu_menu(wuss));

  WUSS_MENU_TITLE(task->colours_menu, "Colours", task->colours_items,
                 NELEMS(task->colours_items));

  /* top-level menu: "Font" borrows the fontmenu's own live wuss_menu_t (so
   * ticks and wuss_fontmenu_selected keep working), "Sample"/"Spacing"/
   * "Colours" are the per-instance menus built above */
  task->top_items[0].text    = "Info";
  task->top_items[0].flags   =
    wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN;
  task->top_items[0].submenu = NULL;
  task->top_items[0].window  = NULL;
  task->top_items[1].text    = "Font";
  task->top_items[1].flags   = wuss_MENU_ITEM_PRE_OPEN;
  task->top_items[1].submenu = menu;
  task->top_items[1].window  = NULL;
  task->top_items[2].text    = "Sample";
  task->top_items[2].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[2].submenu = &task->sample_menu;
  task->top_items[2].window  = NULL;
  task->top_items[3].text    = "Spacing";
  task->top_items[3].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[3].submenu = &task->spacing_menu;
  task->top_items[3].window  = NULL;
  task->top_items[4].text    = "Colours";
  task->top_items[4].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[4].submenu = &task->colours_menu;
  task->top_items[4].window  = NULL;

  WUSS_MENU_TITLE(task->top_menu, "Text", task->top_items,
                 NELEMS(task->top_items));

  delegate_desc.handle    = text_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "text";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task->fonts);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  sz                = SIZE2D(220, 220);
  task->base_width  = sz.w;
  task->base_height = sz.h;

  rc = wuss_window_create_placed(delegate,
                                 sz,
                                 "Sample Text",
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

void text_destroy(text_task_t *task)
{
  int i;

  if (task->menu_handle != NULL)
    wuss_menu_close(task->menu_handle);

  for (i = 0; i < task->nfonts; i++)
    if (task->fonts[i] != NULL)
      bmfont_destroy(task->fonts[i]);
  free(task->fonts);
  free(task->markdown_text);
  free(task->markdown_spans);
  free(task);
}

#define TEXT_INSET 4
#define LEADING    2
#define MAX_LINES  64 /* paragraph is short and fixed; overflow is dropped */

/* like bmtext_draw, but each line is split against tcx->markdown_spans (byte
 * offsets into tcx->text) so a span's run is drawn in its own colour
 * instead of fg; used only when tcx->markdown_nspans > 0 */
static void text__draw_styled(text_task_t         *tcx,
                              screen_t            *scr,
                              const bmtext_line_t *lines,
                              int                  nlines,
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

    while (cursor < line_len)
    {
      int                abs_pos;
      const text_span_t *span;
      int                j;
      int                run_start;
      int                run_len;
      colour_t           run_colour;

      abs_pos = line_off + cursor;
      span    = NULL;
      for (j = 0; j < tcx->markdown_nspans; j++)
      {
        const text_span_t *s;

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
          const text_span_t *s;
          int                rel;

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

    pos.x  = origin.x;
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
static int text__layout_lines(bmfont_t               *font,
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

static result_t text_redraw(const wuss_event_t *event, void *task_data)
{
  text_task_t    *tcx;
  screen_t       *scr;
  const box_t    *bounds;
  int             sx, sy;
  const colour_t *palette;
  colour_t        fg, bg;
  bmtext_line_t   lines[MAX_LINES];
  int             nlines;
  point_t         origin;

  tcx = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  palette = wuss_get_palette(tcx->wuss, NULL);
  fg      = palette[tcx->fg_index];
  bg      = tcx->bg_transparent ? colour_rgba(0, 0, 0, 0) : palette[tcx->bg_index];

  nlines = text__layout_lines(tcx->font,
                              tcx->text,
                              (bounds->x1 - TEXT_INSET) - (bounds->x0 + TEXT_INSET),
                              &tcx->spacing,
                              lines,
                              MAX_LINES);

  origin.x = bounds->x0 - sx + TEXT_INSET;
  origin.y = bounds->y0 - sy + TEXT_INSET;

  if (tcx->markdown_nspans > 0)
    text__draw_styled(tcx, scr, lines, nlines, fg, bg, origin);
  else
    bmtext_draw(tcx->font, scr, lines, nlines, fg, bg, LEADING, origin,
               &tcx->spacing);

  return result_OK;
}

static result_t text_mouse(void *task_data)
{
  text_task_t *tcx;

  tcx = task_data;

  tcx->resizing = !tcx->resizing;

  return result_OK;
}

#define TEXT_RESIZE_PERIOD_FRAMES 300 /* one full swing every 5s at 60fps */
#define TEXT_RESIZE_AMPLITUDE     50  /* +/-50px either side of base_width */

static result_t text_idle(void *task_data)
{
  text_task_t *tcx;
  int          height, width;
  double       angle;
  result_t     rc;

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
  angle = tcx->frame_count * (2.0 * M_PI / TEXT_RESIZE_PERIOD_FRAMES);
  width = tcx->base_width + (int) (TEXT_RESIZE_AMPLITUDE * sin(angle));

  rc = wuss_window_resize(tcx->window, SIZE2D(width, height));
  if (rc != result_OK)
    logf_warning("text_idle: wuss_window_resize(%d, %d) failed", width, height);

  return rc;
}

result_t text_handle(wuss_window_t      *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  text_task_t *tcx;

  tcx = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return text_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != tcx->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return text_open_menu(tcx);
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_SELECT))
      return result_OK;
    return text_mouse(task_data);

  case wuss_EVENT_MENU_SELECT:
    {
      const char   *name;
      wuss_colour_t picked;
      int           mine;

      name = wuss_fontmenu_selected(event);
      if (name != NULL)
        return text_set_font(tcx, event->data.menu_select.index, name);
      if (event->data.menu_select.menu == &tcx->sample_menu)
        return text_set_sample(tcx, event->data.menu_select.index);
      if (event->data.menu_select.menu == &tcx->spacing_menu)
        return text_set_spacing(tcx, event->data.menu_select.index);
      if (tcx->colourmenu_target == NULL)
        return result_OK;
      picked = wuss_colourmenu_selected(event, &mine);
      if (!mine)
        return result_OK;
      if (tcx->colourmenu_target == &tcx->fg_index)
        return text_set_fg(tcx, picked);
      return text_set_bg(tcx, picked);
    }
    return result_OK;

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return text_pre_submenu_open(tcx, event);

  case wuss_EVENT_MENU_CLOSED:
    tcx->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == tcx->window)
      tcx->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == tcx->top_items[TEXT_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;
    if (event->data.pre_show.handle == NULL)
      return result_OK;
    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_QUIT:
    text_destroy(tcx);
    return result_OK;

  case wuss_EVENT_IDLE:
    return text_idle(task_data);

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
