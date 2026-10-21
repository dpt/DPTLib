/* wuss/tasks/minesweeping/minesweeping.c -- minesweeper task */

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

#include "minesweeping.h"
#include "../taskapp.h"
#include "../common.h"

#define MS_BORDER MINESWEEPING_CELL /* one grid unit all round */

#define MS_BOOM_PARTICLES (MAX_PARTICLES / 2) /* particles per detonation */

/* the HUD strip's height and the mines/timer fields' width are sized off the
 * HUD font's own metrics (DPT-Digits-Bold-Lg), not MINESWEEPING_CELL -- that
 * only sizes the board grid. MS_HUD_FIELD_W covers "999" plus a small margin,
 * measured (not guessed) at font-load time: see minesweeping_create. */
#define MS_HUD_V_PAD 4 /* pixels above+below the glyph cell in the strip */
#define MS_HUD_H(ms) ((ms)->hud_font_h)

/* board geometry depends on the current grid size, so these are functions
 * rather than macros; MS_GRID_W/H, MS_WIDTH/HEIGHT below are their obvious
 * shorthand */
#define MS_GRID_W(ms) ((ms)->cols * MINESWEEPING_CELL)
#define MS_GRID_H(ms) ((ms)->rows * MINESWEEPING_CELL)
#define MS_WIDTH(ms)  (MAX(MS_GRID_W(ms) + 2 * MS_BORDER, \
                           2 * MS_BORDER + 2 * (ms)->hud_field_w))
#define MS_HEIGHT(ms) (MS_GRID_H(ms) + 2 * MS_BORDER + MS_HUD_H(ms))

/* the timer's field, window-local content coords: right-aligned in the HUD
 * strip, mirroring minesweeping_draw_hud's placement */
#define MS_TIMER_X0(ms) (MS_WIDTH(ms) - MS_BORDER - (ms)->hud_field_w)
#define MS_TIMER_BOX(ms) \
  ((box_t) { MS_TIMER_X0(ms), 0, MS_WIDTH(ms) - MS_BORDER, MS_HUD_H(ms) })

/* the mines-remaining field, window-local content coords: left-aligned in
 * the HUD strip, mirroring minesweeping_draw_hud's placement */
#define MS_MINES_BOX(ms) \
  ((box_t) { MS_BORDER, 0, MS_BORDER + (ms)->hud_field_w, MS_HUD_H(ms) })

/* window-local pixel box of the cell range [r0,r1) x [c0,c1); mirrors
 * minesweeping_draw_cell's own x,y placement */
#define MS_CELLS_BOX(ms, r0, c0, r1, c1) \
  ((box_t) { MS_BORDER + (c0) * MINESWEEPING_CELL, \
            MS_HUD_H(ms) + MS_BORDER + (r0) * MINESWEEPING_CELL, \
            MS_BORDER + (c1) * MINESWEEPING_CELL, \
            MS_HUD_H(ms) + MS_BORDER + (r1) * MINESWEEPING_CELL })

/* rows, cols, mines offered by the "Grid Size" submenu, indexed by
 * minesweeping_size_t; mine counts follow the classic ~14% density */
static const struct { int rows, cols, mines; }
g_minesweeping_sizes[minesweeping_NSIZES] =
{
  { 24, 24, 99 },
  { 12, 24, 50 },
  { 16, 16, 40 },
  { 12, 16, 30 },
  { 12, 12, 20 }
};

static const char *const g_minesweeping_size_names[minesweeping_NSIZES] =
{
  "24x24", "24x12", "16x16", "16x12", "12x12"
};

/* "Difficulty" submenu rows, indexed by minesweeping_difficulty_t: each
 * scales g_minesweeping_sizes' mine count by percent. Normal is the classic
 * count; Hard tops out near 22% density, well short of the board filling up
 * around the safe 3x3 opening. */
static const struct { const char *name; int percent; }
g_minesweeping_difficulties[minesweeping_NDIFFICULTIES] =
{
  { "Easy",    70 },
  { "Normal", 100 },
  { "Hard",   130 }
};

