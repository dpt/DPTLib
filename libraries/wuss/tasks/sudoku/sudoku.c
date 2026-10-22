/* wuss/tasks/sudoku/sudoku.c -- sudoku task */

#ifdef WUSS_APP

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/screen.h"
#include "geom/point.h"

#include "sudoku.h"
#include "../common/taskapp.h"
#include "../common/common.h"

#define SUDOKU_CELL   20 /* pixels per cell */
#define SUDOKU_BORDER 2  /* frame round the board */
#define SUDOKU_HOLES  50 /* cells emptied when the puzzle is still unique */

/* left (or top) edge of cell i: one pixel between cells, two between boxes */
#define SUDOKU_POS(i) (SUDOKU_BORDER + (i) * (SUDOKU_CELL + 1) + (i) / 3)
#define SUDOKU_SIZE   (SUDOKU_POS(8) + SUDOKU_CELL + SUDOKU_BORDER)

enum
{
  SUDOKU_MENU_NEW,
  SUDOKU_MENU_RESTART
};

/* ----------------------------------------------------------------------- */

static void sudoku_shuffle(rng_t *rng, int *a, int n)
{
  int i, j, t;

  for (i = n - 1; i > 0; i--)
  {
    j    = rng_range(rng, i + 1);
    t    = a[i];
    a[i] = a[j];
    a[j] = t;
  }
}

/* may digit d go at (r,c) without repeating in its row, column or box? */
static bool sudoku_allowed(unsigned char g[9][9], int r, int c, int d)
{
  int i;

  for (i = 0; i < 9; i++)
    if (g[r][i] == d || g[i][c] == d ||
        g[r / 3 * 3 + i / 3][c / 3 * 3 + i % 3] == d)
      return false;

  return true;
}

/* does the digit at (r,c) repeat in its row, column or box? */
static bool sudoku_clashes(unsigned char g[9][9], int r, int c)
{
  int  d;
  bool clash;

  d       = g[r][c];
  g[r][c] = 0;
  clash   = !sudoku_allowed(g, r, c, d);
  g[r][c] = d;

  return clash;
}

/* count g's solutions by backtracking, stopping once limit are found. With
 * rng, digits are tried in a random order, so a limit of 1 fills an empty
 * grid with a random solved board. g is left holding the last solution
 * found if limit was reached, else as it was given */
static int sudoku_count(unsigned char g[9][9], rng_t *rng, int limit)
{
  int i, r, c, count;
  int order[9];

  for (i = 0; i < 81; i++)
    if (g[i / 9][i % 9] == 0)
      break;
  if (i == 81)
    return 1;

  r = i / 9;
  c = i % 9;

  for (i = 0; i < 9; i++)
    order[i] = i + 1;
  if (rng)
    sudoku_shuffle(rng, order, 9);

  count = 0;
  for (i = 0; i < 9; i++)
  {
    if (!sudoku_allowed(g, r, c, order[i]))
      continue;

    g[r][c] = order[i];
    count  += sudoku_count(g, rng, limit - count);
    if (count >= limit)
      return count;
  }
  g[r][c] = 0;

  return count;
}

/* fill a random solved board, then empty cells in a random order, keeping
 * each one only while the puzzle still has a unique solution */
static void sudoku_new_game(sudoku_task_t *sd)
{
  unsigned char copy[9][9];
  int           order[81];
  int           i, r, c, d, holes;

  memset(sd->solution, 0, sizeof(sd->solution));
  sudoku_count(sd->solution, &sd->rng, 1);
  memcpy(sd->cell, sd->solution, sizeof(sd->cell));

  for (i = 0; i < 81; i++)
    order[i] = i;
  sudoku_shuffle(&sd->rng, order, 81);

  holes = 0;
  for (i = 0; i < 81 && holes < SUDOKU_HOLES; i++)
  {
    r = order[i] / 9;
    c = order[i] % 9;
    d = sd->cell[r][c];

    sd->cell[r][c] = 0;
    memcpy(copy, sd->cell, sizeof(copy));
    if (sudoku_count(copy, NULL, 2) != 1)
      sd->cell[r][c] = d;
    else
      holes++;
  }

  for (i = 0; i < 81; i++)
    sd->given[i / 9][i % 9] = sd->cell[i / 9][i % 9] != 0;

  sd->cur_r  = 4;
  sd->cur_c  = 4;
  sd->solved = false;
}

