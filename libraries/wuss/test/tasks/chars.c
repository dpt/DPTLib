/* wuss/test/tasks/chars.c -- bitmap font glyph grid task with a font picker */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bmfontfamily.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/point.h"
#include "io/filetype.h"
#include "io/path.h"
#include "text/utf8.h"
#include "wuss/icon.h"
#include "wuss/menu.h"

#include "chars.h"
#include "common.h"
#include "snapshot.h"

#define CHARS_COLS 16
#define CHARS_ROWS 16
#define CHARS_PAD  1

#define CHARS_PAGE_SIZE  (CHARS_COLS * CHARS_ROWS)
#define CHARS_LAST_PAGE  0x1FF00ul /* scan up to the end of plane 1 */

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in chars_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  CHARS_MENU_INFO = 0,
  CHARS_MENU_FONT,
  CHARS_MENU_PAGE,
  CHARS_MENU_SAVE
};

#define CHARS_SAVE_NAME "chars.png" /* Save As's initial leafname */

/* ----------------------------------------------------------------------- */

/* the shared fontmenu singleton, retargeted at task->wuss's bmfonts dir --
 * cheap to call repeatedly since wuss_fontmenu_menu only rebuilds when the
 * dir or wuss_t actually changes */
static const wuss_menu_t *chars_fontmenu(chars_task_t *task)
{
  const char *resources;
  const char *bmfonts_dir;

  resources   = wuss_get_resources(task->wuss);
  bmfonts_dir = pathf("%s/resources/bmfonts", resources);
  return wuss_fontmenu_menu(bmfonts_dir, "Font", task->wuss);
}

/* the rows of the grid for page that hold at least one glyph of font:
 * [*first_row, *first_row + *nrows). page + first_row * CHARS_COLS is the
 * codepoint of the top-left cell of the first such row. A page with no
 * glyphs at all gives one blank row. */
static void chars_row_range(bmfont_t     *font,
                            unsigned long page,
                            int          *first_row,
                            int          *nrows)
{
  int first, last, i;

  first = -1;
  last  = -1;
  for (i = 0; i < CHARS_PAGE_SIZE; i++)
  {
    if (bmfont_lookup(font, page + (unsigned long) i) < 0)
      continue;

    if (first < 0)
      first = i;
    last = i;
  }

  if (first < 0)
  {
    *first_row = 0;
    *nrows     = 1;
    return;
  }

  *first_row = first / CHARS_COLS;
  *nrows     = last / CHARS_COLS - *first_row + 1;
}

/* whether font has a glyph for any codepoint in page */
static int chars_page_has_glyphs(bmfont_t *font, unsigned long page)
{
  int i;

  for (i = 0; i < CHARS_PAGE_SIZE; i++)
    if (bmfont_lookup(font, page + (unsigned long) i) >= 0)
      return 1;

  return 0;
}

/* content size for the grid at the given font's cell metrics, less any
 * contiguous leading/trailing rows the font has no glyphs in. Each cell
 * stacks the index (drawn in the wuss system font) above the glyph. */
static size2d_t chars_window_size(chars_task_t *task, bmfont_t *font)
{
  int fw, fh, ifw, ifh, cell_w, cell_h;
  int first_row, nrows;

  bmfont_get_info(font, &fw, &fh, NULL, NULL);
  bmfont_get_info(wuss_get_font(task->wuss), &ifw, &ifh, NULL, NULL);
  cell_w = MAX(fw, ifw * 3) + CHARS_PAD * 2;
  cell_h = ifh + fh + CHARS_PAD * 3;
  chars_row_range(font, task->page, &first_row, &nrows);
  return SIZE2D(cell_w * CHARS_COLS + wuss_STD_INSET * 2,
               cell_h * nrows + wuss_STD_INSET * 2);
}

/* load fonts[idx] if not already in hand; returns it or NULL on failure.
 * name is the menu label for that row -- "Family Style". */
