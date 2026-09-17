/* wuss/test/tasks/curve.c -- draggable Bezier curve task */

#ifdef WUSS_APP

#include <stdlib.h>

#include <stddef.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include <string.h>

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "framebuf/curve.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "utils/fxp.h"
#include "wuss/wuss.h"

#include "curve.h"

/* MENU click pops this single-item menu; the item table and wuss_menu_t
 * live per-instance in curve_task_t, not as a file-scope static, so that
 * each window's Info row points at its own proginfo rather than every
 * instance sharing (and overwriting) one global .window pointer */
enum { CURVE_MENU_INFO };

#define CURVE_BLOBSZ           8  /* side length of a control-point marker, matches curve-test.c */
#define CURVE_SEGMENTS_DEFAULT 32
#define CURVE_SEGMENTS_MIN     4
#define CURVE_SEGMENTS_MAX     128

/* 2D cross product of (b - a) and (c - a); > 0 for a left (counter-clockwise)
 * turn, < 0 for a right turn, 0 if collinear. long to keep the products of
 * two point coordinates from overflowing an int. */
static long curve_cross(point_t a, point_t b, point_t c)
{
  return (long) (b.x - a.x) * (c.y - a.y) - (long) (b.y - a.y) * (c.x - a.x);
}

static int curve_point_cmp(const void *va, const void *vb)
{
  const point_t *a = va;
  const point_t *b = vb;

  if (a->x != b->x)
    return a->x - b->x;
  return a->y - b->y;
}

/* Andrew's monotone chain: fill "hull" (capacity 2 * n + 1) with the convex
 * hull of the first n of "src" as a closed polyline (first point repeated at
 * the end) and return its point count. n < 3, or all points collinear,
 * degenerates to the sorted span, still returned closed. */
static int curve_convex_hull(const point_t *src, int n, point_t *hull)
{
  point_t pts[CURVE_MAXCONTROLPTS];
  int     k, i;

  memcpy(pts, src, (size_t) n * sizeof(*pts));
  qsort(pts, (size_t) n, sizeof(*pts), curve_point_cmp);

  k = 0;
  for (i = 0; i < n; i++) /* lower hull */
  {
    while (k >= 2 && curve_cross(hull[k - 2], hull[k - 1], pts[i]) <= 0)
      k--;
    hull[k++] = pts[i];
  }
  {
    int lower, j;

    lower = k + 1;
    for (j = n - 2; j >= 0; j--) /* upper hull */
    {
      while (k >= lower && curve_cross(hull[k - 2], hull[k - 1], pts[j]) <= 0)
        k--;
      hull[k++] = pts[j];
    }
  }

  return k; /* hull[0] == hull[k - 1], a closed loop */
}

