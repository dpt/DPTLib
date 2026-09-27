/* wuss/test/tasks/greeble.c -- random-scatter greebling pattern task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/colour.h"
#include "geom/box.h"
#include "utils/rng.h"
#include "wuss/menu.h"

#include "greeble.h"
#include "greeble-tiles.h"
#include "snapshot.h"

/* ----------------------------------------------------------------------- */

/* MENU click over the content pops this. "Random palettes" is an independent
 * toggle for per-prefab random palettes. "Info" is a wuss_menu_item_t.window
 * leaf retargeted at the shared proginfo singleton just before the menu
 * opens. The item table and wuss_menu_t live per-instance in greeble_task_t,
 * not as a file-scope static, so that each window's Info row and tick state
 * are its own rather than every instance sharing (and overwriting) one
 * global. */
enum
{
  GREEBLE_MENU_INFO = 0,
  GREEBLE_MENU_RANDPAL,
  GREEBLE_MENU_PALETTE,
  GREEBLE_MENU_SIZE,
  GREEBLE_MENU_SAVE
};

#define GREEBLE_SAVE_NAME "greeble.png" /* written to the current dir */

/* the task block sizes its Palette submenu by GREEBLE_MAX_PALETTES, since
 * greeble-tiles.h (and so GREEBLE_NPALETTE) is private to this file */
typedef char greeble_palette_cap_check[GREEBLE_NPALETTE <= GREEBLE_MAX_PALETTES
                                       ? 1 : -1];

/* Size submenu presets, in tiles; each fits the GREEBLE_MAX_* caps. The
 * last is the full grid and the one a new window opens at. */
static const struct
{
  const char *name;
  int         cols, rows;
}
greeble_sizes[GREEBLE_NSIZES] =
{
  { "16 x 16", 16, 16 },
  { "32 x 24", 32, 24 },
  { "40 x 30", 40, 30 },
  { "48 x 48", 48, 48 }
};

/* ----------------------------------------------------------------------- */

/* cell value for "nothing placed here yet"; stamp indices are 0..204 so 0xFF
 * is free. greeble_redraw skips these. */
#define GREEBLE_EMPTY 0xFF

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
 * cells already taken.
 *
 * Blocks are tried largest-footprint first: once small blocks and filler have
 * fragmented the grid a big prefab's bounding box rarely lands wholly empty,
 * so a random prefab order leaves the large shapes almost never placed. The
 * attempt budget is split evenly across prefabs; each prefab gets its slice of
 * random (row,col) tries in descending-area order. */
#define GREEBLE_PREFAB_ATTEMPTS (GREEBLE_MAX_COLS * GREEBLE_MAX_ROWS)

static void greeble_generate(greeble_task_t *task)
{
  rng_t         s;
  int           i;
  unsigned char order[GREEBLE_NPREFAB]; /* prefab indices, largest area first */
  int           tries_each;
  int           r, c;

  rng_seed(&s, task->seed);

  /* insertion-sort a fresh index list by w*h descending; NPREFAB is small and
   * this runs once per regenerate */
  for (i = 0; i < GREEBLE_NPREFAB; i++)
  {
    int a, j;

    a = greeble_prefab[i].w * greeble_prefab[i].h;
    for (j = i; j > 0; j--)
    {
      int b = greeble_prefab[order[j - 1]].w * greeble_prefab[order[j - 1]].h;
      if (b >= a)
        break;
      order[j] = order[j - 1];
    }
    order[j] = (unsigned char) i;
  }

  tries_each = GREEBLE_NPREFAB > 0
             ? GREEBLE_PREFAB_ATTEMPTS / GREEBLE_NPREFAB
             : 0;
  if (tries_each < 1)
    tries_each = 1;

  /* grid[][] is contiguous; the cols..MAX_COLS tail of each live row is never
   * read, so one clear over the live rows is enough. GREEBLE_EMPTY is a byte
   * value, so memset is exact. cellpal[][] defaults to task->palette; prefab
   * placements overwrite their own cells when random palettes are on. */
  memset(task->grid, GREEBLE_EMPTY,
         (size_t) task->rows * GREEBLE_MAX_COLS);
  memset(task->cellpal, task->palette,
         (size_t) task->rows * GREEBLE_MAX_COLS);

  for (i = 0; i < GREEBLE_NPREFAB; i++)
  {
    int p, t;

    p = order[i];

    for (t = 0; t < tries_each; t++)
    {
      int           row, col;
      unsigned char pal;

      row = rng_range(&s, task->rows);
      col = rng_range(&s, task->cols);

      pal = task->palette;
      if (task->random_prefab_palettes)
        pal = (unsigned char) rng_range(&s, GREEBLE_NPALETTE);

      greeble_try_prefab(task, p, row, col, pal);
    }
  }

  for (r = 0; r < task->rows; r++)
    for (c = 0; c < task->cols; c++)
      if (task->grid[r][c] == GREEBLE_EMPTY)
        task->grid[r][c] =
          greeble_filler[rng_range(&s, GREEBLE_NFILLER)];
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
  screen_t    *scr;
  const box_t *content, *bounds;
  colour_t     palette[GREEBLE_NPALETTE][4];
  int          p, r, c, sx, sy, ox, oy;

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
  /* LCG-hop to the next start seed; rng_range's xorshift then walks that.
   * Remap the one zero state so the xorshift never lands on its fixed
   * point. */
  rng_lcg32(&task->seed);
  if (task->seed == 0)
    task->seed = 1;

  greeble_generate(task);
  wuss_window_invalidate_extent(window);

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
  wuss_window_invalidate_extent(window);

  return result_OK;
}