/* MENU click over the board pops this menu; the item tables and wuss_menu_t
 * values live per-instance in minesweeping_task_t, not as file-scope
 * statics, so that two instances don't fight over one shared tick mark on the
 * Grid Size or Difficulty submenus */
enum
{
  MINESWEEPING_MENU_NEW_GAME,
  MINESWEEPING_MENU_SIZE,
  MINESWEEPING_MENU_DIFFICULTY,
  MINESWEEPING_MENU_GIVE_UP
};

/* cache all constant palette entries; call once at create and again whenever
 * wuss_EVENT_PALETTE fires, in case colour_to_pixel results have changed */
static void minesweeping_prepare_colours(minesweeping_task_t *ms)
{
  ms->colours.number[1]     = colour_rgb(0x00, 0x00, 0xFF);
  ms->colours.number[2]     = colour_rgb(0x00, 0x80, 0x00);
  ms->colours.number[3]     = colour_rgb(0xFF, 0x00, 0x00);
  ms->colours.number[4]     = colour_rgb(0x00, 0x00, 0x80);
  ms->colours.number[5]     = colour_rgb(0x80, 0x00, 0x00);
  ms->colours.number[6]     = colour_rgb(0x00, 0x80, 0x80);
  ms->colours.number[7]     = colour_rgb(0x00, 0x00, 0x00);
  ms->colours.number[8]     = colour_rgb(0x80, 0x80, 0x80);
  ms->colours.cell_revealed = colour_rgb(0xE0, 0xE0, 0xE0);
  ms->colours.cell_hidden   = colour_rgb(0xC0, 0xC0, 0xC0);
  ms->colours.cell_border   = colour_rgb(0x80, 0x80, 0x80);
  ms->colours.hud_fg        = colour_rgb(0xFF, 0x00, 0x00);
  ms->colours.hud_bg        = colour_rgba(0x00, 0x00, 0x00, 0x00);
  ms->colours.fill          = colour_rgb(0x80, 0x80, 0x80);
  ms->colours.dead_bg       = colour_rgb(0x80, 0x00, 0x00);
  ms->colours.won_bg        = colour_rgb(0x00, 0x60, 0x00);
  ms->colours.banner_fg     = colour_rgb(0xFF, 0xFF, 0xFF);
  ms->colours.cursor        = colour_rgb(0xFF, 0x80, 0x00);
}

/* ----------------------------------------------------------------------- */

/* ----------------------------------------------------------------------- */

static bool minesweeping_in_bounds(minesweeping_task_t *ms, int r, int c)
{
  return r >= 0 && r < ms->rows && c >= 0 && c < ms->cols;
}

static int minesweeping_count_neighbours(minesweeping_task_t *ms,
                                         int                  r,
                                         int                  c)
{
  int dr, dc, n;

  n = 0;
  for (dr = -1; dr <= 1; dr++)
    for (dc = -1; dc <= 1; dc++)
      if ((dr || dc) &&
          minesweeping_in_bounds(ms, r + dr, c + dc) &&
          ms->mine[r + dr][c + dc])
        n++;

  return n;
}

/* mines are placed on the first reveal, excluding (safe_r,safe_c) and its
 * neighbours, so the opening click always lands on a clear patch; the four
 * board corners never hold a mine */
static void minesweeping_place_mines(minesweeping_task_t *ms,
                                     int                  safe_r,
                                     int                  safe_c)
{
  int placed, r, c;

  placed = 0;
  while (placed < ms->mines)
  {
    r = rand() % ms->rows;
    c = rand() % ms->cols;
    if (ms->mine[r][c] ||
        ((r == 0 || r == ms->rows - 1) && (c == 0 || c == ms->cols - 1)) ||
        (abs(r - safe_r) <= 1 && abs(c - safe_c) <= 1))
      continue;
    ms->mine[r][c] = true;
    placed++;
  }

  ms->placed = true;
}