/* clear every entry, leaving the givens */
static void sudoku_restart(sudoku_task_t *sd)
{
  int i;

  for (i = 0; i < 81; i++)
    if (!sd->given[i / 9][i % 9])
      sd->cell[i / 9][i % 9] = 0;

  sd->solved = false;
}

/* ----------------------------------------------------------------------- */

result_t sudoku_create(wuss_t *wuss, sudoku_task_t **out)
{
  result_t         rc;
  sudoku_task_t   *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->font = wuss_get_font_n(wuss, 1);
  rng_seed(&task->rng, (uint32_t) rand());
  sudoku_new_game(task);

  delegate_desc.handle    = sudoku_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "sudoku";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(SUDOKU_SIZE, SUDOKU_SIZE),
                          "Sudoku",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  /* window up: from here, closing it reaps the task and its wuss_EVENT_QUIT
   * frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SUDOKU_MENU_NEW, "New game",
                          wuss_MENU_ITEM_NONE, "N");
  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SUDOKU_MENU_RESTART, "Restart",
                          wuss_MENU_ITEM_NONE, "R");
  WUSS_MENU_TITLE(task->menu, "Sudoku", task->menu_items,
                  NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

result_t sudoku_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Sudoku",
    "Number placement puzzle",
    (taskapp_create_fn_t) sudoku_create
  };

  return taskapp_install(wuss, &desc);
}

void sudoku_destroy(sudoku_task_t *task)
{
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t sudoku_redraw(const wuss_event_t *event, void *task_data)
{
  sudoku_task_t *sd;
  screen_t      *scr;
  bool           focused;
  int            ox, oy, fh, ascent;
  int            r, c, x, y;
  colour_t       bg, fg;
  char           buf[1];
  bmfont_width_t width;
  point_t        pos;

  sd = task_data;

  scr     = event->data.redraw.scr;
  focused = wuss_get_focus(sd->wuss) == sd->window;
  ox      = event->data.redraw.bounds->x0 - event->data.redraw.scroll.x;
  oy      = event->data.redraw.bounds->y0 - event->data.redraw.scroll.y;
  bmfont_get_info(sd->font, NULL, &fh, &ascent, NULL);

  /* the gaps left between cells form the grid lines */
  screen_fill_rect(scr, ox, oy, SIZE2D(SUDOKU_SIZE, SUDOKU_SIZE),
                   colour_rgb(0x40, 0x40, 0x40));

  for (r = 0; r < 9; r++)
  {
    for (c = 0; c < 9; c++)
    {
      x = ox + SUDOKU_POS(c);
      y = oy + SUDOKU_POS(r);

      if (sd->solved)
        bg = colour_rgb(0xC0, 0xF0, 0xC0);
      else if (focused && r == sd->cur_r && c == sd->cur_c)
        bg = colour_rgb(0xFF, 0xE0, 0x80);
      else
        bg = colour_rgb(0xFF, 0xFF, 0xFF);
      screen_fill_rect(scr, x, y, SIZE2D(SUDOKU_CELL, SUDOKU_CELL), bg);

      if (sd->cell[r][c] == 0)
        continue;

      if (sd->given[r][c])
        fg = colour_rgb(0x00, 0x00, 0x00);
      else if (sudoku_clashes(sd->cell, r, c))
        fg = colour_rgb(0xE0, 0x00, 0x00);
      else
        fg = colour_rgb(0x00, 0x40, 0xC0);

      buf[0] = (char) ('0' + sd->cell[r][c]);
      bmfont_measure(sd->font, buf, 1, NULL, INT_MAX, NULL, &width);
      pos.x = x + (SUDOKU_CELL - width) / 2;
      pos.y = y + (SUDOKU_CELL - fh) / 2 + ascent;
      bmfont_draw(sd->font, scr, buf, 1, fg, colour_rgba(0, 0, 0, 0), NULL,
                  &pos, NULL);
    }
  }

  return result_OK;
}

