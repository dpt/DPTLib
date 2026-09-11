/* wuss/test/tasks/gradient.c -- gradient fill task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include <stdio.h>
#include <string.h>

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "geom/box.h"

#include "gradient.h"

#define GRADIENT_DOC_WIDTH  400
#define GRADIENT_DOC_HEIGHT 400
#define GRADIENT_OPEN_WIDTH  100
#define GRADIENT_OPEN_HEIGHT 100

/* ordered (Bayer) dither matrices, each holding values 0 .. dim*dim-1.
 * SELECT/ADJUST clicks cycle task->dither_index forward/backward through
 * this table. */
static const struct
{
  int dim;
  int cell[64]; /* row-major, first dim*dim entries used */
}
gradient_dithers[] =
{
  {
    2,
    {
      0, 2,
      3, 1,
    }
  },
  {
    4,
    {
       0,  8,  2, 10,
      12,  4, 14,  6,
       3, 11,  1,  9,
      15,  7, 13,  5,
    }
  },
  {
    8,
    {
       0, 32,  8, 40,  2, 34, 10, 42,
      48, 16, 56, 24, 50, 18, 58, 26,
      12, 44,  4, 36, 14, 46,  6, 38,
      60, 28, 52, 20, 62, 30, 54, 22,
       3, 35, 11, 43,  1, 33,  9, 41,
      51, 19, 59, 27, 49, 17, 57, 25,
      15, 47,  7, 39, 13, 45,  5, 37,
      63, 31, 55, 23, 61, 29, 53, 21,
    }
  },
};

/* offset "v" by the dither cell for (x, y), mapped to a fixed -8..+8 swing
 * (matching the original 4x4 code) whatever the matrix size, so a larger
 * matrix just gives a finer pattern rather than a louder one */
static int dither(int index, int v, int x, int y)
{
  int dim, n, m;

  dim = gradient_dithers[index].dim;
  n   = dim * dim;
  m   = gradient_dithers[index].cell[(y % dim) * dim + (x % dim)];

  return CLAMP(v + (m * 16 / (n - 1)) - 8, 0, 255);
}

result_t gradient_create(wuss_t *wuss, gradient_task_t *task)
{
  result_t         rc;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task->wuss         = wuss;
  task->dither_index = 1; /* 4x4, matching the original */

  /* gradient_redraw paints every pixel itself */
  delegate_desc.handle    = gradient_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "gradient";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(GRADIENT_OPEN_WIDTH, GRADIENT_OPEN_HEIGHT),
                                 "Gradient",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(GRADIENT_DOC_WIDTH, GRADIENT_DOC_HEIGHT),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */

  return rc;
}

static result_t gradient_redraw(const wuss_event_t *event, void *task_data)
{
  gradient_task_t *gc;
  screen_t        *scr;
  const box_t     *content, *bounds;
  int              di, sx, sy, x, y, lx, ly;

  gc = task_data;
  di = gc->dither_index;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = content->x0; x < content->x1; x++)
    {
      lx = x - bounds->x0 + sx;
      ly = y - bounds->y0 + sy;

      screen_set_pixel(scr, x, y,
                        colour_rgb(dither(di, lx * 255 / GRADIENT_DOC_WIDTH, lx, ly),
                                   dither(di, ly * 255 / GRADIENT_DOC_HEIGHT, lx, ly),
                                   dither(di, 255 - (lx + ly) * 255 / (GRADIENT_DOC_WIDTH + GRADIENT_DOC_HEIGHT), lx, ly)));
    }
  }

  /* matrix-size label, anchored at document (2, 2) so it scrolls with the
   * content: screen pos = content top-left - scroll + doc offset. Uses bounds,
   * not the per-redraw dirty piece, so it is drawn whole on a partial redraw */
  {
    bmfont_t   *font = wuss_get_font(gc->wuss);
    int         dim  = gradient_dithers[di].dim;
    char        label[8];
    colour_t    ink  = colour_rgb(0xFF, 0xFF, 0xFF);
    colour_t    bg   = colour_rgba(0, 0, 0, 0); /* transparent */

    sprintf(label, "%dx%d", dim, dim);

    if (font != NULL)
    {
      int     ascent;
      point_t pos;

      bmfont_get_info(font, NULL, NULL, &ascent, NULL);
      pos = POINT(bounds->x0 - sx + 2, bounds->y0 - sy + 2 + ascent);
      wuss_text_draw(gc->wuss, 0, scr, label, (int) strlen(label), ink, bg,
                     &pos, NULL);
    }
  }

  return result_OK;
}

static result_t gradient_mouse(const wuss_event_t *event, void *task_data)
{
  gradient_task_t *gc;
  wuss_button_t    button;
  int              n;

  gc = task_data;

  if (event->data.mouse.action != wuss_MOUSE_DOWN)
    return result_OK;

  button = event->data.mouse.button;
  n      = (int) NELEMS(gradient_dithers);

  if (button & wuss_BUTTON_SELECT)
    gc->dither_index = (gc->dither_index + 1) % n;
  else if (button & wuss_BUTTON_ADJUST)
    gc->dither_index = (gc->dither_index + n - 1) % n;
  else
    return result_OK;

  wuss_window_invalidate_visible(gc->window); /* whole fill changes */

  return result_OK;
}

result_t gradient_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  gradient_task_t *gc;

  NOT_USED(window);

  gc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return gradient_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return gradient_mouse(event, task_data);

  case wuss_EVENT_QUIT:
    free(gc); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