/* inclusive-exclusive board-cell range [r0,r1) x [c0,c1) touched by a flood
 * reveal, accumulated by minesweeping_reveal so the caller can invalidate
 * just that area instead of the whole board */
typedef struct { int r0, c0, r1, c1; } minesweeping_cellbox_t;

/* reveals (r,c) and, if it has no adjacent mines, floods outward to its
 * neighbours; recursion depth is bounded by the (small, fixed) board size */
static void minesweeping_reveal(minesweeping_task_t    *ms,
                                int                     r,
                                int                     c,
                                minesweeping_cellbox_t *touched)
{
  int dr, dc;

  if (r < touched->r0) touched->r0 = r;
  if (c < touched->c0) touched->c0 = c;
  if (r + 1 > touched->r1) touched->r1 = r + 1;
  if (c + 1 > touched->c1) touched->c1 = c + 1;

  if (!minesweeping_in_bounds(ms, r, c) || ms->state[r][c] != minesweeping_HIDDEN)
    return;

  ms->state[r][c] = minesweeping_REVEALED;

  if (ms->mine[r][c])
  {
    ms->dead = true;
    return;
  }

  if (minesweeping_count_neighbours(ms, r, c) == 0)
    for (dr = -1; dr <= 1; dr++)
      for (dc = -1; dc <= 1; dc++)
        if (dr || dc)
          minesweeping_reveal(ms, r + dr, c + dc, touched);
}

/* "chording": on a revealed number whose flagged neighbours match it,
 * reveals every other hidden neighbour; a misplaced flag means a mine goes
 * off, as in the classic game */
static void minesweeping_chord(minesweeping_task_t    *ms,
                               int                     r,
                               int                     c,
                               minesweeping_cellbox_t *touched)
{
  int flags;
  int dr, dc;

  flags = 0;
  for (dr = -1; dr <= 1; dr++)
    for (dc = -1; dc <= 1; dc++)
      if (minesweeping_in_bounds(ms, r + dr, c + dc) &&
          ms->state[r + dr][c + dc] == minesweeping_FLAGGED)
        flags++;

  if (flags == 0 || flags != minesweeping_count_neighbours(ms, r, c))
    return;

  for (dr = -1; dr <= 1; dr++)
    for (dc = -1; dc <= 1; dc++)
      if (dr || dc)
        minesweeping_reveal(ms, r + dr, c + dc, touched);
}

/* on a win, keeps the fastest time for the current size and difficulty;
 * new_best says whether this game set it. Session only: not saved. */
static void minesweeping_record_time(minesweeping_task_t *ms)
{
  int *best;

  best         = &ms->best[ms->size][ms->difficulty];
  ms->new_best = (*best == 0 || ms->elapsed + 1 < *best);
  if (ms->new_best)
    *best = ms->elapsed + 1;
}

/* on death, reveal every mine so the player sees where they all were */
static void minesweeping_reveal_all_mines(minesweeping_task_t *ms)
{
  int r, c;

  for (r = 0; r < ms->rows; r++)
    for (c = 0; c < ms->cols; c++)
      if (ms->mine[r][c])
        ms->state[r][c] = minesweeping_REVEALED;
}

static bool minesweeping_check_won(minesweeping_task_t *ms)
{
  int r, c;

  for (r = 0; r < ms->rows; r++)
    for (c = 0; c < ms->cols; c++)
      if (!ms->mine[r][c] && ms->state[r][c] != minesweeping_REVEALED)
        return false;

  return true;
}

static void minesweeping_reset(minesweeping_task_t *ms)
{
  ms->placed   = false;
  ms->dead     = false;
  ms->won      = false;
  ms->new_best = false;
  ms->flags    = 0;
  ms->elapsed  = 0;
  memset(ms->mine,  0, sizeof(ms->mine));
  memset(ms->state, 0, sizeof(ms->state)); /* minesweeping_HIDDEN == 0 */
  fireball_reset(&ms->boom);
}

/* applies a grid size (rows/cols) and its mine count, scaled by the current
 * difficulty, and clears the board; does not touch the window, so it is also
 * used at creation before task->window exists -- the wuss_EVENT_MENU_SELECT
 * handler resizes the window itself afterwards */
