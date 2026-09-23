/* wuss/test/tasks/minesweeper.c -- minesweeper task */

#ifdef WUSS_APP

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/point.h"
#include "io/path.h"
#include "wuss/menu.h"

#include "minesweeper.h"

#define MS_BORDER MINESWEEPER_CELL /* one grid unit all round */

/* the HUD strip's height and the mines/timer fields' width are sized off the
 * HUD font's own metrics (DPT-Digits-Bold-Lg), not MINESWEEPER_CELL -- that
 * only sizes the board grid. MS_HUD_FIELD_W covers "999" plus a small margin,
 * measured (not guessed) at font-load time: see minesweeper_create. */
#define MS_HUD_V_PAD 4 /* pixels above+below the glyph cell in the strip */
#define MS_HUD_H(ms) ((ms)->hud_font_h)

/* board geometry depends on the current grid size, so these are functions
 * rather than macros; MS_GRID_W/H, MS_WIDTH/HEIGHT below are their obvious
 * shorthand */
#define MS_GRID_W(ms) ((ms)->cols * MINESWEEPER_CELL)
#define MS_GRID_H(ms) ((ms)->rows * MINESWEEPER_CELL)
#define MS_WIDTH(ms)  (MAX(MS_GRID_W(ms) + 2 * MS_BORDER, \
                           2 * MS_BORDER + 2 * (ms)->hud_field_w))
#define MS_HEIGHT(ms) (MS_GRID_H(ms) + 2 * MS_BORDER + MS_HUD_H(ms))

/* the timer's field, window-local content coords: right-aligned in the HUD
 * strip, mirroring minesweeper_draw_hud's placement */
#define MS_TIMER_X0(ms) (MS_WIDTH(ms) - MS_BORDER - (ms)->hud_field_w)
#define MS_TIMER_BOX(ms) \
  ((box_t) { MS_TIMER_X0(ms), 0, MS_WIDTH(ms) - MS_BORDER, MS_HUD_H(ms) })

/* the mines-remaining field, window-local content coords: left-aligned in
 * the HUD strip, mirroring minesweeper_draw_hud's placement */
#define MS_MINES_BOX(ms) \
  ((box_t) { MS_BORDER, 0, MS_BORDER + (ms)->hud_field_w, MS_HUD_H(ms) })

/* window-local pixel box of the cell range [r0,r1) x [c0,c1); mirrors
 * minesweeper_draw_cell's own x,y placement */
#define MS_CELLS_BOX(ms, r0, c0, r1, c1) \
  ((box_t) { MS_BORDER + (c0) * MINESWEEPER_CELL, \
            MS_HUD_H(ms) + MS_BORDER + (r0) * MINESWEEPER_CELL, \
            MS_BORDER + (c1) * MINESWEEPER_CELL, \
            MS_HUD_H(ms) + MS_BORDER + (r1) * MINESWEEPER_CELL })

/* rows, cols, mines offered by the "Grid Size" submenu, indexed by
 * minesweeper_size_t; mine counts follow the classic ~14% density */
static const struct { int rows, cols, mines; }
g_minesweeper_sizes[minesweeper_NSIZES] =
{
  { 24, 24, 99 },
  { 12, 24, 50 },
  { 16, 16, 40 },
  { 12, 16, 30 },
  { 12, 12, 20 }
};

static const char *const g_minesweeper_size_names[minesweeper_NSIZES] =
{
  "24x24", "24x12", "16x16", "16x12", "12x12"
};

/* MENU click over the board pops this menu; the item tables and wuss_menu_t
 * values live per-instance in minesweeper_task_t, not as file-scope
 * statics, so that each window's Info row points at its own .window pointer
 * (retargeted at the shared proginfo singleton just before wuss_menu_open,
 * in minesweeper_mouse) rather than every instance sharing (and overwriting)
 * one global .window pointer -- and so two instances don't fight over one
 * shared tick mark on the Grid Size submenu */
enum { MINESWEEPER_MENU_INFO, MINESWEEPER_MENU_NEW_GAME, MINESWEEPER_MENU_SIZE };

/* neighbour-count colour, classic minesweeper palette; index 0 is never
 * drawn (an empty cell shows no digit) */
