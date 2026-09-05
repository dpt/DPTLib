/* wuss/test/tasks/greeble.c -- edge-matched greebling pattern task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "greeble.h"

#define GREEBLE_TILE_DEFAULT 10 /* pixels per tile, so a strip reads clearly */
#define GREEBLE_TILE_MIN     6
#define GREEBLE_TILE_MAX     16

#define GREEBLE_STROKE       2  /* wire width in pixels */
#define GREEBLE_SHADOW_OFF   1  /* drop-shadow offset, down and right */

/* Tile catalogue. Each tile opens a wire toward some subset of its four edges;
 * an edge is either open (a wire crosses its midpoint) or closed. Neighbouring
 * tiles are compatible when the edges they share agree. The bag is the tile
 * list with repeats: repeating a tile raises how often the greedy picker lands
 * on it. Blank dominates so the pattern breathes; the decorated blank/straight
 * variants share their plain tile's edges but stamp an extra frame corner or
 * pip, which is what gives the nested-rectangle and lone-dot look. */
enum
{
  greeble_N = 1 << 0,
  greeble_E = 1 << 1,
  greeble_S = 1 << 2,
  greeble_W = 1 << 3
};

enum
{
  greeble_TILE_BLANK,
  greeble_TILE_PIP,        /* blank + a 2x2 dot at centre */
  greeble_TILE_STUB_N,
  greeble_TILE_STUB_E,
  greeble_TILE_STUB_S,
  greeble_TILE_STUB_W,
  greeble_TILE_VERT,
  greeble_TILE_HORIZ,
  greeble_TILE_FRAME_V,    /* vertical + a short frame spur to the east */
  greeble_TILE_FRAME_H,    /* horizontal + a short frame spur to the south */
  greeble_TILE_ELBOW_NE,
  greeble_TILE_ELBOW_ES,
  greeble_TILE_ELBOW_SW,
  greeble_TILE_ELBOW_WN,
  greeble_TILE_TEE_N,      /* open on all but north */
  greeble_TILE_TEE_E,
  greeble_TILE_TEE_S,
  greeble_TILE_TEE_W,
  greeble_TILE_CROSS,
  greeble_TILE__COUNT
};

/* edge mask per tile, indexed by greeble_TILE_* */
static const unsigned char greeble_edges[greeble_TILE__COUNT] =
{
  0,                                             /* BLANK   */
  0,                                             /* PIP     */
  greeble_N,                                     /* STUB_N  */
  greeble_E,                                     /* STUB_E  */
  greeble_S,                                     /* STUB_S  */
  greeble_W,                                     /* STUB_W  */
  greeble_N | greeble_S,                         /* VERT    */
  greeble_E | greeble_W,                         /* HORIZ   */
  greeble_N | greeble_S,                         /* FRAME_V */
  greeble_E | greeble_W,                         /* FRAME_H */
  greeble_N | greeble_E,                         /* ELBOW_NE */
  greeble_E | greeble_S,                         /* ELBOW_ES */
  greeble_S | greeble_W,                         /* ELBOW_SW */
  greeble_W | greeble_N,                         /* ELBOW_WN */
  greeble_E | greeble_S | greeble_W,             /* TEE_N   */
  greeble_N | greeble_S | greeble_W,             /* TEE_E   */
  greeble_N | greeble_E | greeble_W,             /* TEE_S   */
  greeble_N | greeble_E | greeble_S,             /* TEE_W   */
  greeble_N | greeble_E | greeble_S | greeble_W  /* CROSS   */
};