/* put digit d (0 clears) in the cursor cell, unless it is a given */
static result_t sudoku_set(sudoku_task_t *sd, int d)
{
  if (sd->solved || sd->given[sd->cur_r][sd->cur_c])
    return result_OK;

  sd->cell[sd->cur_r][sd->cur_c] = d;
  sd->solved = memcmp(sd->cell, sd->solution, sizeof(sd->cell)) == 0;
  wuss_window_invalidate_visible(sd->window);

  return result_OK;
}

static void sudoku_move(sudoku_task_t *sd, int r, int c)
{
  sd->cur_r = CLAMP(r, 0, 8);
  sd->cur_c = CLAMP(c, 0, 8);
  wuss_window_invalidate_visible(sd->window);
}

/* which cell does document coordinate v fall in? -1 for a grid line */
static int sudoku_hit(int v)
{
  int i;

  for (i = 0; i < 9; i++)
    if (v >= SUDOKU_POS(i) && v < SUDOKU_POS(i) + SUDOKU_CELL)
      return i;

  return -1;
}

static result_t sudoku_mouse(sudoku_task_t *sd,
                             point_t        point,
                             wuss_button_t  button)
{
  int r, c, step;

  if (button & wuss_BUTTON_MENU)
    return wuss_menu_open_at_pointer(sd->delegate, &sd->menu,
                                     &sd->menu_handle);

  r = sudoku_hit(point.y);
  c = sudoku_hit(point.x);
  if (r < 0 || c < 0)
    return result_OK;

  if (r != sd->cur_r || c != sd->cur_c)
  {
    sudoku_move(sd, r, c);
    return result_OK;
  }

  step = (button & wuss_BUTTON_ADJUST) ? 9 : 1; /* 9 is -1 mod 10 */

  return sudoku_set(sd, (sd->cell[r][c] + step) % 10);
}

/* arrows move the cursor, 1-9 enter a digit and 0, Space, Backspace or
 * Delete clear it; everything else is passed back unclaimed */
static result_t sudoku_key(sudoku_task_t *sd, int code)
{
  switch (code)
  {
  case wuss_KEY_UP:        sudoku_move(sd, sd->cur_r - 1, sd->cur_c); break;
  case wuss_KEY_DOWN:      sudoku_move(sd, sd->cur_r + 1, sd->cur_c); break;
  case wuss_KEY_LEFT:      sudoku_move(sd, sd->cur_r, sd->cur_c - 1); break;
  case wuss_KEY_RIGHT:     sudoku_move(sd, sd->cur_r, sd->cur_c + 1); break;
  case '0':
  case ' ':
  case wuss_KEY_BACKSPACE:
  case wuss_KEY_DELETE:    return sudoku_set(sd, 0);
  default:
    if (code >= '1' && code <= '9')
      return sudoku_set(sd, code - '0');
    return result_WUSS_KEY_UNCLAIMED;
  }

  return result_OK;
}

result_t sudoku_handle(wuss_window_t      *window,
                       const wuss_event_t *event,
                       void               *task_data)
{
  sudoku_task_t *sd;

  sd = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return sudoku_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != sd->window || event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return sudoku_mouse(sd, event->data.mouse.point,
                        event->data.mouse.button);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != sd->window)
      return result_WUSS_KEY_UNCLAIMED;

    if (!task_key_is_plain(sd->delegate, &sd->menu, event, &rc))
      return rc;

    return sudoku_key(sd, event->data.key.code);
  }

  case wuss_EVENT_GAIN_FOCUS:
  case wuss_EVENT_LOSE_FOCUS:
    if (sd->window != NULL)
      wuss_window_invalidate_visible(sd->window); /* show or hide the cursor */
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    if (wuss_menu_select_is(event, &sd->menu, SUDOKU_MENU_NEW))
      sudoku_new_game(sd);
    else if (wuss_menu_select_is(event, &sd->menu, SUDOKU_MENU_RESTART))
      sudoku_restart(sd);
    else
      return result_OK;

    wuss_window_invalidate_visible(sd->window);
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    sd->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == sd->window)
      sd->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    /* the task autocloses once its window goes, so free the block here */
    sudoku_destroy(sd);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
