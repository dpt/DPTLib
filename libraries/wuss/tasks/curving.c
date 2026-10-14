/* wuss/tasks/curving.c -- draggable Bezier curve task */

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

#include "curving.h"
#include "taskapp.h"
#include "common.h"
#include "snapshot.h"

/* MENU click pops this menu; the item table and wuss_menu_t live per-instance
 * in curving_task_t, not as a file-scope static, so that each window's Info row
 * can hold its own .window pointer to the shared proginfo singleton, retargeted
 * just before wuss_menu_open */
enum
{
  CURVING_MENU_BACKGROUND = 0,
  CURVING_MENU_TYPE,
  CURVING_MENU_WIDTH,
  CURVING_MENU_HULL,
  CURVING_MENU_RESET,
  CURVING_MENU_SAVE
};

#define CURVING_SAVE_NAME "curving.png" /* Save As's initial leafname */

/* Curve-type name for each valid task->npoints, indexed by
 * npoints - CURVING_MINCONTROLPTS; also the Menu > Type rows. */
static const char *const curving_kind_names[CURVING_NKINDS] =
{
  "Line", "Quadratic", "Cubic", "Quartic", "Quintic"
};

/* Line width in pixels for each Menu > Width row. */
static const int curving_widths[CURVING_NWIDTHS] = { 1, 2, 3, 4, 6, 8 };

static const char *const curving_width_names[CURVING_NWIDTHS] =
{
  "1 px", "2 px", "3 px", "4 px", "6 px", "8 px"
};

#define CURVING_BLOBSZ           8  /* side length of a control-point marker, matches curve-test.c */
#define CURVING_SEGMENTS_DEFAULT 32
#define CURVING_SEGMENTS_MIN     4
#define CURVING_SEGMENTS_MAX     128

/* 2D cross product of (b - a) and (c - a); > 0 for a left (counter-clockwise)
 * turn, < 0 for a right turn, 0 if collinear. long to keep the products of
 * two point coordinates from overflowing an int. */
static long curving_cross(point_t a, point_t b, point_t c)
{
  return (long) (b.x - a.x) * (c.y - a.y) - (long) (b.y - a.y) * (c.x - a.x);
}

static int curving_point_cmp(const void *va, const void *vb)
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
static int curving_convex_hull(const point_t *src, int n, point_t *hull)
{
  point_t pts[CURVING_MAXCONTROLPTS];
  int     k, i;

  memcpy(pts, src, (size_t) n * sizeof(*pts));
  qsort(pts, (size_t) n, sizeof(*pts), curving_point_cmp);

  k = 0;
  for (i = 0; i < n; i++) /* lower hull */
  {
    while (k >= 2 && curving_cross(hull[k - 2], hull[k - 1], pts[i]) <= 0)
      k--;
    hull[k++] = pts[i];
  }
  {
    int lower, j;

    lower = k + 1;
    for (j = n - 2; j >= 0; j--) /* upper hull */
    {
      while (k >= lower && curving_cross(hull[k - 2], hull[k - 1], pts[j]) <= 0)
        k--;
      hull[k++] = pts[j];
    }
  }

  return k; /* hull[0] == hull[k - 1], a closed loop */
}

/* put every control point back at its starting position */
static void curving_reset_points(curving_task_t *task)
{
  task->points[0] = POINT(10,  10);
  task->points[1] = POINT(10, 140);
  task->points[2] = POINT(210, 10);
  task->points[3] = POINT(210, 140);
  task->points[4] = POINT(110,  10);
  task->points[5] = POINT(110, 140);
}

/* wuss_saveas_save_fn_t: opaque is the curving_task_t */
static result_t curving_saveas_save(const char *path, void *opaque)
{
  curving_task_t *task;

  task = opaque;

  return snapshot_save_png(task->window, curving_handle, task, path);
}

