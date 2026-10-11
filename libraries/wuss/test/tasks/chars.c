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
#include "taskapp.h"
#include "common.h"
#include "snapshot.h"

#define CHARS_COLS 16 /* initial columns; see chars_task_t.cols */
#define CHARS_PAD  1

#define CHARS_PAGE_SIZE  256
#define CHARS_LAST_PAGE  0x1FF00ul /* scan up to the end of plane 1 */

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in chars_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  CHARS_MENU_FONT = 0,
  CHARS_MENU_PAGE,
  CHARS_MENU_SAVE
};

#define CHARS_SAVE_NAME "chars.png" /* Save As's initial leafname */

#define CHARS_INDEX_FONT "Tiny Regular" /* the font for the cell numbers */

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
 * glyphs at all gives one blank row. The grid is cols cells wide. */
static void chars_row_range(bmfont_t     *font,
                            unsigned long page,
                            int           cols,
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

  *first_row = first / cols;
  *nrows     = last / cols - *first_row + 1;
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

/* the font the cell numbers are drawn in */
static bmfont_t *chars_index_font(const chars_task_t *task)
{
  if (task->index_font != NULL)
    return task->index_font;

  return wuss_get_font(task->wuss);
}

/* cell size at the given font's metrics. Each cell stacks the index (drawn
 * in the index font) above the glyph. */
static void chars_cell_size(chars_task_t *task,
                            bmfont_t     *font,
                            int          *cell_w,
                            int          *cell_h)
{
  int fw, fh, ifw, ifh;

  bmfont_get_info(font, &fw, &fh, NULL, NULL);
  bmfont_get_info(chars_index_font(task), &ifw, &ifh, NULL, NULL);
  *cell_w = MAX(fw, ifw * 3) + CHARS_PAD * 2;
  *cell_h = ifh + fh + CHARS_PAD * 3;
}

/* content size for the grid at task->cols, less any contiguous
 * leading/trailing rows the font has no glyphs in */
static size2d_t chars_window_size(chars_task_t *task, bmfont_t *font)
{
  int cell_w, cell_h;
  int first_row, nrows;

  chars_cell_size(task, font, &cell_w, &cell_h);
  chars_row_range(font, task->page, task->cols, &first_row, &nrows);
  return SIZE2D(cell_w * task->cols + wuss_STD_INSET * 2,
               cell_h * nrows + wuss_STD_INSET * 2);
}

/* largest power-of-two column count whose grid fits fit_w pixels, at least 1
 * and at most one row of a page */
static int chars_cols_for_width(int cell_w, int fit_w)
{
  int cols;

  cols = 1;
  while ((cols * 2) * cell_w + wuss_STD_INSET * 2 <= fit_w &&
         cols * 2 <= CHARS_PAGE_SIZE)
    cols *= 2;

  return cols;
}

/* chars_task_t.gv's item callback: cells are drawn by chars_draw_cell, so
 * there is no glyph or label to supply */
static void chars_item(int index, void *opaque, wuss_gridview_item_t *out)
{
  NOT_USED(index);
  NOT_USED(opaque);

  out->glyph = NULL;
  out->label = NULL;
}

/* the cell size at the shown font, handed to the gridview */
static void chars_set_cell(chars_task_t *task)
{
  int cell_w, cell_h;

  chars_cell_size(task, task->font, &cell_w, &cell_h);
  wuss_gridview_set_cell_size(task->gv, SIZE2D(cell_w, cell_h));
}

/* the gridview chose a column count (or the font or page changed): keep the
 * rows that hold glyphs, and so the item count and the first item's
 * codepoint, to suit. Columns are not capped by the item count (see
 * chars_create), so the new count cannot stop the grid growing wider. */
static void chars_sync(chars_task_t *task)
{
  int first_row, nrows;

  task->cols = wuss_gridview_get_columns(task->gv);
  chars_row_range(task->font, task->page, task->cols, &first_row, &nrows);
  task->rows   = nrows;
  task->offset = first_row * task->cols;
  (void) wuss_gridview_set_count(task->gv, nrows * task->cols);
}

/* offer a window event to the gridview and follow any change of columns */
static void chars_relayout(chars_task_t *task, const wuss_event_t *event)
{
  (void) wuss_gridview_handle_event(task->gv, event);
  if (wuss_gridview_get_columns(task->gv) != task->cols)
    chars_sync(task);
}

/* acquire the font labelled name -- "Family Style" -- from the font cache;
 * returns it or NULL on failure */
static bmfont_t *chars_load_named(chars_task_t *task, const char *name)
{
  result_t    rc;
  const char *resources;
  const char *dir;
  char        filename[512];
  bmfont_t   *font;

  resources = wuss_get_resources(task->wuss);
  dir       = pathf("%s/resources/bmfonts", resources);

  rc = bmfontfamily_label_path(dir, name, filename, sizeof(filename));
  if (rc != result_OK)
    return NULL;

  rc = bmfontcache_acquire(wuss_get_font_cache(task->wuss), filename, &font);
  if (rc != result_OK)
    return NULL;

  return font;
}

/* load fonts[idx] if not already in hand; returns it or NULL on failure.
 * name is the menu label for that row -- "Family Style". */
static bmfont_t *chars_load_font(chars_task_t *task,
                                 int           idx,
                                 const char   *name)
{
  if (task->fonts[idx] == NULL)
    task->fonts[idx] = chars_load_named(task, name);

  return task->fonts[idx];
}

/* the grid's cell metrics scale with the font and its row count with the
 * page, so the gridview's cell and count have to follow and the window has to
 * take the new natural size */
static void chars_resize(chars_task_t *task)
{
  chars_set_cell(task);
  chars_sync(task);
  wuss_window_resize(task->window,
                     wuss_gridview_natural_size(task->gv, task->cols,
                                                CHARS_PAGE_SIZE));
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

static wuss_gridview_draw_fn_t chars_draw_cell;

/* ----------------------------------------------------------------------- */

result_t chars_create(wuss_t *wuss, chars_task_t **out)
{
  result_t           rc;
  chars_task_t      *task;
  wuss_task_t       *delegate;
  wuss_task_desc_t   delegate_desc;
  filetype_t         png_type;
  bmfont_t          *font;
  const wuss_menu_t *menu;
  const char        *sysname;
  int                i;
  size2d_t           grid;
  int                cell_w, cell_h;

  font = wuss_get_font(wuss);
  if (font == NULL)
    return result_OK; /* no window opened; nothing to free */

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->font        = font;
  task->cols        = CHARS_COLS;
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

  /* optional: without it the numbers use the wuss system font */
  task->index_font = chars_load_named(task, CHARS_INDEX_FONT);

  /* 16 columns may not fit a small screen; OPEN is not sent at create, so cap
   * here and rewrap against the real (screen-capped) content size after */
  chars_cell_size(task, font, &cell_w, &cell_h);
  task->cols = MIN(task->cols, chars_cols_for_width(cell_w,
                   wuss_get_screen_size(wuss).w));
  grid = chars_window_size(task, font);
  rc = wuss_window_create_at(delegate,
                             &task_place,
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

  /* columns are limited by the screen and window, not by the item count,
   * which follows the columns (chars_sync) */
  rc = wuss_gridview_create(&task->gv, task->window, CHARS_PAGE_SIZE,
                            chars_item, 1, NULL, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  wuss_gridview_set_layout(task->gv, CHARS_PAGE_SIZE, 1, wuss_STD_INSET);
  wuss_gridview_set_draw(task->gv, chars_draw_cell, 0);
  wuss_gridview_set_background(task->gv, task->bg);
  chars_set_cell(task);
  chars_sync(task);

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type,
                          CHARS_SAVE_NAME, chars_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

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

result_t chars_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Chars",
    "Font glyph grid",
    (taskapp_create_fn_t) chars_create
  };

  return taskapp_install(wuss, &desc);
}

void chars_destroy(chars_task_t *task)
{
  int i;

  for (i = 0; i < task->nfonts; i++)
    if (task->fonts[i] != NULL)
      bmfontcache_release(wuss_get_font_cache(task->wuss), task->fonts[i]);
  free(task->fonts);
  if (task->index_font != NULL)
    bmfontcache_release(wuss_get_font_cache(task->wuss), task->index_font);

  wuss_gridview_destroy(task->gv);
  wuss_saveas_destroy(task->saveas);
  free(task);
}

/* the lines on the top and left edges of every cell, drawn as one rule per
 * row and column across the whole grid rather than two per cell. Only those
 * the redraw clip reaches are drawn. */
static void chars_draw_grid(const chars_task_t *cc,
                            const wuss_event_t *event)
{
  screen_t *scr;
  box_t     cell;
  int       x, y, cell_w, cell_h;
  int       first, last, i;

  scr    = event->data.redraw.scr;
  cell   = wuss_gridview_cell_box(cc->gv, 0);
  cell_w = cell.x1 - cell.x0;
  cell_h = cell.y1 - cell.y0;
  x      = cell.x0 + event->data.redraw.bounds->x0 - event->data.redraw.scroll.x;
  y      = cell.y0 + event->data.redraw.bounds->y0 - event->data.redraw.scroll.y;

  first = MAX((scr->clip.x0 - x) / cell_w, 0);
  last  = MIN((scr->clip.x1 - x) / cell_w, cc->cols - 1);
  for (i = first; i <= last; i++)
    screen_fill_rect(scr, x + i * cell_w, y, SIZE2D(1, cell_h * cc->rows),
                     cc->mg);

  first = MAX((scr->clip.y0 - y) / cell_h, 0);
  last  = MIN((scr->clip.y1 - y) / cell_h, cc->rows - 1);
  for (i = first; i <= last; i++)
    screen_fill_rect(scr, x, y + i * cell_h, SIZE2D(cell_w * cc->cols, 1),
                     cc->mg);
}

/* wuss_gridview_draw_fn_t: one cell, the index above the glyph, with a rule
 * for the baseline and a blue rule for the glyph's advance width. The cell's
 * background and edges are left to chars_draw_grid and the gridview. */
static void chars_draw_cell(wuss_gridview_t    *gv,
                            const wuss_event_t *event,
                            int                 index,
                            const box_t        *screen_box,
                            void               *opaque)
{
  chars_task_t  *cc;
  screen_t      *scr;
  bmfont_t      *idxfont;
  int            font_ascent, idxfont_height, idxfont_ascent;
  int            cell_w, cell_h, x, y, len;
  unsigned long  cp;
  char           label[4];
  char           utf8[4];
  point_t        pos;
  bmfont_width_t advance;
  colour_t       blue;
  int            adv_y;

  NOT_USED(gv);

  cc  = opaque;
  scr = event->data.redraw.scr;

  idxfont = chars_index_font(cc);
  bmfont_get_info(cc->font, NULL, NULL, &font_ascent, NULL);
  bmfont_get_info(idxfont, NULL, &idxfont_height, &idxfont_ascent, NULL);

  x      = screen_box->x0;
  y      = screen_box->y0;
  cell_w = screen_box->x1 - screen_box->x0;
  cell_h = screen_box->y1 - screen_box->y0;
  cp     = cc->page + (unsigned long) (cc->offset + index);

  /* the gridview has already filled the background, and chars_draw_grid
   * draws the cell edges */
  snprintf(label, 4, "%02lX", cp & 0xFF);
  pos.x = x + CHARS_PAD;
  pos.y = y + CHARS_PAD + idxfont_ascent;
  bmfont_draw(idxfont, scr, label, (int) strlen(label), cc->mg, cc->bg, NULL,
              &pos, NULL);

  if (bmfont_lookup(cc->font, cp) < 0)
    return; /* no glyph for this codepoint: leave the cell blank */

  len   = utf8_encode(cp, utf8);
  pos.x = x + CHARS_PAD;
  pos.y = y + CHARS_PAD * 2 + idxfont_height + font_ascent;
  screen_draw_dashed_line(scr, x + CHARS_PAD, pos.y, x + cell_w - 1 - CHARS_PAD,
                          pos.y, 1, 1, cc->mg);
  bmfont_draw(cc->font, scr, utf8, len, cc->fg, cc->bg, NULL, &pos, NULL);

  bmfont_measure(cc->font, utf8, len, NULL, INT_MAX, NULL, &advance);
  blue  = colour_rgb(0x66, 0x66, 0xFF);
  adv_y = y + cell_h - 1 - CHARS_PAD;
  screen_draw_line(scr, pos.x, adv_y, pos.x + advance - 1, adv_y, blue);
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

  case wuss_EVENT_OPEN:
  case wuss_EVENT_RELAYOUT:
    if (window == cc->window)
      chars_relayout(cc, event);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    {
      result_t    rc;
      const char *name;

      if (event->data.menu_select.menu == &cc->page_menu)
        return chars_set_page(cc, event);

      if (event->data.menu_select.menu == &cc->menu &&
          event->data.menu_select.index == CHARS_MENU_SAVE)
      {
        return wuss_saveas_open(cc->saveas);
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

  case wuss_EVENT_KEY:
    if (window != cc->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */
    return wuss_menu_dispatch_shortcut(cc->delegate, &cc->menu, event);

  case wuss_EVENT_REDRAW:
    if (window == cc->window)
    {
      (void) wuss_gridview_handle_event(cc->gv, event);
      chars_draw_grid(cc, event);
    }
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