static bmfont_t *chars_load_font(chars_task_t *task,
                                 int           idx,
                                 const char   *name)
{
  result_t    rc;
  const char *resources;
  const char *dir;
  char        filename[512];
  bmfont_t   *font;

  if (task->fonts[idx] != NULL)
    return task->fonts[idx];

  resources = wuss_get_resources(task->wuss);
  dir       = pathf("%s/resources/bmfonts", resources);

  rc = bmfontfamily_label_path(dir, name, filename, sizeof(filename));
  if (rc != result_OK)
    return NULL;

  rc = bmfontcache_acquire(wuss_get_font_cache(task->wuss), filename, &font);
  if (rc != result_OK)
    return NULL;

  task->fonts[idx] = font;
  return font;
}

/* the grid's cell metrics scale with the font and its row count with the
 * page, so both the window and its scrollable extent have to follow -- resize
 * alone would leave the doc (and so the scroll range) sized to the old grid,
 * clipping the far cells of a larger one unreachably */
static void chars_resize(chars_task_t *task)
{
  size2d_t grid;

  grid = chars_window_size(task, task->font);
  wuss_window_resize(task->window, grid);
  wuss_window_set_doc(task->window, grid);
  wuss_window_invalidate_visible(task->window);
}

/* switch the grid to the font at menu row idx (label name), resizing to suit;
 * falls back to page 0 if the new font has nothing on the page shown */
static result_t chars_set_font(chars_task_t *task, int idx, const char *name)
{
  bmfont_t *font;

  if (idx < 0 || idx >= task->nfonts || idx == task->current)
    return result_OK;

  font = chars_load_font(task, idx, name);
  if (font == NULL)
    return result_OK; /* leave the current font in place */

  task->font    = font;
  task->current = idx;
  if (!chars_page_has_glyphs(font, task->page))
    task->page = 0;

  chars_resize(task);
  return result_OK;
}

/* switch the grid to "Page" row idx */
static result_t chars_set_page(chars_task_t       *task,
                               const wuss_event_t *event)
{
  int idx;

  idx = event->data.menu_select.index;
  if (idx < 0 || idx >= task->page_menu.nitems)
    return result_OK;

  task->page = task->page_bases[idx];
  chars_resize(task);

  wuss_menu_tick_exclusive_live(task->menu_handle, &task->page_menu, idx);

  return result_OK;
}

/* step the grid to the next (dir 1) or previous (dir -1) page in which the
 * current font has any glyph, wrapping at either end; stays put if no other
 * page has glyphs */
static void chars_step_page(chars_task_t *task, int dir)
{
  static const unsigned long npages = CHARS_LAST_PAGE / CHARS_PAGE_SIZE + 1;

  unsigned long index;
  unsigned long i;

  index = task->page / CHARS_PAGE_SIZE;
  for (i = 1; i < npages; i++)
  {
    unsigned long page;

    page = ((index + (dir > 0 ? i : npages - i)) % npages) * CHARS_PAGE_SIZE;
    if (chars_page_has_glyphs(task->font, page))
    {
      task->page = page;
      chars_resize(task);
      return;
    }
  }
}

/* rebuild the "Page" submenu: one row per page, up to CHARS_MAX_PAGES, in
 * which the current font has any glyph, with the shown page ticked.
 * ponytail: probes every codepoint up to CHARS_LAST_PAGE (a bsearch each,
 * ~130k per open); walk the cmap groups directly if that ever shows up. */
static void chars_build_page_menu(chars_task_t *task)
{
  unsigned long page;
  int           n;
  int           ticked;

  n      = 0;
  ticked = -1;
  for (page = 0; page <= CHARS_LAST_PAGE && n < CHARS_MAX_PAGES;
       page += CHARS_PAGE_SIZE)
  {
    if (!chars_page_has_glyphs(task->font, page))
      continue;

    snprintf(task->page_labels[n], sizeof(task->page_labels[n]), "U+%04lX",
             page);
    task->page_bases[n] = page;
    WUSS_MENU_ITEM(task->page_items, n, task->page_labels[n],
                   wuss_MENU_ITEM_NONE);
    if (page == task->page)
      ticked = n;
    n++;
  }

  WUSS_MENU_TITLE(task->page_menu, "Page", task->page_items, n);
  if (ticked >= 0)
    wuss_menu_tick_exclusive(&task->page_menu, ticked);
}