/* the bag: tile indices with repeats controlling their frequency */
static const unsigned char greeble_bag[] =
{
  greeble_TILE_BLANK, greeble_TILE_BLANK, greeble_TILE_BLANK,
  greeble_TILE_BLANK, greeble_TILE_BLANK, greeble_TILE_BLANK,
  greeble_TILE_PIP,   greeble_TILE_PIP,
  greeble_TILE_STUB_N, greeble_TILE_STUB_E,
  greeble_TILE_STUB_S, greeble_TILE_STUB_W,
  greeble_TILE_VERT,  greeble_TILE_VERT,  greeble_TILE_VERT,
  greeble_TILE_HORIZ, greeble_TILE_HORIZ, greeble_TILE_HORIZ,
  greeble_TILE_FRAME_V, greeble_TILE_FRAME_H,
  greeble_TILE_ELBOW_NE, greeble_TILE_ELBOW_ES,
  greeble_TILE_ELBOW_SW, greeble_TILE_ELBOW_WN,
  greeble_TILE_ELBOW_NE, greeble_TILE_ELBOW_ES,
  greeble_TILE_ELBOW_SW, greeble_TILE_ELBOW_WN,
  greeble_TILE_TEE_N, greeble_TILE_TEE_E,
  greeble_TILE_TEE_S, greeble_TILE_TEE_W,
  greeble_TILE_CROSS
};

/* ----------------------------------------------------------------------- */

/* Greedy edge-matched fill. Scans row-major; at each cell it collects the bag
 * entries whose west edge matches the east edge of the cell to the left and
 * whose north edge matches the south edge of the cell above (cells off the top
 * or left edge count as closed), then picks one uniformly at random. The bag
 * always contains BLANK, whose edges are all closed, so the candidate set is
 * never empty and no backtracking is needed. The outer ring forces its
 * outward-facing edge closed by only considering tiles that keep it so. */
static void greeble_generate(greeble_task_t *task)
{
  unsigned char candidates[NELEMS(greeble_bag)];
  int           r, c, i, n;
  unsigned int  s;

  s = task->seed;

  for (r = 0; r < task->rows; r++)
  {
    for (c = 0; c < task->cols; c++)
    {
      unsigned char need_open;  /* edges that must be open on this tile */
      unsigned char need_closed; /* edges that must be closed on this tile */
      unsigned char west, north;

      need_open   = 0;
      need_closed = 0;

      west  = (c > 0) ? greeble_edges[task->grid[r][c - 1]] : 0;
      north = (r > 0) ? greeble_edges[task->grid[r - 1][c]] : 0;

      if (west & greeble_E) need_open |= greeble_W; else need_closed |= greeble_W;
      if (north & greeble_S) need_open |= greeble_N; else need_closed |= greeble_N;

      /* keep the pattern inside the window: the outer ring never opens outward */
      if (c == 0)                need_closed |= greeble_W;
      if (r == 0)                need_closed |= greeble_N;
      if (c == task->cols - 1)   need_closed |= greeble_E;
      if (r == task->rows - 1)   need_closed |= greeble_S;

      n = 0;
      for (i = 0; i < (int) NELEMS(greeble_bag); i++)
      {
        unsigned char e = greeble_edges[greeble_bag[i]];

        if ((e & need_open) != need_open)   continue;
        if (e & need_closed)               continue;
        candidates[n++] = greeble_bag[i];
      }

      /* need_open can only name W or N, and BLANK satisfies need_closed, so a
       * miss means the neighbour demanded an edge no tile here can supply --
       * fall back to BLANK and let the seam show rather than loop forever */
      if (n == 0)
      {
        task->grid[r][c] = greeble_TILE_BLANK;
        continue;
      }

      /* xorshift32, kept local so the same seed always yields the same grid */
      s ^= s << 13; s ^= s >> 17; s ^= s << 5;
      task->grid[r][c] = candidates[s % (unsigned int) n];
    }
  }
}

/* recompute the live grid extent for the current tile size, then regenerate */
static void greeble_relayout(greeble_task_t *task, const box_t *content)
{
  task->cols = (content->x1 - content->x0) / task->tile;
  task->rows = (content->y1 - content->y0) / task->tile;
  task->cols = CLAMP(task->cols, 1, GREEBLE_MAX_COLS);
  task->rows = CLAMP(task->rows, 1, GREEBLE_MAX_ROWS);
  greeble_generate(task);
}

/* ----------------------------------------------------------------------- */

/* Draw one tile's strokes into the box (ox,oy)..(ox+t,oy+t) in `colour`. Each
 * open edge gets a 2px bar from the tile centre to that edge; a lone dot for
 * PIP; an extra short spur for the FRAME_* variants. Called twice per tile:
 * once shifted by the shadow offset, once at true position. */
