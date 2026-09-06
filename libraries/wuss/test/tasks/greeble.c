/* wuss/test/tasks/greeble.c -- random-scatter greebling pattern task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/colour.h"
#include "geom/box.h"
#include "wuss/menu.h"

#include "greeble.h"
#include "greeble-tiles.h"

/* ----------------------------------------------------------------------- */

/* MENU click over the content pops this. The sole row is an independent
 * toggle for per-prefab random palettes. */
enum { GREEBLE_MENU_RANDPAL = 0 };

static wuss_menu_item_t g_greeble_menu_items[] =
{
  { "Random palettes", wuss_MENU_ITEM_NONE, NULL, NULL }
};

static const wuss_menu_t g_greeble_menu =
{
  "Greeble", g_greeble_menu_items, NELEMS(g_greeble_menu_items)
};

/* ----------------------------------------------------------------------- */

/* cell value for "nothing placed here yet"; stamp indices are 0..204 so 0xFF
 * is free. greeble_redraw skips these. */
#define GREEBLE_EMPTY 0xFF

/* one xorshift32 step on lvalue s; kept local to greeble_generate so the same
 * seed always yields the same grid */
#define GREEBLE_XORSHIFT(s) \
  ((s) ^= (s) << 13, (s) ^= (s) >> 17, (s) ^= (s) << 5)

/* try to drop prefab p with its top-left at (row,col): succeeds only if every
 * non-hole cell of the prefab lands on an in-bounds, still-empty grid cell, so
 * blocks never overlap. pal is the greeble_palettes[] row to tag every cell
 * this block fills with. returns 1 on placement. */
static int greeble_try_prefab(greeble_task_t *task,
                              int             p,
                              int             row,
                              int             col,
                              unsigned char   pal)
{
  const unsigned char *cells;
  int                  w, h, x, y;

  w     = greeble_prefab[p].w;
  h     = greeble_prefab[p].h;
  cells = greeble_prefab_cells + greeble_prefab[p].off;

  if (row < 0 || col < 0 ||
      row + h > task->rows || col + w > task->cols)
    return 0;

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      if (cells[y * w + x] != GREEBLE_PREFAB_HOLE &&
          task->grid[row + y][col + x] != GREEBLE_EMPTY)
        return 0;

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      if (cells[y * w + x] != GREEBLE_PREFAB_HOLE)
      {
        task->grid[row + y][col + x]    = cells[y * w + x];
        task->cellpal[row + y][col + x] = pal;
      }

  return 1;
}

/* Fill the grid by scattering the artist's prefab blocks (greeble-tiles.h)
 * without overlap, then dropping a loose stamp into every cell no block
 * claimed. The blocks carry the intended circuit shapes; the loose stamps come
 * from greeble_filler[] -- the stamps the artist drew standing alone, i.e. the
 * shortlist that reads well without neighbours. GREEBLE_PREFAB_ATTEMPTS
 * placement tries give a dense but varied cover; more attempts just repaint
 * cells already taken. */
#define GREEBLE_PREFAB_ATTEMPTS (GREEBLE_MAX_COLS * GREEBLE_MAX_ROWS)

