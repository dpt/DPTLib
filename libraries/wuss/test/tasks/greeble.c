/* wuss/test/tasks/greeble.c -- random-scatter greebling pattern task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "greeble.h"
#include "greeble-tiles.h"

/* The four stamp palette slots (0..3 in greeble_tiles[]) map to these Wuss
 * palette indices. The source art is PICO-8, so these are its black,
 * dark-purple, red and orange; a non-PICO Wuss palette simply recolours the
 * pattern through the same slots. */
static const int greeble_palette[4] =
{
  palette_PICO8_BLACK,
  palette_PICO8_DARK_PURPLE,
  palette_PICO8_RED,
  palette_PICO8_ORANGE
};

/* ----------------------------------------------------------------------- */

/* Fill every cell with a stamp drawn uniformly at random. The tile sheet is a
 * scrapbook of decorative 8x8 fragments rather than a connective tile set, so
 * edge matching only ever collapsed the plane into noise; a flat random
 * scatter reads as the intended greeble texture and needs no adjacency data.
 * greeble_tile_edge[] in greeble-tiles.h is left unused. */
static void greeble_generate(greeble_task_t *task)
{
  int          r, c;
  unsigned int s;

  s = task->seed;

  for (r = 0; r < task->rows; r++)
  {
    for (c = 0; c < task->cols; c++)
    {
      /* xorshift32, kept local so the same seed always yields the same grid */
      s ^= s << 13; s ^= s >> 17; s ^= s << 5;
      task->grid[r][c] = (unsigned char) (s % (unsigned int) GREEBLE_NTILES);
    }
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

/* Blit one 8x8 stamp 1:1 at (ox,oy). Clipped implicitly by screen_set_pixel;
 * only pixels inside the redraw's content box need drawing, but the stamp is
 * tiny and wuss clips per pixel, so an unconditional 8x8 loop is fine. */
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

    for (x = 0; x < GREEBLE_TILE_PX; x++)
    {
      int slot = (bits >> (2 * x)) & 3;

      screen_set_pixel(scr, ox + x, oy + y,
                       palette[greeble_palette[slot]]);
    }
  }
}

static result_t greeble_redraw(const wuss_event_t *event,
                               greeble_task_t     *task)
{
  screen_t       *scr;
  const box_t    *content, *bounds;
  const colour_t *palette;
  int             r, c, sx, sy, ox, oy;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;
  palette = scr->palette;

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

      greeble_stamp(scr, palette, task->grid[r][c], ox, oy);
    }
  }

  return result_OK;
}

static result_t greeble_mouse(greeble_task_t *task, wuss_window_t *window)
{
  /* advance to a fresh pattern; any nonzero state keeps xorshift off its
   * fixed point */
  task->seed = task->seed * 1664525u + 1013904223u;
  if (task->seed == 0)
    task->seed = 1;

  greeble_generate(task);
  wuss_window_invalidate_all(window);

  return result_OK;
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
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_SELECT))
      return result_OK;
    return greeble_mouse(task, window);

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

  task->seed = 0x9E3779B9u; /* any nonzero start */

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