static void minesweeping_set_size(minesweeping_task_t *ms,
                                  minesweeping_size_t  size)
{
  ms->size  = size;
  ms->rows  = g_minesweeping_sizes[size].rows;
  ms->cols  = g_minesweeping_sizes[size].cols;
  ms->mines = g_minesweeping_sizes[size].mines *
              g_minesweeping_difficulties[ms->difficulty].percent / 100;
  ms->cur_r = MIN(ms->cur_r, ms->rows - 1);
  ms->cur_c = MIN(ms->cur_c, ms->cols - 1);
  minesweeping_reset(ms);
}

/* seconds since the first reveal, frozen once the game has ended */
static void minesweeping_tick_clock(minesweeping_task_t *ms)
{
  if (!ms->placed || ms->dead || ms->won)
    return;
  ms->elapsed = (int) difftime(time(NULL), ms->start_time);
  ms->elapsed = MIN(ms->elapsed, 999); /* keep the HUD's 3 digits in range */
}

/* ----------------------------------------------------------------------- */

result_t minesweeping_create(wuss_t *wuss, minesweeping_task_t **out)
{
  result_t             rc;
  minesweeping_task_t *task;
  wuss_task_t         *delegate;
  wuss_task_desc_t     delegate_desc;
  const char          *resources, *filename;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->font = wuss_get_font_n(wuss, 1);
  task->difficulty = minesweeping_DIFFICULTY_NORMAL;
  fireball_init(&task->boom);
  minesweeping_set_size(task, minesweeping_SIZE_12X12);
  minesweeping_prepare_colours(task);

  resources = wuss_get_resources(wuss);
  filename  = pathf("%s/resources/bmfonts/DPT-DigitsLg/Bold.png", resources);
  rc = bmfontcache_acquire(wuss_get_font_cache(wuss), filename,
                           &task->hud_font);
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

  delegate_desc.handle    = minesweeping_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "minesweeping";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    bmfontcache_release(wuss_get_font_cache(wuss), task->hud_font);
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  task->task = delegate;
  wuss_task_set_autoclose(delegate, 1);

  /* mine and flag live in the task's private pool, not the shared one */
  rc = wuss_task_iconpool_load_resource(delegate, resources);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  task->mine_bm = wuss_task_iconpool_find(delegate, "mine");
  task->flag_bm = wuss_task_iconpool_find(delegate, "flag");
  if (task->mine_bm == NULL || task->flag_bm == NULL)
  {
    wuss_task_destroy(delegate);
    return result_NOT_FOUND;
  }

  rc = task_window_create(delegate,
                          SIZE2D(MS_WIDTH(task),
                          MS_HEIGHT(task)),
                          "Minesweeping",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  {
    int i;

    for (i = 0; i < minesweeping_NSIZES; i++)
    {
      WUSS_MENU_ITEM(task->size_items, i, g_minesweeping_size_names[i],
                    wuss_MENU_ITEM_NONE);
    }
    WUSS_MENU_TITLE(task->size_menu, "Grid Size", task->size_items,
                   NELEMS(task->size_items));

    for (i = 0; i < minesweeping_NDIFFICULTIES; i++)
    {
      WUSS_MENU_ITEM(task->difficulty_items, i,
                     g_minesweeping_difficulties[i].name, wuss_MENU_ITEM_NONE);
    }
    WUSS_MENU_TITLE(task->difficulty_menu, "Difficulty",
                   task->difficulty_items, NELEMS(task->difficulty_items));
  }

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, MINESWEEPING_MENU_NEW_GAME,
                          "New Game", wuss_MENU_ITEM_NONE, "N");

  WUSS_MENU_ITEM_MENU(task->menu_items, MINESWEEPING_MENU_SIZE, "Grid Size",
                      wuss_MENU_ITEM_NONE, &task->size_menu);

  WUSS_MENU_ITEM_MENU(task->menu_items, MINESWEEPING_MENU_DIFFICULTY,
                      "Difficulty", wuss_MENU_ITEM_NONE,
                      &task->difficulty_menu);

  WUSS_MENU_ITEM(task->menu_items, MINESWEEPING_MENU_GIVE_UP,
                "Give Up", wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->menu, "Minesweeping", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

result_t minesweeping_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Minesweeping",
    "Classic minesweeper",
    (taskapp_create_fn_t) minesweeping_create
  };

  return taskapp_install(wuss, &desc);
}