result_t curve_create(wuss_t *wuss, curve_task_t **out)
{
  result_t         rc;
  curve_task_t    *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->bg        = colour_rgb(0xFF, 0xFF, 0xFF);
  task->line      = colour_rgb(0x00, 0x00, 0x00);
  task->blob      = colour_rgb(0xFF, 0x00, 0x00);
  task->wuss      = wuss;
  task->nsegments = CURVE_SEGMENTS_DEFAULT;
  task->npoints   = 4; /* cubic, matching the original task */
  task->dragging  = -1;

  task->points[0] = POINT(10,  10);
  task->points[1] = POINT(10, 140);
  task->points[2] = POINT(210, 10);
  task->points[3] = POINT(210, 140);
  task->points[4] = POINT(110,  10);
  task->points[5] = POINT(110, 140);

  /* curve_redraw paints its own background */
  delegate_desc.handle    = curve_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "curve";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(220, 160),
                                 "Curve",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(220, 160),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  {
    static const wuss_proginfo_desc_t desc =
    {
      "Curve",
      "Draggable Bezier curve",
      "(c) DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };
    if (wuss_proginfo_create(&task->proginfo, delegate, &desc) != result_OK)
      task->proginfo = NULL;
  }
  WUSS_MENU_ITEM_WINDOW(task->menu_items, CURVE_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        wuss_proginfo_window(task->proginfo));

  WUSS_MENU_TITLE(task->menu, "Curve", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void curve_destroy(curve_task_t *task)
{
  if (task->menu_handle != NULL)
    wuss_menu_close(task->menu_handle);
  wuss_proginfo_destroy(task->proginfo);
  free(task);
}

static int blob_hit(const point_t *p, int x, int y)
{
  int half = CURVE_BLOBSZ / 2;

  return x >= p->x - half && x < p->x + half &&
         y >= p->y - half && y < p->y + half;
}

/* Marker colour for control point i of a curve with npoints points: the two
 * end points (0 and npoints - 1) draw in task->blob (red); the interior
 * control points cycle through a set of other bright hues so each is
 * distinct. */
static colour_t blob_colour(const curve_task_t *task, int i)
{
  colour_t control[4];

  if (i == 0 || i == task->npoints - 1)
    return task->blob;

  control[0] = colour_rgb(0x00, 0xC0, 0x00); /* green  */
  control[1] = colour_rgb(0x00, 0x80, 0xFF); /* blue   */
  control[2] = colour_rgb(0xFF, 0xA0, 0x00); /* orange */
  control[3] = colour_rgb(0xC0, 0x00, 0xFF); /* purple */

  return control[(i - 1) % NELEMS(control)];
}

/* Curve-type name for each valid task->npoints, indexed by
 * npoints - CURVE_MINCONTROLPTS. */
static const char *curve_kind_name(int npoints)
{
  static const char *const names[] =
  {
    "Line", "Quadratic", "Cubic", "Quartic", "Quintic"
  };

  return names[npoints - CURVE_MINCONTROLPTS];
}

/* The point at time t on the curve through the first task->npoints points,
 * dispatching on the count: 2 is a straight line, 3..6 the quadratic through
 * quintic Beziers. */
static point_t curve_point(const curve_task_t *task, fix16_t t)
{
  const point_t *p = task->points;

  switch (task->npoints)
  {
  case 2:  return curve_point_on_line(p[0], p[1], t);
  case 3:  return curve_bezier_point_on_quad(p[0], p[1], p[2], t);
  case 4:  return curve_bezier_point_on_cubic(p[0], p[1], p[2], p[3], t);
  case 5:  return curve_bezier_point_on_quartic(p[0], p[1], p[2], p[3],
                                                p[4], t);
  default: return curve_bezier_point_on_quintic(p[0], p[1], p[2], p[3],
                                                p[4], p[5], t);
  }
}

static result_t curve_redraw(const wuss_event_t *event, curve_task_t *task)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  point_t      prev, cur;
  int          i, half, sx, sy;
  fix16_t      t;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content),
                   task->bg);

  /* control polygon's convex hull, drawn first so the curve and the blobs
   * sit on top of it */
  {
    point_t hull[2 * CURVE_MAXCONTROLPTS + 1];
    int     nhull, k;

    nhull = curve_convex_hull(task->points, task->npoints, hull);
    for (k = 0; k < nhull; k++)
    {
      hull[k].x += bounds->x0 - sx;
      hull[k].y += bounds->y0 - sy;
    }
    screen_draw_lines(scr, hull, nhull, colour_rgb(0xC0, 0xC0, 0xC0));
  }

  prev = task->points[0];
  prev.x += bounds->x0 - sx; prev.y += bounds->y0 - sy;

  for (i = 1; i <= task->nsegments; i++)
  {
    t   = i * FIX16_ONE / task->nsegments;
    cur = curve_point(task, t);
    cur.x += bounds->x0 - sx; cur.y += bounds->y0 - sy;

    screen_draw_line(scr, prev.x, prev.y, cur.x, cur.y, task->line);

    prev = cur;
  }

  half = CURVE_BLOBSZ / 2;
  for (i = 0; i < task->npoints; i++)
  {
    cur = task->points[i];
    cur.x += bounds->x0 - sx; cur.y += bounds->y0 - sy;
    screen_fill_square(scr, cur.x - half, cur.y - half, CURVE_BLOBSZ,
                       blob_colour(task, i));
  }

  /* curve-type label, anchored at document (2, 2) so it scrolls with the
   * content: screen pos = content top-left - scroll + doc offset. Uses bounds,
   * not the per-redraw dirty piece "content", so it is drawn whole on a
   * partial redraw */
  {
    bmfont_t   *font = wuss_get_font(task->wuss);
    const char *name = curve_kind_name(task->npoints);

    if (font != NULL)
    {
      int     ascent;
      point_t pos;

      bmfont_get_info(font, NULL, NULL, &ascent, NULL);
      pos = POINT(bounds->x0 - sx + 2, bounds->y0 - sy + 2 + ascent);
      wuss_text_draw(task->wuss, 0, scr, name, (int) strlen(name),
                     task->line, task->bg, &pos, NULL);
    }
  }

  return result_OK;
}

static result_t curve_mouse(curve_task_t       *task,
                            wuss_mouse_action_t action,
                            int                 x,
                            int                 y,
                            wuss_button_t       button,
                            wuss_window_t      *window)
{
  int i;

  /* x,y already arrive in virtual content space: wuss_mouse_click/move add the
   * window's scroll offset before delivering the event. */

  switch (action)
  {
  case wuss_MOUSE_DOWN:
    if (button & wuss_BUTTON_MENU)
      return wuss_menu_open(task->delegate, &task->menu,
                            wuss_get_pointer(task->wuss), &task->menu_handle);
    if (button & wuss_BUTTON_ADJUST)
    {
      /* cycle line -> quad -> cubic -> quartic -> quintic -> line */
      task->npoints++;
      if (task->npoints > CURVE_MAXCONTROLPTS)
        task->npoints = CURVE_MINCONTROLPTS;
      wuss_window_invalidate_visible(window);
      break;
    }
    if (!(button & wuss_BUTTON_SELECT))
      break;
    for (i = 0; i < task->npoints; i++)
    {
      if (blob_hit(&task->points[i], x, y))
      {
        task->dragging = i;
        break;
      }
    }
    break;

  case wuss_MOUSE_MOVE:
    if (task->dragging < 0)
      break;
    task->points[task->dragging].x = x;
    task->points[task->dragging].y = y;
    wuss_window_invalidate_visible(window);
    break;

  case wuss_MOUSE_UP:
    task->dragging = -1;
    break;
  }

  return result_OK;
}

static result_t curve_scroll(curve_task_t  *task,
                             int            delta,
                             wuss_window_t *window)
{
  task->nsegments += delta;
  task->nsegments  = CLAMP(task->nsegments, CURVE_SEGMENTS_MIN, CURVE_SEGMENTS_MAX);

  wuss_window_invalidate_visible(window);

  return result_OK;
}

result_t curve_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  curve_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return curve_redraw(event, task);

  case wuss_EVENT_MOUSE:
    if (window != task->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    return curve_mouse(task, event->data.mouse.action,
                       event->data.mouse.point.x, event->data.mouse.point.y,
                       event->data.mouse.button, window);

  case wuss_EVENT_SCROLL:
    return curve_scroll(task, event->data.scroll.delta, window);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == wuss_proginfo_window(task->proginfo))
      rc = wuss_proginfo_handle_pre_show(task->proginfo);
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
    curve_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
