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
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/point.h"
#include "io/path.h"
#include "wuss/icon.h"
#include "wuss/menu.h"

#include "chars.h"

#define CHARS_COLS 16
#define CHARS_ROWS 16
#define CHARS_PAD  1

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in chars_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum { CHARS_MENU_INFO = 0, CHARS_MENU_FONT };

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

/* the rows of the CHARS_COLS x CHARS_ROWS grid that hold at least one glyph
 * of font: [*first_row, *first_row + *nrows). first_row * CHARS_COLS is the
 * byte value of the top-left cell of the first such row. */
static void chars_row_range(bmfont_t *font, int *first_row, int *nrows)
{
  int first, count, last_row;

  first = ' '; /* bmfont glyphs are laid out contiguously starting here */
  count = bmfont_get_count(font);
  /* bmfont indexes its glyph table off a plain char, so a byte value above
   * CHAR_MAX would index negatively on a signed-char platform. Never draw
   * one, however many glyphs the font claims. */
  if (first + count > CHAR_MAX + 1)
    count = CHAR_MAX + 1 - first;

  *first_row = first / CHARS_COLS;
  last_row   = (first + count - 1) / CHARS_COLS;
  *nrows     = last_row - *first_row + 1;
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
  chars_row_range(font, &first_row, &nrows);
  return SIZE2D(cell_w * CHARS_COLS + wuss_STD_INSET * 2,
               cell_h * nrows + wuss_STD_INSET * 2);
}

/* load fonts[idx] if not already in hand; returns it or NULL on failure.
 * name is the menu label for that row -- the font's leafname sans ".png". */
static bmfont_t *chars_load_font(chars_task_t *task,
                                 int           idx,
                                 const char   *name)
{
  result_t    rc;
  const char *resources;
  const char *filename;
  bmfont_t   *font;

  if (task->fonts[idx] != NULL)
    return task->fonts[idx];

  resources = wuss_get_resources(task->wuss);
  filename  = pathf("%s/resources/bmfonts/%s.png", resources, name);

  rc = bmfont_create(filename, &font);
  if (rc != result_OK)
    return NULL;

  task->fonts[idx] = font;
  return font;
}

/* switch the grid to the font at menu row idx (label name), resizing to suit */
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

  /* the grid's cell metrics scale with the font, so both the window and its
   * scrollable extent have to follow the new size -- resize alone would leave
   * the doc (and so the scroll range) sized to the font the window was
   * created with, clipping the far cells of a larger font unreachably */
  {
    size2d_t grid;

    grid = chars_window_size(task, font);
    wuss_window_resize(task->window, grid);
    wuss_window_set_doc(task->window, grid);
  }
  wuss_window_invalidate_visible(task->window);
  return result_OK;
}

static result_t chars_open_menu(chars_task_t *task)
{
  static const wuss_proginfo_desc_t desc =
  {
    "Chars",
    "Bitmap font glyph grid",
    "(c) DPTLib contributors",
    "1.0 (" __DATE__ ")"
  };

  wuss_proginfo_set_desc(&desc);
  task->menu_items[CHARS_MENU_INFO].window =
    wuss_proginfo_window(task->delegate);

  return wuss_menu_open(task->delegate, &task->menu,
                        wuss_get_pointer(task->wuss), &task->menu_handle);
}

/* "Font"'s hover: ticks the current font's row before handing the fontmenu
 * back as the submenu to open (task->current is -1 for the wuss system
 * font, which ticks nothing). "Info" has no retargeting to do. */
static result_t chars_pre_submenu_open(chars_task_t       *task,
                                       const wuss_event_t *event)
{
  wuss_menu_handle_t handle;
  int                index;

  handle = event->data.pre_submenu_open.handle;
  index  = event->data.pre_submenu_open.index;

  if (index == CHARS_MENU_FONT)
  {
    wuss_fontmenu_set_ticked(task->current);
    /* the fontmenu singleton may have been rebuilt (at a new address) by
     * another task since task->menu_items[index].submenu was cached in
     * chars_create -- re-fetch instead of handing the spawner a stale
     * pointer */
    return wuss_menu_open_submenu_now(handle, index, chars_fontmenu(task));
  }

  return wuss_menu_open_submenu_now(handle, index,
                                    task->menu_items[index].submenu);
}

/* ----------------------------------------------------------------------- */

