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

/* one entry per row of the "Sample" submenu; name is the menu label, text
 * what text_redraw lays out. Pangrams first, lorem ipsum last (and default,
 * matching this task's original fixed paragraph). */
typedef struct text_sample
{
  const char *name;
  const char *text;
}
text_sample_t;

static const text_sample_t text_samples[] =
{
  { "Lorem Ipsum",
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed eros lacus, imperdiet eget finibus ac, tempus vel risus. Donec scelerisque, elit quis pretium imperdiet, orci magna varius tellus, sit amet sodales ante orci nec nibh. Pellentesque placerat eu diam vitae pharetra. Nunc aliquet ante mi, vulputate commodo dui placerat eu. Morbi velit ex, scelerisque vel mi elementum, tristique viverra enim. Integer a interdum eros, id fringilla nunc. Nunc non felis nisi. Aliquam nec ullamcorper tellus. Maecenas sed aliquam diam. Duis pretium aliquet metus. Suspendisse rhoncus turpis vel dui euismod fringilla. Vivamus efficitur leo vel metus condimentum, tempor bibendum augue vehicula. Nunc eleifend sagittis tortor eget pretium. Fusce interdum tortor eget sapien blandit consectetur. Aliquam vestibulum euismod eros a luctus. Etiam nec nisl et diam imperdiet lobortis. Sed in eros sed tellus commodo bibendum. Sed ipsum velit, sodales a pulvinar non, pharetra ut est. Curabitur eu odio id magna posuere eleifend non id erat. Pellentesque commodo blandit mauris, ac consequat nisi dapibus eget. Mauris sollicitudin molestie urna, sit amet ornare turpis tincidunt bibendum. Vivamus interdum bibendum luctus. Suspendisse in arcu velit. Aenean eget bibendum dolor. Quisque tristique porta purus ornare tincidunt. Etiam hendrerit nunc tellus, et tempus ligula laoreet eu. Quisque pellentesque malesuada tempor. Mauris eu lectus ut neque fringilla hendrerit. Sed scelerisque laoreet felis a eleifend. Fusce sit amet mauris tellus. Sed orci ipsum, consectetur vitae blandit ut, egestas vehicula dolor. Integer ullamcorper, metus a vulputate mattis, elit orci accumsan ligula, eget elementum nibh tortor sit amet mauris. In aliquet nibh at scelerisque suscipit. Sed elit purus, sagittis eu accumsan ultricies, lobortis a odio. Duis libero sem, tempus in fringilla nec, bibendum eu metus. Nulla eget justo metus. In luctus ante massa, pellentesque commodo lorem pretium sed. Cras ultricies est lacus, ut dictum lorem gravida sed. Ut augue mauris, dignissim a pulvinar eget, pharetra ac turpis. Sed ultricies nulla mauris, id dictum ex scelerisque sit amet. Morbi et placerat enim. Phasellus arcu nisl, tempor vitae lacinia et, finibus quis justo. Fusce ipsum mi, porttitor nec faucibus at, fermentum ut massa. Cras faucibus molestie mauris. Sed et metus eget lectus luctus cursus a sit amet eros. Sed enim ligula, gravida eget magna eu, suscipit fermentum lorem. Maecenas vestibulum mollis lacus nec accumsan. Nullam molestie justo eu turpis facilisis tempus quis quis velit. Phasellus gravida mollis condimentum. Nam fringilla mollis dolor, quis posuere quam iaculis ac. Suspendisse ac maximus mi. Pellentesque aliquam ante ante, sed facilisis sapien dictum ac. Nullam pulvinar ante vitae dictum rhoncus. Praesent in pretium justo. Quisque pellentesque at sapien at pulvinar. Aenean a lorem at sapien molestie ullamcorper in dignissim ante. Nunc sagittis mi at dolor accumsan laoreet. Ut id congue elit, ut semper metus. Praesent tellus orci, feugiat suscipit diam sit amet, malesuada efficitur metus. Phasellus condimentum justo ipsum, et lobortis mi ultrices a. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nunc vestibulum volutpat laoreet. Vestibulum convallis lectus at accumsan imperdiet. Aliquam suscipit, justo condimentum sodales iaculis, tellus enim fermentum lacus, quis volutpat sapien tortor quis enim. Vestibulum vehicula turpis eu lorem gravida, nec volutpat massa dapibus. Mauris egestas accumsan mattis. Nullam ex risus, imperdiet ut vestibulum a, malesuada at odio. Praesent congue, nulla a eleifend dignissim, ligula arcu tincidunt tortor, vel vestibulum quam ipsum vitae metus. Nullam lacinia interdum enim id bibendum. Praesent quis elit id turpis cursus auctor. Etiam turpis massa, finibus sed odio quis, dapibus tristique magna. Morbi quis commodo tortor. Pellentesque hendrerit non libero non pretium. Aenean nunc est, aliquet eget tincidunt id, auctor ultrices orci velit." },
  { "Quick Brown Fox",
    "The quick brown fox jumps over the lazy dog." },
  { "Pangram (Cwm Fjord)",
    "Cwm fjord bank glyphs vext quiz." },
  { "Pangram (Waltz)",
    "Waltz, bad nymph, for quick jigs vex." }
};

#define TEXT_DEFAULT_SAMPLE (0) /* "Lorem Ipsum" */

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

/* index into task->top_items[] of the tickable "No Background" leaf */
#define TEXT_MENU_NO_BACKGROUND 5

/* index into task->top_items[] of the "Info" leaf */
#define TEXT_MENU_INFO 6

/* ----------------------------------------------------------------------- */

/* load fonts[idx] if not already in hand; returns it or NULL on failure.
 * name is the menu label for that row -- the font's leafname sans ".png". */
static bmfont_t *text_load_font(text_task_t *task,
                                const char  *resources,
                                int          idx,
                                const char  *name)
{
  result_t    rc;
  const char *leaf;
  const char *filename;
  bmfont_t   *font;

  if (task->fonts[idx] != NULL)
    return task->fonts[idx];

  leaf     = path_join_leafname(name, "png");
  filename = path_join_filename(resources, 3, "resources", "bmfonts", leaf);

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

/* switch the shown text to text_samples[idx] */
static result_t text_set_sample(text_task_t *task, int idx)
{
  if (idx < 0 || idx >= NELEMS(text_samples) || idx == task->sample)
    return result_OK;

  task->sample = idx;
  task->text   = text_samples[idx].text;

  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

/* toggle whether the paragraph is drawn with its picked background colour or
 * none, so glyphs blend straight onto whatever is already behind the
 * window's content */
static result_t text_toggle_bg(text_task_t *task)
{
  wuss_menu_item_t *item;

  task->bg_transparent = !task->bg_transparent;

  item = &task->top_items[TEXT_MENU_NO_BACKGROUND];
  if (task->bg_transparent)
    item->flags |= wuss_MENU_ITEM_TICKED;
  else
    item->flags &= ~(wuss_menu_item_flags_t) wuss_MENU_ITEM_TICKED;

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

/* switch the paragraph's background to system palette index "picked" */
static result_t text_set_bg(text_task_t *task, wuss_colour_t picked)
{
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
  return wuss_menu_open(task->delegate,
                        &task->top_menu,
                        wuss_get_pointer(task->wuss), &task->menu_handle);
}

/* ----------------------------------------------------------------------- */

result_t text_create(wuss_t *wuss, text_task_t **out)
{
  result_t           rc;
  text_task_t       *task;
  wuss_task_t       *delegate;
  wuss_task_desc_t   delegate_desc;
  const char        *resources;
  const char        *bmfonts_dir;
  const wuss_menu_t *menu;
  size2d_t           sz;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->font        = wuss_get_font(wuss);
  task->current     = -1; /* the wuss system font is none of the picker's */
  task->sample      = TEXT_DEFAULT_SAMPLE;
  task->text        = text_samples[TEXT_DEFAULT_SAMPLE].text;
  task->spacing_idx = TEXT_DEFAULT_SPACING;
  task->spacing.letter_spacing = text_spacing_presets[TEXT_DEFAULT_SPACING].letter_spacing;
  task->spacing.word_spacing   = text_spacing_presets[TEXT_DEFAULT_SPACING].word_spacing;
  task->fg_index    = 0; /* wuss__default_palette: 0 is black */
  task->bg_index    = 7; /* wuss__default_palette: 7 is white */
  task->frame_count = 0;
  task->resizing    = true;

  resources = wuss_get_resources(wuss);

  /* the picker: every ".png" font under resources/bmfonts, sorted, less any
   * SYSTEM-class font (e.g. the one wuss draws menu ticks/arrows from) */
  bmfonts_dir = path_join_filename(resources, 2, "resources", "bmfonts");
  rc = wuss_fontmenu_create(&task->fontmenu, bmfonts_dir, "Font", wuss, NULL);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  menu         = wuss_fontmenu_menu(task->fontmenu);
  task->nfonts = menu->nitems;
  task->fonts  = calloc((size_t) task->nfonts, sizeof(*task->fonts));
  if (task->nfonts > 0 && task->fonts == NULL)
  {
    wuss_fontmenu_destroy(task->fontmenu);
    free(task);
    return result_OOM;
  }

  rc = wuss_colourmenu_create(&task->fgmenu, wuss, "Foreground");
  if (rc != result_OK)
  {
    wuss_fontmenu_destroy(task->fontmenu);
    free(task->fonts);
    free(task);
    return rc;
  }

  rc = wuss_colourmenu_create(&task->bgmenu, wuss, "Background");
  if (rc != result_OK)
  {
    wuss_colourmenu_destroy(task->fgmenu);
    wuss_fontmenu_destroy(task->fontmenu);
    free(task->fonts);
    free(task);
    return rc;
  }

  task->sample_items[0].text    = "Lorem Ipsum";
  task->sample_items[0].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[0].submenu = NULL;
  task->sample_items[0].window  = NULL;
  task->sample_items[1].text    = "Quick Brown Fox";
  task->sample_items[1].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[1].submenu = NULL;
  task->sample_items[1].window  = NULL;
  task->sample_items[2].text    = "Pangram (Cwm Fjord)";
  task->sample_items[2].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[2].submenu = NULL;
  task->sample_items[2].window  = NULL;
  task->sample_items[3].text    = "Pangram (Waltz)";
  task->sample_items[3].flags   = wuss_MENU_ITEM_NONE;
  task->sample_items[3].submenu = NULL;
  task->sample_items[3].window  = NULL;

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

  /* top-level menu: "Font" borrows the fontmenu's own live wuss_menu_t (so
   * ticks and wuss_fontmenu_selected keep working), "Sample"/"Spacing" are
   * the per-instance menus built above, "Foreground"/"Background" each
   * borrow their colourmenu's live wuss_menu_t the same way */
  task->top_items[0].text    = "Font";
  task->top_items[0].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[0].submenu = menu;
  task->top_items[0].window  = NULL;
  task->top_items[1].text    = "Sample";
  task->top_items[1].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[1].submenu = &task->sample_menu;
  task->top_items[1].window  = NULL;
  task->top_items[2].text    = "Spacing";
  task->top_items[2].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[2].submenu = &task->spacing_menu;
  task->top_items[2].window  = NULL;
  task->top_items[3].text    = "Foreground";
  task->top_items[3].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[3].submenu = wuss_colourmenu_menu(task->fgmenu);
  task->top_items[3].window  = NULL;
  task->top_items[4].text    = "Background";
  task->top_items[4].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[4].submenu = wuss_colourmenu_menu(task->bgmenu);
  task->top_items[4].window  = NULL;
  task->top_items[5].text    = "No Background";
  task->top_items[5].flags   = wuss_MENU_ITEM_NONE;
  task->top_items[5].submenu = NULL;
  task->top_items[5].window  = NULL;
  task->top_items[6].text    = "Info";
  task->top_items[6].flags   =
    wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN;
  task->top_items[6].submenu = NULL;
  task->top_items[6].window  = NULL;

  WUSS_MENU_TITLE(task->top_menu, "Text", task->top_items,
                 NELEMS(task->top_items));

  delegate_desc.handle    = text_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "text";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    wuss_fontmenu_destroy(task->fontmenu);
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

  {
    static const wuss_proginfo_desc_t desc =
    {
      "Text",
      "Sample-text layout with font/colour/spacing pickers",
      "(c) DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };
    if (wuss_proginfo_create(&task->proginfo, delegate, &desc) != result_OK)
      task->proginfo = NULL;
  }
  task->top_items[TEXT_MENU_INFO].window =
    wuss_proginfo_window(task->proginfo);

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
  wuss_fontmenu_destroy(task->fontmenu);
  wuss_colourmenu_destroy(task->fgmenu);
  wuss_colourmenu_destroy(task->bgmenu);
  wuss_proginfo_destroy(task->proginfo);
  free(task);
}

#define INSET      4
#define LEADING    2
#define MAX_LINES  64 /* paragraph is short and fixed; overflow is dropped */

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

  nlines = bmtext_layout(tcx->font,
                         tcx->text,
                         (int) strlen(tcx->text),
                         (bounds->x1 - INSET) - (bounds->x0 + INSET),
                         &tcx->spacing,
                         lines,
                         MAX_LINES);

  origin.x = bounds->x0 - sx + INSET;
  origin.y = bounds->y0 - sy + INSET;

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

      name = wuss_fontmenu_selected(tcx->fontmenu, event);
      if (name != NULL)
        return text_set_font(tcx, event->data.menu_select.index, name);
      if (event->data.menu_select.menu == &tcx->sample_menu)
        return text_set_sample(tcx, event->data.menu_select.index);
      if (event->data.menu_select.menu == &tcx->spacing_menu)
        return text_set_spacing(tcx, event->data.menu_select.index);
      picked = wuss_colourmenu_selected(tcx->fgmenu, event, &mine);
      if (mine)
        return text_set_fg(tcx, picked);
      picked = wuss_colourmenu_selected(tcx->bgmenu, event, &mine);
      if (mine)
        return text_set_bg(tcx, picked);
      if (event->data.menu_select.menu == &tcx->top_menu &&
          event->data.menu_select.index == TEXT_MENU_NO_BACKGROUND)
        return text_toggle_bg(tcx);
    }
    return result_OK;

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

    if (window == wuss_proginfo_window(tcx->proginfo))
      rc = wuss_proginfo_handle_pre_show(tcx->proginfo);
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