/* Switch the base palette on the current pattern. Regenerate so filler
 * cells (and prefab cells, when random palettes are off) pick up the new row;
 * the seed is unchanged so the layout is identical. Shared by the Adjust
 * click and the Palette submenu. */
static result_t greeble_set_palette(greeble_task_t *task, int index)
{
  task->palette = (unsigned char) index;
  greeble_generate(task);
  if (task->window != NULL)
    wuss_window_invalidate_extent(task->window);

  return result_OK;
}

/* Switch the grid to a size preset: regenerate from the same seed, then fit
 * the document extent and the window to it. */
static result_t greeble_set_size(greeble_task_t *task, int index)
{
  result_t rc;
  size2d_t px;

  task->size = index;
  task->cols = greeble_sizes[index].cols;
  task->rows = greeble_sizes[index].rows;
  greeble_generate(task);

  px = SIZE2D(task->cols * GREEBLE_TILE_PX, task->rows * GREEBLE_TILE_PX);

  rc = wuss_window_set_doc(task->window, px);
  if (rc != result_OK)
    return rc;

  return wuss_window_resize(task->window, px);
}

/* Adjust: step to the next base palette. */
static result_t greeble_adjust(greeble_task_t *task)
{
  return greeble_set_palette(task, (task->palette + 1) % GREEBLE_NPALETTE);
}

/* Space reseeds, as Select does; Left/Right step the base palette back and
 * forward. Anything else is passed back unclaimed. */
static result_t greeble_key(greeble_task_t *task,
                            wuss_window_t  *window,
                            int             code)
{
  switch (code)
  {
  case ' ':
    return greeble_select(task, window);

  case wuss_KEY_LEFT:
    return greeble_set_palette(task, (task->palette + GREEBLE_NPALETTE - 1) %
                                     GREEBLE_NPALETTE);

  case wuss_KEY_RIGHT:
    return greeble_adjust(task);

  default:
    return result_WUSS_KEY_UNCLAIMED;
  }
}

/* Menu pick: the Random palettes row toggles per-prefab random palettes; a
 * Palette submenu row picks the base palette. An ADJUST pick keeps the chain
 * open without rebuilding it, so the tick set at open is now
 * stale on screen -- retick the still-open level in place. A SELECT pick has
 * already closed and freed the chain by the time this arrives, so the handle
 * is stale; drop it. */
