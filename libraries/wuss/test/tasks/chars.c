/* chars.c -- wuss test - system font glyph grid task */

#ifdef USE_SDL

#include <stdlib.h>

#include <limits.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/point.h"

#include "chars.h"

#define CHARS_COLS 32
#define CHARS_ROWS 8
#define CHARS_PAD  2

result_t chars_create(wuss_t         *wuss,
                      const colour_t *palette,
                      chars_task_t   *task)
{
  wuss_task_t delegate;
  size2d_t    sz;
  bmfont_t   *font;
  int         font_width, font_height, cell_w, cell_h;

  font = wuss_get_font(wuss);
  if (font == NULL)
  {
    task->window = NULL;
    return result_OK;
  }

  bmfont_get_info(font, &font_width, &font_height);
  cell_w = font_width  + CHARS_PAD * 2;
  cell_h = font_height + CHARS_PAD * 2;

  task->font = font;
  task->fg   = palette[palette_PICO8_BLACK];
  task->bg   = palette[palette_PICO8_WHITE];

  delegate = wuss_task_start(chars_handle, task); /* chars_redraw paints every cell itself */
  sz       = SIZE2D(cell_w * CHARS_COLS, cell_h * CHARS_ROWS);

  return wuss_window_create_placed(wuss,
                                   sz,
                                   "Chars",
                                   wuss_WINDOW_NO_RESIZE      |
                                   wuss_WINDOW_NO_TOGGLE_SIZE |
                                   wuss_WINDOW_NO_VSCROLL     |
                                   wuss_WINDOW_NO_HSCROLL,
                                   wuss_NO_BACKGROUND,
                                   &delegate,
                                   sz,
                                   SIZE2D(0, 0),
                                   &task->window);
}

static result_t chars_redraw(const wuss_event_t *event, void *task_data)
{
  chars_task_t *cc;
  screen_t     *scr;
  const box_t  *bounds;
  int           font_width, font_height, cell_w, cell_h;
  int           first, count;
  int           i, sx, sy;

  cc = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  bmfont_get_info(cc->font, &font_width, &font_height);
  cell_w = font_width  + CHARS_PAD * 2;
  cell_h = font_height + CHARS_PAD * 2;

  first = ' '; /* bmfont glyphs are laid out contiguously starting here */
  count = bmfont_get_count(cc->font);
  /* bmfont indexes its glyph table off a plain char, so a byte value above
   * CHAR_MAX would index negatively on a signed-char platform. Never draw
   * one, however many glyphs the font claims. */
  if (first + count > CHAR_MAX + 1)
    count = CHAR_MAX + 1 - first;

  for (i = 0; i < CHARS_COLS * CHARS_ROWS; i++)
  {
    int     col, row, x, y;
    char    ch;
    point_t pos;

    col = i % CHARS_COLS;
    row = i / CHARS_COLS;
    x   = bounds->x0 - sx + col * cell_w;
    y   = bounds->y0 - sy + row * cell_h;

    screen_draw_rect(scr, x, y, SIZE2D(cell_w, cell_h), cc->bg);

    if (i < first || i >= first + count)
      continue; /* no glyph for this byte value: leave the cell blank */

    ch    = (char) i;
    pos.x = x + CHARS_PAD;
    pos.y = y + CHARS_PAD;
    bmfont_draw(cc->font, scr, &ch, 1, cc->fg, cc->bg, &pos, NULL);
  }

  return result_OK;
}

result_t chars_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  if (event->kind == wuss_EVENT_CLOSE)
  {
    wuss_window_close(window);
    free(task_data); /* calloc'd per instance by the spawner */
    return result_OK;
  }

  if (event->kind != wuss_EVENT_REDRAW)
    return result_OK;

  return chars_redraw(event, task_data);
}

#endif /* USE_SDL */
