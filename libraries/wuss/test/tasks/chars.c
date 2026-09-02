/* wuss/test/tasks/chars.c -- bitmap font glyph grid task with a font picker */

#ifdef WUSS_APP

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
#include "wuss/menu.h"

#include "chars.h"

#define CHARS_COLS 32
#define CHARS_ROWS 8
#define CHARS_PAD  2

/* the fonts the picker offers: menu label and the leafname under
 * resources/bmfonts. Index order is the menu order and the fonts[] slot. */
static const struct
{
  const char *label;
  const char *leaf;
}
chars_fonts[CHARS_NFONTS] =
{
  { "Digits",         "digits"       },
  { "Daydream",       "daydream"     },
  { "Glider Rider",   "gliderrider"  },
  { "Henry",          "henry"        },
  { "MS Sans Serif",  "ms-sans-serif"},
  { "Tall",           "tall"         },
  { "Tiny",           "tiny"         }
};

/* rebuilt before every open so the tick tracks task->current */
static wuss_menu_item_t chars_menu_items[CHARS_NFONTS];
static const wuss_menu_t chars_menu =
{
  "Font", chars_menu_items, CHARS_NFONTS
};

/* keep the resources root the task was created with; the picker needs it to
 * load a font on demand and chars_create does not stash it elsewhere.
 * ponytail: one demo, one instance at a time -- a file-scope copy is fine. */
static char chars_resources[256];

/* ----------------------------------------------------------------------- */

/* content size for the grid at the given font's cell metrics */
static size2d_t chars_window_size(bmfont_t *font)
{
  int fw, fh;

  bmfont_get_info(font, &fw, &fh);
  return SIZE2D((fw + CHARS_PAD * 2) * CHARS_COLS,
                (fh + CHARS_PAD * 2) * CHARS_ROWS);
}

/* load fonts[idx] if not already in hand; returns it or NULL on failure */
static bmfont_t *chars_load_font(chars_task_t *task, int idx)
{
  const char *leaf;
  const char *filename;
  bmfont_t   *font;
  result_t    rc;

  if (task->fonts[idx] != NULL)
    return task->fonts[idx];

  leaf     = path_join_leafname(chars_fonts[idx].leaf, "png");
  filename = path_join_filename(chars_resources, 3, "resources", "bmfonts",
                               leaf);

  rc = bmfont_create(filename, &font);
  if (rc != result_OK)
    return NULL;

  task->fonts[idx] = font;
  return font;
}

/* switch the grid to fonts[idx], resizing the window to suit */
static result_t chars_set_font(chars_task_t *task, int idx)
{
  bmfont_t *font;

  if (idx < 0 || idx >= CHARS_NFONTS || idx == task->current)
    return result_OK;

  font = chars_load_font(task, idx);
  if (font == NULL)
    return result_OK; /* leave the current font in place */

  task->font    = font;
  task->current = idx;

  wuss_window_resize(task->window, chars_window_size(font));
  wuss_window_invalidate_all(task->window);
  return result_OK;
}

static result_t chars_open_menu(chars_task_t *task)
{
  int i;

  for (i = 0; i < CHARS_NFONTS; i++)
  {
    chars_menu_items[i].text    = chars_fonts[i].label;
    chars_menu_items[i].flags   = (i == task->current) ? wuss_MENU_ITEM_TICKED
                                                       : wuss_MENU_ITEM_NONE;
    chars_menu_items[i].submenu = NULL;
    chars_menu_items[i].window  = NULL;
  }

  return wuss_menu_open(task->delegate, &chars_menu,
                        wuss_get_pointer(task->wuss), NULL);
}

/* ----------------------------------------------------------------------- */

result_t chars_create(wuss_t       *wuss,
                      const char   *resources,
                      chars_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t         rc;
  bmfont_t        *font;

  font = wuss_get_font(wuss);
  if (font == NULL)
  {
    task->window = NULL;
    return result_OK;
  }

  strncpy(chars_resources, resources, sizeof(chars_resources) - 1);
  chars_resources[sizeof(chars_resources) - 1] = '\0';

  task->wuss    = wuss;
  task->font    = font;
  task->current = -1; /* the wuss system font is not one of chars_fonts[] */
  task->fg      = colour_rgb(0x00, 0x00, 0x00);
  task->bg      = colour_rgb(0xFF, 0xFF, 0xFF);

  /* chars_redraw paints every cell itself */
  delegate_desc.handle    = chars_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "chars";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 chars_window_size(font),
                                 "Chars",
                                 wuss_WINDOW_NO_RESIZE      |
                                 wuss_WINDOW_NO_TOGGLE_SIZE |
                                 wuss_WINDOW_NO_VSCROLL     |
                                 wuss_WINDOW_NO_HSCROLL,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 chars_window_size(font),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */

  return rc;
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

    screen_fill_rect(scr, x, y, SIZE2D(cell_w, cell_h), cc->bg);

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
  chars_task_t *cc = task_data;

  NOT_USED(window);

  switch (event->kind)
  {
  case wuss_EVENT_QUIT:
    {
      int i;

      for (i = 0; i < CHARS_NFONTS; i++)
        if (cc->fonts[i] != NULL)
          bmfont_destroy(cc->fonts[i]);
      free(cc); /* calloc'd per instance by the spawner */
    }
    return result_OK;

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return chars_open_menu(cc);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &chars_menu)
      return chars_set_font(cc, event->data.menu_select.index);
    return result_OK;

  case wuss_EVENT_REDRAW:
    return chars_redraw(event, task_data);

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