static result_t greeble_menu_select(greeble_task_t     *task,
                                    const wuss_event_t *event)
{
  result_t rc;

  if (event->data.menu_select.menu == &task->palette_menu)
  {
    rc = greeble_set_palette(task, event->data.menu_select.index);
    wuss_menu_tick_exclusive_live(task->menu_handle, &task->palette_menu,
                                  task->palette);
  }
  else if (event->data.menu_select.menu == &task->size_menu)
  {
    rc = greeble_set_size(task, event->data.menu_select.index);
    wuss_menu_tick_exclusive_live(task->menu_handle, &task->size_menu,
                                  task->size);
  }
  else if (event->data.menu_select.menu == &task->menu &&
           event->data.menu_select.index == GREEBLE_MENU_RANDPAL)
  {
    rc = greeble_toggle_randpal(task, task->window);
    wuss_menu_tick_item_live(task->menu_handle, &task->menu,
                             GREEBLE_MENU_RANDPAL,
                             task->random_prefab_palettes);
  }
  else if (event->data.menu_select.menu == &task->menu &&
           event->data.menu_select.index == GREEBLE_MENU_SAVE)
  {
    rc = snapshot_save_png(task->window, greeble_handle, task,
                           GREEBLE_SAVE_NAME);
  }
  else
  {
    return result_OK;
  }

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
    if (window != task->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_MENU)
    {
      static const wuss_proginfo_desc_t desc =
      {
        "Greeble",
        "Prefab-scatter greebling pattern",
        "© DPTLib contributors",
        "1.0 (" __DATE__ ")"
      };
      unsigned int ticks;

      wuss_proginfo_set_desc(&desc);
      task->menu_items[GREEBLE_MENU_INFO].window =
        wuss_proginfo_window(task->delegate);

      wuss_menu_tick_exclusive(&task->palette_menu, task->palette);
      wuss_menu_tick_exclusive(&task->size_menu, task->size);

      ticks = task->random_prefab_palettes ? 1u << GREEBLE_MENU_RANDPAL : 0;
      return wuss_menu_open_ticked(task->delegate, &task->menu, ticks,
                                   wuss_get_pointer(task->wuss),
                                   &task->menu_handle);
    }
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      return greeble_select(task, window);
    if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      return greeble_adjust(task);
    return result_OK;

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != task->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    rc = wuss_menu_dispatch_shortcut(task->delegate, &task->menu, event);
    if (rc != result_WUSS_KEY_UNCLAIMED)
      return rc;

    if (event->data.key.modifiers & (wuss_KEY_MOD_CTRL | wuss_KEY_MOD_ALT))
      return result_WUSS_KEY_UNCLAIMED;
    return greeble_key(task, window, event->data.key.code);
  }

  case wuss_EVENT_MENU_SELECT:
    return greeble_menu_select(task, event);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == task->menu_items[GREEBLE_MENU_INFO].window)
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

  case wuss_EVENT_QUIT:
    greeble_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

result_t greeble_create(wuss_t *wuss, greeble_task_t **out)
{
  result_t         rc;
  greeble_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  size2d_t         grid_px;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  /* open at the last (full grid) size preset */
  task->size = GREEBLE_NSIZES - 1;
  task->cols = greeble_sizes[task->size].cols;
  task->rows = greeble_sizes[task->size].rows;
  grid_px    = SIZE2D(task->cols * GREEBLE_TILE_PX,
                      task->rows * GREEBLE_TILE_PX);

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
                                 grid_px,
                                 "Greeble",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 grid_px,
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  /* fill the whole document, not just the visible content box: placement
   * can shrink the window below grid_px and the rest scrolls into view */
  greeble_generate(task);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, GREEBLE_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open_ticked, in
                                * greeble_handle */

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, GREEBLE_MENU_RANDPAL,
                          "Random palettes", wuss_MENU_ITEM_NONE, "R");

  /* ponytail: the palettes are unnamed, so the rows are numbered */
  for (i = 0; i < GREEBLE_NPALETTE; i++)
  {
    snprintf(task->palette_names[i], sizeof(task->palette_names[i]), "%d",
             i + 1);
    WUSS_MENU_ITEM(task->palette_items, i, task->palette_names[i],
                   wuss_MENU_ITEM_NONE);
  }

  WUSS_MENU_TITLE(task->palette_menu, "Palette", task->palette_items,
                 GREEBLE_NPALETTE);

  WUSS_MENU_ITEM_MENU(task->menu_items, GREEBLE_MENU_PALETTE, "Palette",
                      wuss_MENU_ITEM_NONE, &task->palette_menu);

  for (i = 0; i < GREEBLE_NSIZES; i++)
    WUSS_MENU_ITEM(task->size_items, i, greeble_sizes[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->size_menu, "Size", task->size_items, GREEBLE_NSIZES);

  WUSS_MENU_ITEM_MENU(task->menu_items, GREEBLE_MENU_SIZE, "Size",
                      wuss_MENU_ITEM_NONE, &task->size_menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, GREEBLE_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");

  WUSS_MENU_TITLE(task->menu, "Greeble", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void greeble_destroy(greeble_task_t *task)
{
  wuss_menu_close(task->menu_handle);
  task->menu_handle = NULL;

  free(task);
}

#endif /* WUSS_APP */