static void greeble_generate(greeble_task_t *task)
{
  int          r, c, i;
  unsigned int s;

  s = task->seed;

  /* grid[][] is contiguous; the cols..MAX_COLS tail of each live row is never
   * read, so one clear over the live rows is enough. GREEBLE_EMPTY is a byte
   * value, so memset is exact. cellpal[][] defaults to task->palette; prefab
   * placements overwrite their own cells when random palettes are on. */
  memset(task->grid, GREEBLE_EMPTY,
         (size_t) task->rows * GREEBLE_MAX_COLS);
  memset(task->cellpal, task->palette,
         (size_t) task->rows * GREEBLE_MAX_COLS);

  for (i = 0; i < GREEBLE_PREFAB_ATTEMPTS && GREEBLE_NPREFAB > 0; i++)
  {
    int           p, row, col;
    unsigned char pal;

    GREEBLE_XORSHIFT(s);
    p = (int) (s % (unsigned int) GREEBLE_NPREFAB);
    GREEBLE_XORSHIFT(s);
    row = (int) (s % (unsigned int) task->rows);
    GREEBLE_XORSHIFT(s);
    col = (int) (s % (unsigned int) task->cols);

    pal = task->palette;
    if (task->random_prefab_palettes)
    {
      GREEBLE_XORSHIFT(s);
      pal = (unsigned char) (s % (unsigned int) GREEBLE_NPALETTE);
    }

    greeble_try_prefab(task, p, row, col, pal);
  }

  for (r = 0; r < task->rows; r++)
    for (c = 0; c < task->cols; c++)
      if (task->grid[r][c] == GREEBLE_EMPTY)
      {
        GREEBLE_XORSHIFT(s);
        task->grid[r][c] =
          greeble_filler[s % (unsigned int) GREEBLE_NFILLER];
      }
}

/* recompute the grid extent for the window's content box, then regenerate */
static void greeble_relayout(greeble_task_t *task, const box_t *content)
{
  task->cols = (content->x1 - content->x0) / GREEBLE_TILE_PX;
  task->rows = (content->y1 - content->y0) / GREEBLE_TILE_PX;
  task->cols = CLAMP(task->cols, 1, GREEBLE_MAX_COLS);
  task->rows = CLAMP(task->rows, 1, GREEBLE_MAX_ROWS);
  greeble_generate(task);
}

/* ----------------------------------------------------------------------- */

/* Blit one 8x8 stamp 1:1 at (ox,oy). palette is the four colours for this
 * pattern's greeble_palettes[] row, indexed by the stamp's 2-bit slots.
 * Each row is walked as same-slot runs and drawn with screen_fill_hline
 * (which clips and writes contiguous words) rather than a pixel at a time:
 * a stamp row is 1-4 runs, not 8 clipped stores. */
static void greeble_stamp(screen_t       *scr,
                          const colour_t *palette,
                          int             tile_index,
                          int             ox,
                          int             oy)
{
  const unsigned short *rows;
  int                   x, y;

  rows = greeble_tiles[tile_index];

  for (y = 0; y < GREEBLE_TILE_PX; y++)
  {
    unsigned short bits = rows[y];
    int            run_start = 0;
    int            run_slot = bits & 3;

    for (x = 1; x <= GREEBLE_TILE_PX; x++)
    {
      int slot = (x < GREEBLE_TILE_PX) ? ((bits >> (2 * x)) & 3) : -1;

      if (slot != run_slot)
      {
        screen_fill_hline(scr, ox + run_start, oy + y,
                          x - run_start, palette[run_slot]);
        run_start = x;
        run_slot  = slot;
      }
    }
  }
}

static result_t greeble_redraw(const wuss_event_t *event,
                               greeble_task_t     *task)
{
  screen_t       *scr;
  const box_t    *content, *bounds;
  colour_t        palette[GREEBLE_NPALETTE][4];
  int             p, r, c, sx, sy, ox, oy;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  /* expand every palette row into colour_t once; cells index by cellpal[][] */
  for (p = 0; p < GREEBLE_NPALETTE; p++)
    for (r = 0; r < 4; r++)
    {
      unsigned int v = greeble_palettes[p][r];
      palette[p][r] = colour_rgba((int) ( v        & 0xFF),
                                  (int) ((v >>  8) & 0xFF),
                                  (int) ((v >> 16) & 0xFF),
                                  (int) ((v >> 24) & 0xFF));
    }

  for (r = 0; r < task->rows; r++)
  {
    oy = bounds->y0 - sy + r * GREEBLE_TILE_PX;
    if (oy + GREEBLE_TILE_PX <= content->y0 || oy >= content->y1)
      continue;

    for (c = 0; c < task->cols; c++)
    {
      ox = bounds->x0 - sx + c * GREEBLE_TILE_PX;
      if (ox + GREEBLE_TILE_PX <= content->x0 || ox >= content->x1)
        continue;

      greeble_stamp(scr, palette[task->cellpal[r][c]],
                    task->grid[r][c], ox, oy);
    }
  }

  return result_OK;
}