static colour_t minesweeper_number_colour(int n)
{
  switch (n)
  {
  case 1:  return colour_rgb(0x00, 0x00, 0xFF);
  case 2:  return colour_rgb(0x00, 0x80, 0x00);
  case 3:  return colour_rgb(0xFF, 0x00, 0x00);
  case 4:  return colour_rgb(0x00, 0x00, 0x80);
  case 5:  return colour_rgb(0x80, 0x00, 0x00);
  case 6:  return colour_rgb(0x00, 0x80, 0x80);
  case 7:  return colour_rgb(0x00, 0x00, 0x00);
  default: return colour_rgb(0x80, 0x80, 0x80); /* 8 */
  }
}

/* ----------------------------------------------------------------------- */

static bool minesweeper_in_bounds(minesweeper_task_t *ms, int r, int c)
{
  return r >= 0 && r < ms->rows && c >= 0 && c < ms->cols;
}

static int minesweeper_count_neighbours(minesweeper_task_t *ms, int r, int c)
{
  int dr, dc, n;

  n = 0;
  for (dr = -1; dr <= 1; dr++)
    for (dc = -1; dc <= 1; dc++)
      if ((dr || dc) &&
          minesweeper_in_bounds(ms, r + dr, c + dc) &&
          ms->mine[r + dr][c + dc])
        n++;

  return n;
}

/* mines are placed on the first reveal, excluding (safe_r,safe_c) and its
 * neighbours, so the opening click always lands on a clear patch */
static void minesweeper_place_mines(minesweeper_task_t *ms,
                                    int                 safe_r,
                                    int                 safe_c)
{
  int placed, r, c;

  placed = 0;
  while (placed < ms->mines)
  {
    r = rand() % ms->rows;
    c = rand() % ms->cols;
    if (ms->mine[r][c] ||
        (abs(r - safe_r) <= 1 && abs(c - safe_c) <= 1))
      continue;
    ms->mine[r][c] = true;
    placed++;
  }

  ms->placed = true;
}

/* inclusive-exclusive board-cell range [r0,r1) x [c0,c1) touched by a flood
 * reveal, accumulated by minesweeper_reveal so the caller can invalidate
 * just that area instead of the whole board */
typedef struct { int r0, c0, r1, c1; } minesweeper_cellbox_t;

/* reveals (r,c) and, if it has no adjacent mines, floods outward to its
 * neighbours; recursion depth is bounded by the (small, fixed) board size */
static void minesweeper_reveal(minesweeper_task_t    *ms,
                               int                    r,
                               int                    c,
                               minesweeper_cellbox_t *touched)
{
  int dr, dc;

  if (r < touched->r0) touched->r0 = r;
  if (c < touched->c0) touched->c0 = c;
  if (r + 1 > touched->r1) touched->r1 = r + 1;
  if (c + 1 > touched->c1) touched->c1 = c + 1;

  if (!minesweeper_in_bounds(ms, r, c) || ms->state[r][c] != minesweeper_HIDDEN)
    return;

  ms->state[r][c] = minesweeper_REVEALED;

  if (ms->mine[r][c])
  {
    ms->dead = true;
    return;
  }

  if (minesweeper_count_neighbours(ms, r, c) == 0)
    for (dr = -1; dr <= 1; dr++)
      for (dc = -1; dc <= 1; dc++)
        if (dr || dc)
          minesweeper_reveal(ms, r + dr, c + dc, touched);
}

/* on death, reveal every mine so the player sees where they all were */
static void minesweeper_reveal_all_mines(minesweeper_task_t *ms)
{
  int r, c;

  for (r = 0; r < ms->rows; r++)
    for (c = 0; c < ms->cols; c++)
      if (ms->mine[r][c])
        ms->state[r][c] = minesweeper_REVEALED;
}

static bool minesweeper_check_won(minesweeper_task_t *ms)
{
  int r, c;

  for (r = 0; r < ms->rows; r++)
    for (c = 0; c < ms->cols; c++)
      if (!ms->mine[r][c] && ms->state[r][c] != minesweeper_REVEALED)
        return false;

  return true;
}