void minesweeping_destroy(minesweeping_task_t *task)
{
  bmfontcache_release(wuss_get_font_cache(task->wuss), task->hud_font);
  free(task);
}

static void minesweeping_draw_cell(minesweeping_task_t *ms,
                                   screen_t            *scr,
                                   int                  r,
                                   int                  c,
                                   int                  ox,
                                   int                  oy)
{
  minesweeping_cell_state_t state;
  int                       x, y, n;

  state = ms->state[r][c];
  x     = ox + c * MINESWEEPING_CELL;
  y     = oy + r * MINESWEEPING_CELL;

  screen_fill_rect(scr, x, y,
                   SIZE2D(MINESWEEPING_CELL, MINESWEEPING_CELL),
                   state == minesweeping_REVEALED ?
                     ms->colours.cell_revealed : ms->colours.cell_hidden);
  screen_draw_rect(scr, x, y,
                   SIZE2D(MINESWEEPING_CELL, MINESWEEPING_CELL),
                   ms->colours.cell_border);

  if (state == minesweeping_FLAGGED)
  {
    screen_copy_bitmap(scr, x, y, ms->flag_bm);
    return;
  }

  if (state != minesweeping_REVEALED)
    return;

  if (ms->mine[r][c])
  {
    screen_copy_bitmap(scr, x, y, ms->mine_bm);
    return;
  }

  n = minesweeping_count_neighbours(ms, r, c);
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
    pos.x = x + (MINESWEEPING_CELL - width) / 2;
    pos.y = y + (MINESWEEPING_CELL - fh) / 2 + ascent;
    bmfont_draw(ms->font, scr, buf, 1, ms->colours.number[n],
               colour_rgba(0, 0, 0, 0), NULL, &pos, NULL);
  }
}

/* mines-remaining counter (left) and elapsed-seconds timer (right), drawn in
 * the HUD strip above the border */