/* Select: advance to a fresh pattern (new seed, regenerate the grid). */
static result_t greeble_select(greeble_task_t *task, wuss_window_t *window)
{
  /* LCG step; any nonzero state keeps the pattern xorshift off its fixed
   * point */
  task->seed = task->seed * 1664525u + 1013904223u;
  if (task->seed == 0)
    task->seed = 1;

  greeble_generate(task);
  wuss_window_invalidate_all(window);

  return result_OK;
}

/* Flip per-prefab random palettes and regenerate from the same seed so it is a
 * straight A/B of the pattern. The shared menu struct's tick is not touched
 * here: a fresh open re-syncs it from task state, and an ADJUST pick reticks
 * the open chain in place (see greeble_menu_select). Shared by the Adjust
 * click and the menu row. */
static result_t greeble_toggle_randpal(greeble_task_t *task,
                                       wuss_window_t  *window)
{
  task->random_prefab_palettes = !task->random_prefab_palettes;
  greeble_generate(task);
  wuss_window_invalidate_all(window);

  return result_OK;
}

/* Adjust: toggle per-prefab random palettes on the current pattern. */
static result_t greeble_adjust(greeble_task_t *task, wuss_window_t *window)
{
  return greeble_toggle_randpal(task, window);
}

/* Menu pick: the sole row toggles per-prefab random palettes. An ADJUST pick
 * keeps the chain open without rebuilding it, so the tick set at open is now
 * stale on screen -- retick the still-open level in place. A SELECT pick has
 * already closed and freed the chain by the time this arrives, so the handle
 * is stale; drop it. */
static result_t greeble_menu_select(greeble_task_t     *task,
                                    const wuss_event_t *event)
{
  result_t rc;

  if (event->data.menu_select.menu != &g_greeble_menu)
    return result_OK;
  if (event->data.menu_select.index != GREEBLE_MENU_RANDPAL)
    return result_OK;

  rc = greeble_toggle_randpal(task, task->window);

  if (event->data.menu_select.button & wuss_BUTTON_ADJUST)
    wuss_menu_set_item_ticked(task->menu_handle, &g_greeble_menu,
                              GREEBLE_MENU_RANDPAL,
                              task->random_prefab_palettes);
  else
    task->menu_handle = NULL;

  return rc;
}

result_t greeble_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  greeble_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return greeble_redraw(event, task);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_MENU)
    {
      /* the menu struct is shared by every greeble window; sync its tick to
       * this window's state before it opens */
      g_greeble_menu_items[GREEBLE_MENU_RANDPAL].flags =
        task->random_prefab_palettes ? wuss_MENU_ITEM_TICKED
                                     : wuss_MENU_ITEM_NONE;
      return wuss_menu_open(task->delegate, &g_greeble_menu,
                            wuss_get_pointer(task->wuss),
                            &task->menu_handle);
    }
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      return greeble_select(task, window);
    if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      return greeble_adjust(task, window);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    return greeble_menu_select(task, event);

  case wuss_EVENT_QUIT:
    free(task); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

result_t greeble_create(wuss_t *wuss, greeble_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  box_t            content;
  result_t         rc;

  task->wuss        = wuss;
  task->menu_handle = NULL;
  task->seed        = 0x9E3779B9u; /* any nonzero start */
  task->palette     = 0;           /* Adjust cycles from here */
  task->random_prefab_palettes = 0; /* Menu toggles this */

  /* greeble_redraw paints every pixel itself */
  delegate_desc.handle    = greeble_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "greeble";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate; /* the task the menu opens against */
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(160, 320),
                                 "Greeble",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 SIZE2D(160, 320),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  wuss_window_get_content_bounds(task->window, &content);
  greeble_relayout(task, &content);

  return result_OK;
}

#endif /* WUSS_APP */