result_t curving_create(wuss_t *wuss, curving_task_t **out)
{
  result_t         rc;
  curving_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->bg        = colour_rgb(0xFF, 0xFF, 0xFF);
  task->line      = colour_rgb(0x00, 0x00, 0x00);
  task->blob      = colour_rgb(0x66, 0xCC, 0xFF); /* light blue */
  task->wuss      = wuss;
  task->nsegments = CURVING_SEGMENTS_DEFAULT;
  task->width     = curving_widths[0];
  task->npoints   = 4; /* cubic, matching the original task */
  task->dragging  = -1;
  task->hull      = 0;
  curving_reset_points(task);

  /* curving_redraw paints its own background */
  delegate_desc.handle    = curving_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "curving";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate, SIZE2D(220, 160), "Curving", &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  rc = snapshot_saveas_create(&task->saveas, wuss, CURVING_SAVE_NAME,
                              curving_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  WUSS_MENU_ITEM_MENU(task->menu_items, CURVING_MENU_BACKGROUND, "Background",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  for (i = 0; i < CURVING_NKINDS; i++)
    WUSS_MENU_ITEM(task->type_items, i, curving_kind_names[i],
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->type_menu, "Type", task->type_items,
                 NELEMS(task->type_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, CURVING_MENU_TYPE, "Type",
                      wuss_MENU_ITEM_NONE, &task->type_menu);

  for (i = 0; i < CURVING_NWIDTHS; i++)
    WUSS_MENU_ITEM(task->width_items, i, curving_width_names[i],
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->width_menu, "Width", task->width_items,
                  NELEMS(task->width_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, CURVING_MENU_WIDTH, "Width",
                      wuss_MENU_ITEM_NONE, &task->width_menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CURVING_MENU_HULL, "Hull",
                          wuss_MENU_ITEM_NONE, "H");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CURVING_MENU_RESET, "Reset points",
                          wuss_MENU_ITEM_NONE, "R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CURVING_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");
  /* hover opens the Save As dialogue as a submenu; ^S shows it standalone */
  task->menu_items[CURVING_MENU_SAVE].window = wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Curving", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

result_t curving_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Curving",
    "Draggable Bezier curve",
    (taskapp_create_fn_t) curving_create
  };

  return taskapp_install(wuss, &desc);
}

void curving_destroy(curving_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task);
}

static int blob_hit(const point_t *p, int x, int y)
{
  int half = CURVING_BLOBSZ / 2;

  return x >= p->x - half && x < p->x + half &&
         y >= p->y - half && y < p->y + half;
}

/* Marker colour for point i of a curve with npoints points: the two end
 * points (0 and npoints - 1) draw in task->blob (light blue); the first and
 * last control points (1 and npoints - 2) draw orange; any remaining interior
 * control points each get their own hue from a set of four, which covers the
 * most a quintic has (two). */
static colour_t blob_colour(const curving_task_t *task, int i)
{
  colour_t control[4];

  if (i == 0 || i == task->npoints - 1)
    return task->blob;

  if (i == 1 || i == task->npoints - 2)
    return colour_rgb(0xFF, 0xA0, 0x00); /* orange */

  control[0] = colour_rgb(0x00, 0xC0, 0x00); /* green  */
  control[1] = colour_rgb(0xC0, 0x00, 0xFF); /* purple */
  control[2] = colour_rgb(0xFF, 0xE0, 0x00); /* yellow */
  control[3] = colour_rgb(0xFF, 0x60, 0xB0); /* pink   */

  return control[(i - 2) % NELEMS(control)];
}

/* index of task->width in curving_widths, for the Menu > Width tick */
static int curving_width_index(const curving_task_t *task)
{
  int i;

  for (i = 0; i < CURVING_NWIDTHS - 1; i++)
    if (curving_widths[i] >= task->width)
      break;

  return i;
}

static const char *curving_kind_name(int npoints)
{
  return curving_kind_names[npoints - CURVING_MINCONTROLPTS];
}

/* The point at time t on the curve through the first task->npoints points,
 * dispatching on the count: 2 is a straight line, 3..6 the quadratic through
 * quintic Beziers. */
static point_t curving_point(const curving_task_t *task, fix16_t t)
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

static result_t curving_redraw(const wuss_event_t *event,
                               curving_task_t     *task)
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
  if (task->hull)
  {
    point_t hull[2 * CURVING_MAXCONTROLPTS + 1];
    int     nhull, k;

    nhull = curving_convex_hull(task->points, task->npoints, hull);
    for (k = 0; k < nhull; k++)
    {
      hull[k].x += bounds->x0 - sx;
      hull[k].y += bounds->y0 - sy;
    }
    screen_draw_lines(scr, hull, nhull, colour_rgb(0xFF, 0x00, 0x00));
  }

  /* tangent lines: each end point to its adjacent control point, faint so
   * they sit behind the curve. A straight line has no control points. */
  if (task->npoints > 2)
  {
    point_t tangent[2];
    int     e;

    for (e = 0; e < 2; e++)
    {
      tangent[0] = task->points[e ? task->npoints - 1 : 0];
      tangent[1] = task->points[e ? task->npoints - 2 : 1];
      for (i = 0; i < 2; i++)
      {
        tangent[i].x += bounds->x0 - sx;
        tangent[i].y += bounds->y0 - sy;
      }
      screen_draw_line(scr, tangent[0].x, tangent[0].y,
                       tangent[1].x, tangent[1].y,
                       colour_rgb(0xC0, 0xC0, 0xC0));
    }
  }

  prev = task->points[0];
  prev.x += bounds->x0 - sx; prev.y += bounds->y0 - sy;

  for (i = 1; i <= task->nsegments; i++)
  {
    t   = i * FIX16_ONE / task->nsegments;
    cur = curving_point(task, t);
    cur.x += bounds->x0 - sx; cur.y += bounds->y0 - sy;

    screen_draw_thick_line(scr, prev.x, prev.y, cur.x, cur.y, task->width,
                           task->line);
    /* round the joint: butt-ended segments leave a notch on the outside of
     * each bend */
    if (task->width > 2)
      screen_fill_circle(scr, cur.x, cur.y, task->width / 2, task->line);

    prev = cur;
  }

  half = CURVING_BLOBSZ / 2;
  for (i = 0; i < task->npoints; i++)
  {
    cur = task->points[i];
    cur.x += bounds->x0 - sx; cur.y += bounds->y0 - sy;
    screen_fill_square(scr, cur.x - half, cur.y - half, CURVING_BLOBSZ,
                       blob_colour(task, i));
  }

  /* curve-type label, anchored at document (2, 2) so it scrolls with the
   * content: screen pos = content top-left - scroll + doc offset. Uses bounds,
   * not the per-redraw dirty piece "content", so it is drawn whole on a
   * partial redraw */
  {
    bmfont_t   *font = wuss_get_font(task->wuss);
    const char *name = curving_kind_name(task->npoints);

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

static result_t curving_mouse(curving_task_t     *task,
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
    {
      wuss_menu_tick_exclusive(&task->type_menu,
                               task->npoints - CURVING_MINCONTROLPTS);
      wuss_menu_tick_exclusive(&task->width_menu, curving_width_index(task));
      wuss_menu_tick_item(&task->menu, CURVING_MENU_HULL, task->hull);

      return wuss_menu_open_at_pointer(task->delegate, &task->menu,
                                       &task->menu_handle);
    }
    if (button & wuss_BUTTON_ADJUST)
    {
      /* cycle line -> quad -> cubic -> quartic -> quintic -> line */
      task->npoints++;
      if (task->npoints > CURVING_MAXCONTROLPTS)
        task->npoints = CURVING_MINCONTROLPTS;
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

static result_t curving_scroll(curving_task_t *task,
                               int             delta,
                               wuss_window_t  *window)
{
  task->nsegments += delta;
  task->nsegments  = CLAMP(task->nsegments, CURVING_SEGMENTS_MIN, CURVING_SEGMENTS_MAX);

  wuss_window_invalidate_visible(window);

  return result_OK;
}

/* Left/Right step the curve type back/forward; Up/Down add/remove a
 * segment, as the wheel does. Anything else is passed back unclaimed. */
static result_t curving_key(curving_task_t *task,
                            wuss_window_t  *window,
                            int             code)
{
  switch (code)
  {
  case wuss_KEY_UP:   return curving_scroll(task, +1, window);
  case wuss_KEY_DOWN: return curving_scroll(task, -1, window);

  case wuss_KEY_LEFT:
    task->npoints = (task->npoints - CURVING_MINCONTROLPTS + CURVING_NKINDS - 1) %
                    CURVING_NKINDS + CURVING_MINCONTROLPTS;
    break;

  case wuss_KEY_RIGHT:
    task->npoints = (task->npoints - CURVING_MINCONTROLPTS + 1) %
                    CURVING_NKINDS + CURVING_MINCONTROLPTS;
    break;

  default:
    return result_WUSS_KEY_UNCLAIMED;
  }

  wuss_window_invalidate_visible(window);

  return result_OK;
}

static result_t curving_menu_select(curving_task_t     *task,
                                    const wuss_event_t *event)
{
  if (event->data.menu_select.menu == &task->menu &&
      event->data.menu_select.index == CURVING_MENU_HULL)
  {
    task->hull = !task->hull;
    wuss_menu_tick_item_live(task->menu_handle, &task->menu,
                             CURVING_MENU_HULL, task->hull);
    wuss_window_invalidate_visible(task->window);
    return result_OK;
  }

  if (event->data.menu_select.menu == &task->menu &&
      event->data.menu_select.index == CURVING_MENU_RESET)
  {
    curving_reset_points(task);
    wuss_window_invalidate_visible(task->window);
    return result_OK;
  }

  if (event->data.menu_select.menu == &task->menu &&
      event->data.menu_select.index == CURVING_MENU_SAVE)
  {
    return wuss_saveas_open(task->saveas);
  }

  if (event->data.menu_select.menu == &task->type_menu)
  {
    task->npoints = CURVING_MINCONTROLPTS + event->data.menu_select.index;
    wuss_menu_tick_exclusive_live(task->menu_handle, &task->type_menu,
                                  event->data.menu_select.index);
    wuss_window_invalidate_visible(task->window);
    return result_OK;
  }

  if (event->data.menu_select.menu == &task->width_menu)
  {
    task->width = curving_widths[event->data.menu_select.index];
    wuss_menu_tick_exclusive_live(task->menu_handle, &task->width_menu,
                                  event->data.menu_select.index);
    wuss_window_invalidate_visible(task->window);
    return result_OK;
  }

  if (wuss_colourmenu_selected_rgb(event, &task->bg))
    wuss_window_invalidate_visible(task->window);

  return result_OK;
}

result_t curving_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  curving_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return curving_redraw(event, task);

  case wuss_EVENT_MOUSE:
    if (window != task->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    return curving_mouse(task, event->data.mouse.action,
                       event->data.mouse.point.x, event->data.mouse.point.y,
                       event->data.mouse.button, window);

  case wuss_EVENT_SCROLL:
    return curving_scroll(task, event->data.scroll.delta, window);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != task->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(task->delegate, &task->menu, event, &rc))
      return rc;

    return curving_key(task, window, event->data.key.code);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    /* the "Background" row: the shared colourmenu, set up per open */
    return wuss_colourmenu_open_rgb(task->wuss, event, "Background", task->bg);

  case wuss_EVENT_MENU_SELECT:
    return curving_menu_select(task, event);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    curving_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