static void greeble_stamp(screen_t *scr,
                          int       tile_index,
                          int       ox,
                          int       oy,
                          int       t,
                          colour_t  colour)
{
  unsigned char edges;
  int           mid, half, spur;

  half = GREEBLE_STROKE / 2;
  mid  = t / 2 - half;   /* top-left of a stroke-wide band through the centre */
  spur = t / 2;          /* frame-spur length */

  edges = greeble_edges[tile_index];

  if (tile_index == greeble_TILE_PIP)
  {
    screen_fill_square(scr, ox + mid, oy + mid, GREEBLE_STROKE, colour);
    return;
  }

  if (edges & greeble_N)
    screen_fill_rect(scr, ox + mid, oy, SIZE2D(GREEBLE_STROKE, t / 2), colour);
  if (edges & greeble_S)
    screen_fill_rect(scr, ox + mid, oy + t / 2,
                     SIZE2D(GREEBLE_STROKE, t - t / 2), colour);
  if (edges & greeble_W)
    screen_fill_rect(scr, ox, oy + mid, SIZE2D(t / 2, GREEBLE_STROKE), colour);
  if (edges & greeble_E)
    screen_fill_rect(scr, ox + t / 2, oy + mid,
                     SIZE2D(t - t / 2, GREEBLE_STROKE), colour);

  /* decorated straights: a stub of frame poking off perpendicular */
  if (tile_index == greeble_TILE_FRAME_V)
    screen_fill_rect(scr, ox + t / 2, oy + mid,
                     SIZE2D(spur, GREEBLE_STROKE), colour);
  if (tile_index == greeble_TILE_FRAME_H)
    screen_fill_rect(scr, ox + mid, oy + t / 2,
                     SIZE2D(GREEBLE_STROKE, spur), colour);
}

static result_t greeble_redraw(const wuss_event_t *event,
                               greeble_task_t     *task)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  int          r, c, sx, sy, ox, oy;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content), task->bg);

  for (r = 0; r < task->rows; r++)
  {
    for (c = 0; c < task->cols; c++)
    {
      unsigned char tile = task->grid[r][c];

      if (tile == greeble_TILE_BLANK)
        continue;

      ox = bounds->x0 - sx + c * task->tile;
      oy = bounds->y0 - sy + r * task->tile;

      greeble_stamp(scr, tile, ox + GREEBLE_SHADOW_OFF, oy + GREEBLE_SHADOW_OFF,
                    task->tile, task->shadow);
      greeble_stamp(scr, tile, ox, oy, task->tile, task->fg);
    }
  }

  return result_OK;
}

static result_t greeble_mouse(greeble_task_t *task, wuss_window_t *window)
{
  /* advance to a fresh pattern; any nonzero mix keeps xorshift out of its
   * fixed point */
  task->seed = task->seed * 1664525u + 1013904223u;
  if (task->seed == 0)
    task->seed = 1;

  greeble_generate(task);
  wuss_window_invalidate_all(window);

  return result_OK;
}

static result_t greeble_scroll(greeble_task_t *task,
                               int             delta,
                               wuss_window_t  *window)
{
  int prev;

  prev = task->tile;
  task->tile += delta;
  task->tile  = CLAMP(task->tile, GREEBLE_TILE_MIN, GREEBLE_TILE_MAX);

  if (task->tile != prev)
  {
    /* extent depends on tile size; recompute from the window's content box */
    box_t content;

    wuss_window_get_content_bounds(window, &content);
    greeble_relayout(task, &content);
    wuss_window_invalidate_all(window);
  }

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

  case wuss_EVENT_SCROLL:
    return greeble_scroll(task, event->data.scroll.delta, window);

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

  task->fg     = colour_rgb(0xFF, 0x30, 0x30);
  task->shadow = colour_rgb(0x60, 0x00, 0x00);
  task->bg     = colour_rgb(0x18, 0x18, 0x20);
  task->seed   = 0x9E3779B9u; /* any nonzero start */
  task->tile   = GREEBLE_TILE_DEFAULT;

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
                                 SIZE2D(180, 320),
                                 "Greeble",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 SIZE2D(180, 320),
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