result_t chars_create(wuss_t *wuss, chars_task_t **out)
{
  result_t           rc;
  chars_task_t      *task;
  wuss_task_t       *delegate;
  wuss_task_desc_t   delegate_desc;
  bmfont_t          *font;
  const wuss_menu_t *menu;
  size2d_t           grid;

  font = wuss_get_font(wuss);
  if (font == NULL)
    return result_OK; /* no window opened; nothing to free */

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->font        = font;
  task->current     = -1; /* the wuss system font is none of the picker's */
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

  WUSS_MENU_ITEM_WINDOW(task->menu_items, CHARS_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * chars_open_menu */

  WUSS_MENU_ITEM_MENU(task->menu_items, CHARS_MENU_FONT, "Font",
                      wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                      menu);

  WUSS_MENU_TITLE(task->menu, "Chars", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void chars_destroy(chars_task_t *task)
{
  int i;

  wuss_menu_close(task->menu_handle);
  task->menu_handle = NULL;

  for (i = 0; i < task->nfonts; i++)
    if (task->fonts[i] != NULL)
      bmfont_destroy(task->fonts[i]);
  free(task->fonts);
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
  int           first, count;
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

  first = ' '; /* bmfont glyphs are laid out contiguously starting here */
  count = bmfont_get_count(cc->font);
  /* bmfont indexes its glyph table off a plain char, so a byte value above
   * CHAR_MAX would index negatively on a signed-char platform. Never draw
   * one, however many glyphs the font claims. */
  if (first + count > CHAR_MAX + 1)
    count = CHAR_MAX + 1 - first;
  chars_row_range(cc->font, &first_row, &nrows);

  /* the wuss_STD_INSET margin around the grid falls outside every cell's own
   * fill below, so paint the whole dirty rect first -- covers that margin
   * and any dirty strip past the last row/column of cells too. */
  screen_fill_rect(scr, bounds->x0, bounds->y0,
                   SIZE2D(bounds->x1 - bounds->x0,
                          bounds->y1 - bounds->y0), cc->bg);

  for (i = 0; i < CHARS_COLS * nrows; i++)
  {
    int     col, row, x, y, byte;
    char    ch;
    char    label[4];
    point_t pos;

    col  = i % CHARS_COLS;
    row  = i / CHARS_COLS;
    byte = (first_row + row) * CHARS_COLS + col;
    x    = bounds->x0 - sx + wuss_STD_INSET + col * cell_w;
    y    = bounds->y0 - sy + wuss_STD_INSET + row * cell_h;

    screen_fill_rect(scr, x, y, SIZE2D(cell_w, cell_h), cc->bg);
    screen_draw_line(scr, x, y, x + cell_w - 1, y, cc->mg);
    screen_draw_line(scr, x, y, x, y + cell_h - 1, cc->mg);

    snprintf(label, 4, "%d", byte);
    pos.x = x + CHARS_PAD;
    pos.y = y + CHARS_PAD + sysfont_ascent;
    wuss_text_draw(cc->wuss, 0, scr, label, (int) strlen(label), cc->mg,
                   cc->bg, &pos, NULL);

    if (byte < first || byte >= first + count)
      continue; /* no glyph for this byte value: leave the cell blank */

    ch    = (char) byte;
    pos.x = x + CHARS_PAD;
    pos.y = y + CHARS_PAD * 2 + sysfont_height + font_ascent;
    screen_draw_dashed_line(scr, x + CHARS_PAD, pos.y,
                            x + cell_w - 1 - CHARS_PAD, pos.y, 1, 1, cc->mg);
    bmfont_draw(cc->font, scr, &ch, 1, cc->fg, cc->bg, NULL, &pos, NULL);

    /* advance width: a blue rule under the glyph spanning pos.x..pos.x+advance,
     * a ruler for how far this glyph pushes the pen */
    {
      bmfont_width_t advance;
      colour_t       blue;
      int            adv_y;

      bmfont_measure(cc->font, &ch, 1, NULL, INT_MAX, NULL, &advance);
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
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return chars_open_menu(cc);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    {
      result_t    rc;
      const char *name;

      name = wuss_fontmenu_selected(event);
      if (name == NULL)
        return result_OK;

      rc = chars_set_font(cc, event->data.menu_select.index, name);

      if (wuss_menu_should_keep_open(event))
        /* ADJUST keeps the chain open without rebuilding it, so the
         * fresh-open tick set in chars_open_menu is now stale on screen;
         * retick the still-open chain in place to match cc->current. Use
         * the handle's own live menu, not a fresh chars_fontmenu(cc) --
         * that can rebuild the fontmenu singleton at a new address while
         * this handle's chain node still points at the old one, which
         * would leave the open submenu's node->menu dangling. */
        wuss_menu_tick_exclusive_live(cc->menu_handle,
                                      wuss_menu_handle_menu(cc->menu_handle),
                                      cc->current);
      else
        /* SELECT has already closed and freed the chain by the time this
         * event arrives; the handle is stale, don't touch it */
        cc->menu_handle = NULL;

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

  case wuss_EVENT_REDRAW:
    return chars_redraw(event, task_data);

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