static result_t chars_open_menu(chars_task_t *task)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Chars", "Bitmap font glyph grid");

  wuss_proginfo_set_desc(&desc);
  task->menu_items[CHARS_MENU_INFO].window =
    wuss_proginfo_window(task->delegate);

  return wuss_menu_open_at_pointer(task->delegate, &task->menu,
                                   &task->menu_handle);
}

/* "Font"'s hover: ticks the current font's row before handing the fontmenu
 * back as the submenu to open (task->current is -1 if the wuss system font
 * is not listed, which ticks nothing). "Info" has no retargeting to do. */
static result_t chars_pre_submenu_open(chars_task_t       *task,
                                       const wuss_event_t *event)
{
  wuss_menu_handle_t handle;
  int                index;

  handle = event->data.pre_submenu_open.handle;
  index  = event->data.pre_submenu_open.index;

  if (index == CHARS_MENU_FONT)
  {
    const wuss_menu_t *fontmenu;

    /* the fontmenu singleton may have been rebuilt (at a new address) by
     * another task since task->menu_items[index].submenu was cached in
     * chars_create -- re-fetch instead of handing the spawner a stale
     * pointer, and before ticking, as a rebuild would drop the tick */
    fontmenu = chars_fontmenu(task);
    wuss_fontmenu_set_ticked(task->current);
    return wuss_menu_open_submenu_now(handle, index, fontmenu);
  }

  if (index == CHARS_MENU_PAGE)
    chars_build_page_menu(task);

  return wuss_menu_open_submenu_now(handle, index,
                                    task->menu_items[index].submenu);
}

/* wuss_saveas_save_fn_t: opaque is the chars_task_t */
static result_t chars_saveas_save(const char *path, void *opaque)
{
  chars_task_t *cc;

  cc = opaque;

  return snapshot_save_png(cc->window, chars_handle, cc, path);
}

/* the Save As dialogue's own task: forwards every event on its window into
 * wuss_saveas_handle_event. Not autoclose, and does nothing on QUIT -- the
 * chars_task_t block belongs to task->delegate's lifecycle, freed there,
 * not here. */