static void minesweeper_reset(minesweeper_task_t *ms)
{
  ms->placed  = false;
  ms->dead    = false;
  ms->won     = false;
  ms->flags   = 0;
  ms->elapsed = 0;
  memset(ms->mine,  0, sizeof(ms->mine));
  memset(ms->state, 0, sizeof(ms->state)); /* minesweeper_HIDDEN == 0 */
}

/* applies a grid size (rows/cols/mines) and clears the board; does not touch
 * the window, so it is also used at creation before task->window exists --
 * the wuss_EVENT_MENU_SELECT handler resizes the window itself afterwards */
static void minesweeper_set_size(minesweeper_task_t *ms,
                                 minesweeper_size_t  size)
{
  ms->size  = size;
  ms->rows  = g_minesweeper_sizes[size].rows;
  ms->cols  = g_minesweeper_sizes[size].cols;
  ms->mines = g_minesweeper_sizes[size].mines;
  minesweeper_reset(ms);
}

/* seconds since the first reveal, frozen once the game has ended */
static void minesweeper_tick_clock(minesweeper_task_t *ms)
{
  if (!ms->placed || ms->dead || ms->won)
    return;
  ms->elapsed = (int) difftime(time(NULL), ms->start_time);
  if (ms->elapsed > 999)
    ms->elapsed = 999; /* keep the HUD's 3-digit field from overflowing */
}

/* ----------------------------------------------------------------------- */