static void minesweeping_draw_hud(minesweeping_task_t *ms,
                                  screen_t            *scr,
                                  const box_t         *bounds)
{
  char     buf[8];
  point_t  pos;
  colour_t fg, bg;
  int      fh, ascent;

  fg = ms->colours.hud_fg;
  bg = ms->colours.hud_bg;
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
static void minesweeping_draw_banner(minesweeping_task_t *ms,
                                     screen_t            *scr,
                                     const box_t         *bounds,
                                     const char          *text,
                                     colour_t             bg,
                                     colour_t             fg)
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

static result_t minesweeping_redraw(const wuss_event_t *event,
                                    void               *task_data)
{
  minesweeping_task_t *ms;
  screen_t            *scr;
  box_t                origin;
  const box_t         *bounds;
  const box_t         *clip;
  int                  board_y0;
  int                  r0, c0, r1, c1;
  int                  r, c;
  const char          *banner;

  ms  = task_data;
  scr = event->data.redraw.scr;

  /* everything below lays out from bounds' top-left, so shift it back by the
   * scroll offset: its x0,y0 are then the document origin, screen space */
  origin     = *event->data.redraw.bounds;
  origin.x0 -= event->data.redraw.scroll.x;
  origin.y0 -= event->data.redraw.scroll.y;
  bounds     = &origin;
  clip       = event->data.redraw.content;

  /* the HUD strip and the board below are repainted independently, each only
   * when the dirty region actually reaches it -- a flag toggle invalidates
   * one cell plus the mine counter, a timer tick just the timer field, so
   * either half alone is the common case and this keeps both cheap */
  board_y0 = bounds->y0 + MS_HUD_H(ms);

  minesweeping_tick_clock(ms);

  if (clip->y0 < board_y0)
  {
    /* the window owns every pixel (wuss_NO_BACKGROUND), so the HUD strip's
     * own backdrop is painted here before the counters go on top */
    screen_fill_rect(scr, bounds->x0, bounds->y0,
                     SIZE2D(MS_WIDTH(ms), MS_HUD_H(ms)),
                     ms->colours.fill);
    minesweeping_draw_hud(ms, scr, bounds);
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
                   ms->colours.fill);

  /* only the cells overlapping the dirty rect need redrawing */
  r0 = MAX(0, (clip->y0 - board_y0 - MS_BORDER) / MINESWEEPING_CELL);
  c0 = MAX(0, (clip->x0 - bounds->x0 - MS_BORDER) / MINESWEEPING_CELL);
  r1 = MIN(ms->rows, (clip->y1 - board_y0 - MS_BORDER + MINESWEEPING_CELL - 1) /
                     MINESWEEPING_CELL);
  c1 = MIN(ms->cols, (clip->x1 - bounds->x0 - MS_BORDER + MINESWEEPING_CELL - 1) /
                     MINESWEEPING_CELL);

  for (r = r0; r < r1; r++)
    for (c = c0; c < c1; c++)
      minesweeping_draw_cell(ms, scr, r, c, bounds->x0 + MS_BORDER,
                            bounds->y0 + MS_HUD_H(ms) + MS_BORDER);

  /* keyboard cursor: an inset outline, shown only while focused */
  if (wuss_get_focus(ms->wuss) == ms->window)
    screen_draw_rect(scr,
                     bounds->x0 + MS_BORDER + ms->cur_c * MINESWEEPING_CELL + 1,
                     board_y0 + MS_BORDER + ms->cur_r * MINESWEEPING_CELL + 1,
                     SIZE2D(MINESWEEPING_CELL - 2, MINESWEEPING_CELL - 2),
                     ms->colours.cursor);

  if (fireball_active(&ms->boom))
    fireball_render(&ms->boom, scr, bounds->x0, bounds->y0);

  banner = ms->dead     ? "BOOM! Click to retry"    :
           ms->new_best ? "New best! Click to retry" :
           ms->won      ? "You win! Click to retry"  : NULL;
  if (banner != NULL)
    minesweeping_draw_banner(ms, scr, bounds, banner,
                            ms->dead ? ms->colours.dead_bg : ms->colours.won_bg,
                            ms->colours.banner_fg);

  return result_OK;
}

/* Select reveals cell (r,c), Adjust toggles its flag; either restarts a
 * finished game wherever it lands. (r,c) may be off the board. */
static result_t minesweeping_act(minesweeping_task_t *ms,
                                 int                  r,
                                 int                  c,
                                 wuss_button_t        button)
{
  if (ms->dead || ms->won)
  {
    /* any click on a finished board starts a fresh game */
    if (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST))
    {
      minesweeping_reset(ms);
      wuss_window_invalidate_visible(ms->window);
    }
    return result_OK;
  }

  if (!minesweeping_in_bounds(ms, r, c))
    return result_OK;

  if (button & wuss_BUTTON_SELECT)
  {
    minesweeping_cellbox_t touched;
    box_t                  local;

    if (ms->state[r][c] == minesweeping_FLAGGED)
      return result_OK;

    if (!ms->placed)
    {
      minesweeping_place_mines(ms, r, c);
      ms->start_time = time(NULL);
    }

    touched.r0 = touched.r1 = r;
    touched.c0 = touched.c1 = c;
    if (ms->state[r][c] == minesweeping_REVEALED)
      minesweeping_chord(ms, r, c, &touched);
    else
      minesweeping_reveal(ms, r, c, &touched);

    if (ms->dead)
    {
      minesweeping_reveal_all_mines(ms);
      fireball_burst(&ms->boom,
                     MS_BORDER + c * MINESWEEPING_CELL + MINESWEEPING_CELL / 2,
                     MS_HUD_H(ms) + MS_BORDER + r * MINESWEEPING_CELL +
                     MINESWEEPING_CELL / 2,
                     MS_WIDTH(ms), MS_HEIGHT(ms), MS_BOOM_PARTICLES);
      wuss_window_invalidate_visible(ms->window); /* every mine, plus banner */
    }
    else
    {
      minesweeping_tick_clock(ms); /* before won freezes it */
      ms->won = minesweeping_check_won(ms);
      if (ms->won)
      {
        minesweeping_record_time(ms);
        wuss_window_invalidate_visible(ms->window); /* banner covers the lot */
      }
      else
      {
        local = MS_CELLS_BOX(ms, touched.r0, touched.c0, touched.r1,
                             touched.c1);
        wuss_window_invalidate(ms->window, &local);
      }
    }
    minesweeping_tick_clock(ms);
  }
  else if (button & wuss_BUTTON_ADJUST)
  {
    box_t local = MS_CELLS_BOX(ms, r, c, r + 1, c + 1);

    if (ms->state[r][c] == minesweeping_HIDDEN)
    {
      ms->state[r][c] = minesweeping_FLAGGED;
      ms->flags++;
    }
    else if (ms->state[r][c] == minesweeping_FLAGGED)
    {
      ms->state[r][c] = minesweeping_HIDDEN;
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

  return result_OK;
}

static void minesweeping_invalidate_cursor(minesweeping_task_t *ms)
{
  box_t local;

  local = MS_CELLS_BOX(ms, ms->cur_r, ms->cur_c, ms->cur_r + 1, ms->cur_c + 1);
  wuss_window_invalidate(ms->window, &local);
}

/* arrows move the cursor (clamped at the edges), Return acts as Select and
 * Space as Adjust on the cursor cell; everything else is passed back
 * unclaimed */
static result_t minesweeping_key(minesweeping_task_t *ms, int code)
{
  int r, c;

  r = ms->cur_r;
  c = ms->cur_c;

  switch (code)
  {
  case wuss_KEY_UP:    r--; break;
  case wuss_KEY_DOWN:  r++; break;
  case wuss_KEY_LEFT:  c--; break;
  case wuss_KEY_RIGHT: c++; break;
  case 13:             return minesweeping_act(ms, r, c, wuss_BUTTON_SELECT);
  case ' ':            return minesweeping_act(ms, r, c, wuss_BUTTON_ADJUST);
  default:             return result_WUSS_KEY_UNCLAIMED;
  }

  r = CLAMP(r, 0, ms->rows - 1);
  c = CLAMP(c, 0, ms->cols - 1);
  if (r == ms->cur_r && c == ms->cur_c)
    return result_OK;

  minesweeping_invalidate_cursor(ms);
  ms->cur_r = r;
  ms->cur_c = c;
  minesweeping_invalidate_cursor(ms);

  return result_OK;
}

/* Give Up only makes sense while a game is in progress. Also shades the
 * open menu, if any, so an Adjust pick shows it at once. */
static void minesweeping_menu_update(minesweeping_task_t *ms)
{
  wuss_menu_item_t *give_up;
  bool              disabled;

  give_up  = &ms->menu_items[MINESWEEPING_MENU_GIVE_UP];
  disabled = !(ms->placed && !ms->dead && !ms->won);
  if (disabled)
    give_up->flags |= wuss_MENU_ITEM_DISABLED;
  else
    give_up->flags &= ~wuss_MENU_ITEM_DISABLED;

  wuss_menu_disable_item_live(ms->menu_handle, &ms->menu,
                              MINESWEEPING_MENU_GIVE_UP, disabled);
}

static result_t minesweeping_mouse(minesweeping_task_t *ms,
                                   point_t              point,
                                   wuss_button_t        button)
{
  int r, c;

  if (button & wuss_BUTTON_MENU)
  {
    wuss_menu_tick_exclusive(&ms->size_menu, ms->size);
    wuss_menu_tick_exclusive(&ms->difficulty_menu, ms->difficulty);
    minesweeping_menu_update(ms);

    return wuss_menu_open_at_pointer(ms->task, &ms->menu, &ms->menu_handle);
  }

  c = (point.x - MS_BORDER) / MINESWEEPING_CELL;
  r = (point.y - MS_HUD_H(ms) - MS_BORDER) / MINESWEEPING_CELL;
  if (point.x < MS_BORDER || point.y < MS_HUD_H(ms) + MS_BORDER)
    r = c = -1; /* in the frame or HUD: division truncates towards zero */

  return minesweeping_act(ms, r, c, button);
}

result_t minesweeping_handle(wuss_window_t      *window,
                             const wuss_event_t *event,
                             void               *task_data)
{
  minesweeping_task_t *ms;

  ms = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return minesweeping_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != ms->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return minesweeping_mouse(ms, event->data.mouse.point,
                             event->data.mouse.button);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != ms->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(ms->task, &ms->menu, event, &rc))
      return rc;

    return minesweeping_key(ms, event->data.key.code);
  }

  case wuss_EVENT_GAIN_FOCUS:
  case wuss_EVENT_LOSE_FOCUS:
    minesweeping_invalidate_cursor(ms); /* show or hide the cursor */
    return result_OK;

  case wuss_EVENT_IDLE:
    /* the shared proginfo singleton is a second window on this same
     * (autoclose) delegate while its dialogue is open, so closing the board
     * window alone doesn't necessarily empty task->windows immediately --
     * guard against the dangling window in the meantime */
    if (ms->window == NULL)
      return result_OK;
    if (fireball_active(&ms->boom))
    {
      fireball_step(&ms->boom);
      wuss_window_invalidate_visible(ms->window);
    }
    if (ms->placed && !ms->dead && !ms->won)
    {
      int was;

      was = ms->elapsed;
      minesweeping_tick_clock(ms);
      if (ms->elapsed != was)
      {
        box_t timer = MS_TIMER_BOX(ms);

        wuss_window_invalidate(ms->window, &timer);
      }
    }
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &ms->size_menu)
    {
      minesweeping_set_size(ms, (minesweeping_size_t)
                           event->data.menu_select.index);
      wuss_menu_tick_exclusive_live(ms->menu_handle, &ms->size_menu,
                                    ms->size);
    }
    else if (event->data.menu_select.menu == &ms->difficulty_menu)
    {
      ms->difficulty = (minesweeping_difficulty_t)
                       event->data.menu_select.index;
      minesweeping_set_size(ms, ms->size); /* re-derive the mine count */
      wuss_menu_tick_exclusive_live(ms->menu_handle, &ms->difficulty_menu,
                                    ms->difficulty);
    }
    else if (event->data.menu_select.index == MINESWEEPING_MENU_GIVE_UP)
    {
      /* lose on the spot: freeze the clock, then show every mine */
      if (!ms->placed || ms->dead || ms->won)
        return result_OK;

      minesweeping_tick_clock(ms);
      ms->dead = true;
      minesweeping_reveal_all_mines(ms);
      minesweeping_menu_update(ms);
      wuss_window_invalidate_visible(ms->window);
      return result_OK;
    }
    else if (event->data.menu_select.index == MINESWEEPING_MENU_NEW_GAME)
    {
      minesweeping_reset(ms);
    }
    else
    {
      return result_OK; /* submenu rows act through their own picks */
    }

    {
      size2d_t sz = SIZE2D(MS_WIDTH(ms), MS_HEIGHT(ms));

      wuss_window_resize(ms->window, sz);
      wuss_window_set_doc(ms->window, sz);
    }
    minesweeping_menu_update(ms);
    wuss_window_invalidate_visible(ms->window);
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    ms->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == ms->window)
      ms->window = NULL;
    return result_OK;

  case wuss_EVENT_PALETTE:
    minesweeping_prepare_colours(ms);
    wuss_window_invalidate_visible(ms->window);
    return result_OK;

  case wuss_EVENT_QUIT:
    minesweeping_destroy(ms);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