static result_t chars_saveas_handle(wuss_window_t      *window,
                                    const wuss_event_t *event,
                                    void               *task_data)
{
  chars_task_t *cc;

  cc = task_data;

  if (event->kind == wuss_EVENT_QUIT)
    return result_OK;

  (void) wuss_saveas_handle_event(cc->saveas, window, event);

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t chars_create(wuss_t *wuss, chars_task_t **out)
{
  result_t           rc;
  chars_task_t      *task;
  wuss_task_t       *delegate;
  wuss_task_desc_t   delegate_desc;
  wuss_task_desc_t   saveas_desc;
  filetype_t         png_type;
  bmfont_t          *font;
  const wuss_menu_t *menu;
  const char        *sysname;
  int                i;
  size2d_t           grid;

  font = wuss_get_font(wuss);
  if (font == NULL)
    return result_OK; /* no window opened; nothing to free */

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->font        = font;
  task->current     = -1; /* until the system font's row is found below */
  task->menu_handle = NULL;
  task->fg          = colour_rgb(0x00, 0x00, 0x00);
  task->mg          = colour_rgb(0xBB, 0xBB, 0xBB);
  task->bg          = colour_rgb(0xFF, 0xFF, 0xFF);

  /* the shared picker: every ".png" font under resources/bmfonts, sorted,
   * less any SYSTEM-class font (e.g. the one wuss draws menu ticks/arrows
   * from) */
  menu = chars_fontmenu(task);
  if (menu == NULL)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return result_OOM;
  }

  task->nfonts  = menu->nitems;
  task->fonts   = calloc((size_t) task->nfonts, sizeof(*task->fonts));
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

  /* chars_redraw paints every cell itself */
  delegate_desc.handle    = chars_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "chars";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    /* nothing registered yet; the spawner will not free it */
    free(task->fonts);
    free(task);
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  grid = chars_window_size(task, font);
  rc = wuss_window_create_placed(delegate,
                                 grid,
                                 "Chars",
                                 wuss_WINDOW_DEFAULT & ~wuss_WINDOW_HSCROLL,
                                 wuss_NO_BACKDROP,
                                 grid,
                                 SIZE2D(64, 64),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  /* separate, non-autoclose task: wuss_saveas_create forbids an autoclose
   * owner, and delegate (above) is one */
  saveas_desc.handle    = chars_saveas_handle;
  saveas_desc.task_data = task;
  saveas_desc.name      = "chars-saveas";
  rc = wuss_task_create(wuss, &saveas_desc, &task->saveas_task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, task->saveas_task, &png_type,
                          CHARS_SAVE_NAME, chars_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->saveas_task);
    wuss_task_destroy(delegate);
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, CHARS_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * chars_open_menu */

  WUSS_MENU_ITEM_MENU(task->menu_items, CHARS_MENU_FONT, "Font",
                      wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                      menu);

  /* rows filled in by chars_build_page_menu each time it opens */
  WUSS_MENU_ITEM_MENU(task->menu_items, CHARS_MENU_PAGE, "Page",
                      wuss_MENU_ITEM_PRE_OPEN, &task->page_menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CHARS_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");
  /* hover opens the Save As dialogue as a submenu; ^S shows it standalone */
  task->menu_items[CHARS_MENU_SAVE].window = wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Chars", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void chars_destroy(chars_task_t *task)
{
  int i;

  for (i = 0; i < task->nfonts; i++)
    if (task->fonts[i] != NULL)
      bmfontcache_release(wuss_get_font_cache(task->wuss), task->fonts[i]);
  free(task->fonts);

  wuss_saveas_destroy(task->saveas);
  wuss_task_destroy(task->saveas_task);
  free(task);
}

static result_t chars_redraw(const wuss_event_t *event, void *task_data)
{
  chars_task_t *cc;
  screen_t     *scr;
  bmfont_t     *sysfont;
  const box_t  *bounds;
  int           font_width, font_height, font_ascent;
  int           sysfont_width, sysfont_height, sysfont_ascent;
  int           cell_w, cell_h;
  int           first_row, nrows;
  int           i, sx, sy;

  cc = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  sysfont = wuss_get_font(cc->wuss);

  bmfont_get_info(cc->font, &font_width, &font_height, &font_ascent, NULL);
  bmfont_get_info(sysfont, &sysfont_width, &sysfont_height, &sysfont_ascent,
                  NULL);
  cell_w = MAX(font_width, sysfont_width * 3) + CHARS_PAD * 2;
  cell_h = sysfont_height + font_height + CHARS_PAD * 3;

  chars_row_range(cc->font, cc->page, &first_row, &nrows);

  /* the wuss_STD_INSET margin around the grid falls outside every cell's own
   * fill below, so paint the whole dirty rect first -- covers that margin
   * and any dirty strip past the last row/column of cells too. */
  screen_fill_rect(scr, bounds->x0, bounds->y0,
                   SIZE2D(bounds->x1 - bounds->x0,
                          bounds->y1 - bounds->y0), cc->bg);

  for (i = 0; i < CHARS_COLS * nrows; i++)
  {
    int           col, row, x, y;
    unsigned long cp;
    char          label[4];
    point_t       pos;
    char          utf8[4];
    int           len;

    col  = i % CHARS_COLS;
    row  = i / CHARS_COLS;
    cp   = cc->page + (unsigned long) ((first_row + row) * CHARS_COLS + col);
    x    = bounds->x0 - sx + wuss_STD_INSET + col * cell_w;
    y    = bounds->y0 - sy + wuss_STD_INSET + row * cell_h;

    screen_fill_rect(scr, x, y, SIZE2D(cell_w, cell_h), cc->bg);
    screen_draw_line(scr, x, y, x + cell_w - 1, y, cc->mg);
    screen_draw_line(scr, x, y, x, y + cell_h - 1, cc->mg);

    snprintf(label, 4, "%02lX", cp & 0xFF);
    pos.x = x + CHARS_PAD;
    pos.y = y + CHARS_PAD + sysfont_ascent;
    wuss_text_draw(cc->wuss, 0, scr, label, (int) strlen(label), cc->mg,
                   cc->bg, &pos, NULL);

    if (bmfont_lookup(cc->font, cp) < 0)
      continue; /* no glyph for this codepoint: leave the cell blank */

    len   = utf8_encode(cp, utf8);
    pos.x = x + CHARS_PAD;
    pos.y = y + CHARS_PAD * 2 + sysfont_height + font_ascent;
    screen_draw_dashed_line(scr, x + CHARS_PAD, pos.y,
                            x + cell_w - 1 - CHARS_PAD, pos.y, 1, 1, cc->mg);
    bmfont_draw(cc->font, scr, utf8, len, cc->fg, cc->bg, NULL, &pos, NULL);

    /* advance width: a blue rule under the glyph spanning pos.x..pos.x+advance,
     * a ruler for how far this glyph pushes the pen */
    {
      bmfont_width_t advance;
      colour_t       blue;
      int            adv_y;

      bmfont_measure(cc->font, utf8, len, NULL, INT_MAX, NULL, &advance);
      blue  = colour_rgb(0x66, 0x66, 0xFF);
      adv_y = y + cell_h - 1 - CHARS_PAD;
      screen_draw_line(scr, pos.x, adv_y, pos.x + advance - 1, adv_y, blue);
    }
  }

  return result_OK;
}

result_t chars_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  chars_task_t *cc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_QUIT:
    chars_destroy(cc);
    return result_OK;

  case wuss_EVENT_MOUSE:
    if (window != cc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;

    if (event->data.mouse.button & wuss_BUTTON_MENU)
      return chars_open_menu(cc);
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      chars_step_page(cc, 1);
    else if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      chars_step_page(cc, -1);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    {
      result_t       rc;
      wuss_window_t *saveas_win;
      const char    *name;

      if (event->data.menu_select.menu == &cc->page_menu)
        return chars_set_page(cc, event);

      if (event->data.menu_select.menu == &cc->menu &&
          event->data.menu_select.index == CHARS_MENU_SAVE)
      {
        saveas_win = wuss_saveas_window(cc->saveas);
        wuss_window_set_hidden(saveas_win, 0);
        wuss_window_restack(saveas_win, wuss_ZORDER_FRONT);

        return result_OK;
      }

      name = wuss_fontmenu_selected(event);
      if (name == NULL)
        return result_OK;

      rc = chars_set_font(cc, event->data.menu_select.index, name);

      /* ADJUST keeps the chain open without rebuilding it, so the fresh-open
       * tick set in chars_pre_submenu_open is now stale on screen; retick
       * the still-open font level in place to match cc->current (a no-op
       * once a SELECT pick has closed the chain). Use the event's own menu,
       * not a fresh chars_fontmenu(cc) -- that can rebuild the fontmenu
       * singleton at a new address while this handle's chain node still
       * points at the old one, which would leave the open submenu's
       * node->menu dangling. */
      wuss_menu_tick_exclusive_live(cc->menu_handle,
                                    event->data.menu_select.menu,
                                    cc->current);

      return rc;
    }

  case wuss_EVENT_MENU_CLOSED:
    cc->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return chars_pre_submenu_open(cc, event);

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == cc->menu_items[CHARS_MENU_INFO].window)
    {
      rc = wuss_proginfo_handle_pre_show();
      if (rc != result_OK)
        return rc;
    }

    if (event->data.pre_show.handle == NULL)
      return result_OK;

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_KEY:
    if (window != cc->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */
    return wuss_menu_dispatch_shortcut(cc->delegate, &cc->menu, event);

  case wuss_EVENT_REDRAW:
    return chars_redraw(event, task_data);

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