result_t minesweeper_create(wuss_t *wuss, minesweeper_task_t **out)
{
  result_t            rc;
  minesweeper_task_t *task;
  wuss_task_t        *delegate;
  wuss_task_desc_t    delegate_desc;
  const char         *resources, *filename;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->font = wuss_get_font_n(wuss, 1);
  minesweeper_set_size(task, minesweeper_SIZE_12X12);

  resources = wuss_get_resources(wuss);
  filename  = pathf("%s/resources/bmfonts/DPT-Digits-Bold-Lg.png", resources);
  rc = bmfont_create(filename, &task->hud_font);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }

  {
    bmfont_width_t field_width;

    bmfont_get_info(task->hud_font, NULL, &task->hud_font_h, NULL, NULL);
    bmfont_measure(task->hud_font, "999", 3, NULL, INT_MAX, NULL,
                   &field_width);
    task->hud_field_w = (int) field_width + MS_HUD_V_PAD;
  }

  filename = pathf("%s/resources/wuss/icons/mine.png", resources);
  rc = bitmap_load_png(&task->mine_bm, filename);
  if (rc != result_OK)
  {
    bmfont_destroy(task->hud_font);
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }

  filename = pathf("%s/resources/wuss/icons/flag.png", resources);
  rc = bitmap_load_png(&task->flag_bm, filename);
  if (rc != result_OK)
  {
    free(task->mine_bm.base);
    bmfont_destroy(task->hud_font);
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }

  delegate_desc.handle    = minesweeper_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "minesweeper";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task->flag_bm.base);
    free(task->mine_bm.base);
    bmfont_destroy(task->hud_font);
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  task->task = delegate;
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(MS_WIDTH(task), MS_HEIGHT(task)),
                                 "Minesweeper",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(MS_WIDTH(task), MS_HEIGHT(task)),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  {
    int i;

    for (i = 0; i < minesweeper_NSIZES; i++)
    {
      WUSS_MENU_ITEM(task->size_items, i, g_minesweeper_size_names[i],
                    wuss_MENU_ITEM_NONE);
    }
    WUSS_MENU_TITLE(task->size_menu, "Grid Size", task->size_items,
                   NELEMS(task->size_items));
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, MINESWEEPER_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * minesweeper_mouse */

  WUSS_MENU_ITEM(task->menu_items, MINESWEEPER_MENU_NEW_GAME,
                "New Game", wuss_MENU_ITEM_NONE);

  WUSS_MENU_ITEM_MENU(task->menu_items, MINESWEEPER_MENU_SIZE, "Grid Size",
                      wuss_MENU_ITEM_NONE, &task->size_menu);

  WUSS_MENU_TITLE(task->menu, "Minesweeper", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void minesweeper_destroy(minesweeper_task_t *task)
{
  if (task->menu_handle != NULL)
    wuss_menu_close(task->menu_handle);
  free(task->flag_bm.base);
  free(task->mine_bm.base);
  bmfont_destroy(task->hud_font);
  free(task);
}

static void minesweeper_draw_cell(minesweeper_task_t *ms,
                                  screen_t           *scr,
                                  int                 r,
                                  int                 c,
                                  int                 ox,
                                  int                 oy)
{
  minesweeper_cell_state_t state;
  int                      x, y, n;

  state = ms->state[r][c];
  x     = ox + c * MINESWEEPER_CELL;
  y     = oy + r * MINESWEEPER_CELL;

  screen_fill_rect(scr, x, y,
                   SIZE2D(MINESWEEPER_CELL, MINESWEEPER_CELL),
                   state == minesweeper_REVEALED ?
                     colour_rgb(0xE0, 0xE0, 0xE0) : colour_rgb(0xC0, 0xC0, 0xC0));
  screen_draw_rect(scr, x, y,
                   SIZE2D(MINESWEEPER_CELL, MINESWEEPER_CELL),
                   colour_rgb(0x80, 0x80, 0x80));

  if (state == minesweeper_FLAGGED)
  {
    screen_copy_bitmap(scr, x, y, &ms->flag_bm);
    return;
  }

  if (state != minesweeper_REVEALED)
    return;

  if (ms->mine[r][c])
  {
    screen_copy_bitmap(scr, x, y, &ms->mine_bm);
    return;
  }

  n = minesweeper_count_neighbours(ms, r, c);
  if (n > 0)
  {
    char           buf[2];
    point_t        pos;
    bmfont_width_t width;
    int            fh, ascent;

    buf[0] = (char) ('0' + n);
    buf[1] = '\0';
    bmfont_measure(ms->font, buf, 1, NULL, INT_MAX, NULL, &width);
    bmfont_get_info(ms->font, NULL, &fh, &ascent, NULL);
    pos.x = x + (MINESWEEPER_CELL - width) / 2;
    pos.y = y + (MINESWEEPER_CELL - fh) / 2 + ascent;
    bmfont_draw(ms->font, scr, buf, 1, minesweeper_number_colour(n),
               colour_rgba(0, 0, 0, 0), NULL, &pos, NULL);
  }
}

/* mines-remaining counter (left) and elapsed-seconds timer (right), drawn in
 * the HUD strip above the border */
static void minesweeper_draw_hud(minesweeper_task_t *ms,
                                 screen_t           *scr,
                                 const box_t        *bounds)
{
  char     buf[8];
  point_t  pos;
  colour_t fg, bg;
  int      fh, ascent;

  fg = colour_rgb(0xFF, 0x00, 0x00);
  bg = colour_rgb(0x00, 0x00, 0x00);
  bmfont_get_info(ms->hud_font, NULL, &fh, &ascent, NULL);

  snprintf(buf, sizeof(buf), "%03d", ms->mines - ms->flags);
  pos.x = bounds->x0 + MS_BORDER;
  pos.y = bounds->y0 + (MS_HUD_H(ms) - fh) / 2 + ascent;
  bmfont_draw(ms->hud_font, scr, buf, 3, fg, bg, NULL, &pos, NULL);

  snprintf(buf, sizeof(buf), "%03d", ms->elapsed);
  pos.x = bounds->x0 + MS_TIMER_X0(ms);
  bmfont_draw(ms->hud_font, scr, buf, 3, fg, bg, NULL, &pos, NULL);
}

/* draws a translucent-looking banner strip across the middle of the board;
 * bmfont has no alpha blend here so the strip is drawn solid first */
static void minesweeper_draw_banner(minesweeper_task_t *ms,
                                    screen_t           *scr,
                                    const box_t        *bounds,
                                    const char         *text,
                                    colour_t            bg,
                                    colour_t            fg)
{
  bmfont_width_t width;
  point_t        pos;
  int            len, fh, ascent, strip_y;

  len = (int) strlen(text);
  bmfont_measure(ms->font, text, len, NULL, INT_MAX, NULL, &width);
  bmfont_get_info(ms->font, NULL, &fh, &ascent, NULL);

  strip_y = bounds->y0 + MS_HUD_H(ms) +
           (MS_GRID_H(ms) + 2 * MS_BORDER - fh) / 2 - 2;
  screen_fill_rect(scr, bounds->x0, strip_y, SIZE2D(MS_WIDTH(ms), fh + 4), bg);

  pos.x = bounds->x0 + (MS_WIDTH(ms) - width) / 2;
  pos.y = strip_y + 2 + ascent;
  bmfont_draw(ms->font, scr, text, len, fg, colour_rgba(0, 0, 0, 0), NULL,
             &pos, NULL);
}

static result_t minesweeper_redraw(const wuss_event_t *event,
                                   void               *task_data)
{
  minesweeper_task_t *ms;
  screen_t           *scr;
  const box_t        *bounds;
  const box_t        *clip;
  int                 board_y0;
  int                 r0, c0, r1, c1;
  int                 r, c;

  ms     = task_data;
  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  clip   = event->data.redraw.content;

  /* the HUD strip and the board below are repainted independently, each only
   * when the dirty region actually reaches it -- a flag toggle invalidates
   * one cell plus the mine counter, a timer tick just the timer field, so
   * either half alone is the common case and this keeps both cheap */
  board_y0 = bounds->y0 + MS_HUD_H(ms);

  minesweeper_tick_clock(ms);

  if (clip->y0 < board_y0)
  {
    /* the window owns every pixel (wuss_NO_BACKGROUND), so the HUD strip's
     * own backdrop is painted here before the counters go on top */
    screen_fill_rect(scr, bounds->x0, bounds->y0,
                     SIZE2D(MS_WIDTH(ms), MS_HUD_H(ms)),
                     colour_rgb(0x80, 0x80, 0x80));
    minesweeper_draw_hud(ms, scr, bounds);
  }

  if (clip->y1 <= board_y0)
    return result_OK;

  /* one-cell frame right round the grid; drawn under the cells, so only the
   * margin strip actually needs filling -- but that strip is thin and
   * irregular to clip precisely, and it changes only on a resize/new game,
   * which already invalidates the whole window, so no clipping is worth
   * doing here */
  screen_fill_rect(scr, bounds->x0, board_y0,
                   SIZE2D(MS_WIDTH(ms), MS_HEIGHT(ms) - MS_HUD_H(ms)),
                   colour_rgb(0x80, 0x80, 0x80));

  /* only the cells overlapping the dirty rect need redrawing */
  r0 = MAX(0, (clip->y0 - board_y0 - MS_BORDER) / MINESWEEPER_CELL);
  c0 = MAX(0, (clip->x0 - bounds->x0 - MS_BORDER) / MINESWEEPER_CELL);
  r1 = MIN(ms->rows, (clip->y1 - board_y0 - MS_BORDER + MINESWEEPER_CELL - 1) /
                     MINESWEEPER_CELL);
  c1 = MIN(ms->cols, (clip->x1 - bounds->x0 - MS_BORDER + MINESWEEPER_CELL - 1) /
                     MINESWEEPER_CELL);

  for (r = r0; r < r1; r++)
    for (c = c0; c < c1; c++)
      minesweeper_draw_cell(ms, scr, r, c, bounds->x0 + MS_BORDER,
                            bounds->y0 + MS_HUD_H(ms) + MS_BORDER);

  if (ms->dead)
    minesweeper_draw_banner(ms, scr, bounds, "BOOM! Click to retry",
                            colour_rgb(0x80, 0x00, 0x00),
                            colour_rgb(0xFF, 0xFF, 0xFF));
  else if (ms->won)
    minesweeper_draw_banner(ms, scr, bounds, "You win! Click to retry",
                            colour_rgb(0x00, 0x60, 0x00),
                            colour_rgb(0xFF, 0xFF, 0xFF));

  return result_OK;
}

static result_t minesweeper_mouse(minesweeper_task_t *ms,
                                  point_t             point,
                                  wuss_button_t       button)
{
  int r, c;

  if (button & wuss_BUTTON_MENU)
  {
    static const wuss_proginfo_desc_t desc =
    {
      "Minesweeper",
      "Classic minesweeper",
      "(c) DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };

    wuss_proginfo_set_desc(&desc);
    ms->menu_items[MINESWEEPER_MENU_INFO].window = wuss_proginfo_window(ms->task);

    wuss_menu_tick_exclusive(&ms->size_menu, ms->size);
    return wuss_menu_open(ms->task, &ms->menu,
                          wuss_get_pointer(ms->wuss), &ms->menu_handle);
  }

  if (ms->dead || ms->won)
  {
    /* any click on a finished board starts a fresh game */
    if (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST))
    {
      minesweeper_reset(ms);
      wuss_window_invalidate_visible(ms->window);
    }
    return result_OK;
  }

  c = (point.x - MS_BORDER) / MINESWEEPER_CELL;
  r = (point.y - MS_HUD_H(ms) - MS_BORDER) / MINESWEEPER_CELL;
  if (point.x < MS_BORDER || point.y < MS_HUD_H(ms) + MS_BORDER ||
      !minesweeper_in_bounds(ms, r, c))
    return result_OK;

  if (button & wuss_BUTTON_SELECT)
  {
    minesweeper_cellbox_t touched;
    box_t                 local;

    if (ms->state[r][c] == minesweeper_FLAGGED)
      return result_OK;
    if (!ms->placed)
    {
      minesweeper_place_mines(ms, r, c);
      ms->start_time = time(NULL);
    }

    touched.r0 = touched.r1 = r;
    touched.c0 = touched.c1 = c;
    minesweeper_reveal(ms, r, c, &touched);

    if (ms->dead)
    {
      minesweeper_reveal_all_mines(ms);
      wuss_window_invalidate_visible(ms->window); /* every mine, plus banner */
    }
    else
    {
      ms->won = minesweeper_check_won(ms);
      if (ms->won)
        wuss_window_invalidate_visible(ms->window); /* banner covers the lot */
      else
      {
        local = MS_CELLS_BOX(ms, touched.r0, touched.c0, touched.r1,
                             touched.c1);
        wuss_window_invalidate(ms->window, &local);
      }
    }
    minesweeper_tick_clock(ms);
  }
  else if (button & wuss_BUTTON_ADJUST)
  {
    box_t local = MS_CELLS_BOX(ms, r, c, r + 1, c + 1);

    if (ms->state[r][c] == minesweeper_HIDDEN)
    {
      ms->state[r][c] = minesweeper_FLAGGED;
      ms->flags++;
    }
    else if (ms->state[r][c] == minesweeper_FLAGGED)
    {
      ms->state[r][c] = minesweeper_HIDDEN;
      ms->flags--;
    }
    else
    {
      return result_OK;
    }

    /* the flag count in the HUD changed too */
    {
      box_t mines_box = MS_MINES_BOX(ms);

      wuss_window_invalidate(ms->window, &mines_box);
    }
    wuss_window_invalidate(ms->window, &local);
  }
  else
  {
    return result_OK;
  }

  return result_OK;
}

result_t minesweeper_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  minesweeper_task_t *ms;

  ms = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return minesweeper_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != ms->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return minesweeper_mouse(ms, event->data.mouse.point,
                             event->data.mouse.button);

  case wuss_EVENT_IDLE:
    /* the shared proginfo singleton is a second window on this same
     * (autoclose) delegate while its dialogue is open, so closing the board
     * window alone doesn't necessarily empty task->windows immediately --
     * guard against the dangling window in the meantime */
    if (ms->window == NULL)
      return result_OK;
    if (ms->placed && !ms->dead && !ms->won)
    {
      int was;

      was = ms->elapsed;
      minesweeper_tick_clock(ms);
      if (ms->elapsed != was)
      {
        box_t timer = MS_TIMER_BOX(ms);

        wuss_window_invalidate(ms->window, &timer);
      }
    }
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &ms->size_menu)
      minesweeper_set_size(ms, (minesweeper_size_t)
                           event->data.menu_select.index);
    else if (event->data.menu_select.index == MINESWEEPER_MENU_NEW_GAME)
      minesweeper_reset(ms);
    else
      return result_OK; /* Info row: nothing to do here */

    {
      size2d_t sz = SIZE2D(MS_WIDTH(ms), MS_HEIGHT(ms));

      wuss_window_resize(ms->window, sz);
      wuss_window_set_doc(ms->window, sz);
    }
    wuss_window_invalidate_visible(ms->window);
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    ms->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == ms->window)
      ms->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == ms->menu_items[MINESWEEPER_MENU_INFO].window)
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
    minesweeper_destroy(ms);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
