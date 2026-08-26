/* sofa.c -- wuss test - rotating wireframe sofa and spaceship task */

#ifdef USE_SDL

#include <math.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "utils/fxp.h"

#include "sofa.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SOFA_TILT           -0.5 /* static camera tilt, radians, so the seat is visible from above */
#define SOFA_SPIN_PER_FRAME  0.02 /* radians/frame at 60fps, one turn every ~5s */
#define SOFA_CAMERA_DIST     4.0  /* perspective divisor: bigger = flatter */
#define SOFA_UNIT_FRACTION   0.35 /* fraction of min(width,height) per model unit, at zoom 1.0 */
#define SOFA_ZOOM_MIN        0.2
#define SOFA_ZOOM_MAX        4.0
#define SOFA_ZOOM_PER_NOTCH  0.1

/* one 3D point */
typedef struct vec3 { double x, y, z; } vec3_t;

/* an axis-aligned box in model space, given as opposite corners */
typedef struct box3 { double x0, y0, z0, x1, y1, z1; } box3_t;

/* the sofa: seat, backrest and two arms, in model units (roughly -1..1) */
static const box3_t sofa_parts[] =
{
  { -0.8, -0.3, -0.6,   0.8,  0.1,  0.6 }, /* seat */
  { -1.0, -0.3,  0.35,  1.0,  0.9,  0.6 }, /* backrest */
  { -1.0, -0.3, -0.6,  -0.8,  0.5,  0.6 }, /* left arm */
  {  0.8, -0.3, -0.6,   1.0,  0.5,  0.6 }, /* right arm */
};

/* cube corner edges, indexing box3_corners' bit-numbered corners (bit0=x,
 * bit1=y, bit2=z; each pair differs in exactly one bit) */
static const int cube_edges[12][2] =
{
  { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },
  { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
  { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
};

/* a simplified wireframe spaceship: diamond fuselage plus
 * delta wings, model units roughly -1.4..1.4 */
static const vec3_t ship_vertices[10] =
{
  {  0.0,  0.0,  1.3 }, /* 0 nose */
  {  0.0,  0.0, -1.0 }, /* 1 tail */
  {  0.0,  0.35, 0.1 }, /* 2 top */
  {  0.0, -0.35, 0.1 }, /* 3 bottom */
  { -0.5,  0.0,  0.1 }, /* 4 left */
  {  0.5,  0.0,  0.1 }, /* 5 right */
  { -1.4,  0.0, -0.5 }, /* 6 left wingtip */
  { -0.5,  0.0, -0.4 }, /* 7 left wing root rear */
  {  1.4,  0.0, -0.5 }, /* 8 right wingtip */
  {  0.5,  0.0, -0.4 }, /* 9 right wing root rear */
};

static const int ship_edges[18][2] =
{
  { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, /* nose to ring */
  { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 }, /* tail to ring */
  { 2, 4 }, { 4, 3 }, { 3, 5 }, { 5, 2 }, /* ring */
  { 4, 6 }, { 6, 7 }, { 7, 4 },           /* left wing */
  { 5, 8 }, { 8, 9 }, { 9, 5 },           /* right wing */
};

static void box3_corners(const box3_t *box, vec3_t out[8])
{
  int i;

  for (i = 0; i < 8; i++)
  {
    out[i].x = (i & 1) ? box->x1 : box->x0;
    out[i].y = (i & 2) ? box->y1 : box->y0;
    out[i].z = (i & 4) ? box->z1 : box->z0;
  }
}

static vec3_t rotate_xy(vec3_t v, double tilt, double spin)
{
  vec3_t r;
  double cs, sn, ct, st;

  /* spin about Y (vertical), then tilt about X (camera pitch) */
  cs = cos(spin); sn = sin(spin);
  r.x = v.x * cs + v.z * sn;
  r.y = v.y;
  r.z = -v.x * sn + v.z * cs;

  ct = cos(tilt); st = sin(tilt);
  v.y = r.y * ct - r.z * st;
  v.z = r.y * st + r.z * ct;
  r.y = v.y;

  return r;
}

/* a screen-space point in fix8_t, for screen_draw_line_wu_fix8 */
typedef struct fix8_point { fix8_t x, y; } fix8_point_t;

static fix8_point_t project(vec3_t v, int cx, int cy, double unit)
{
  fix8_point_t p;
  double       scale;

  scale = SOFA_CAMERA_DIST / (SOFA_CAMERA_DIST + v.z);
  p.x   = FLOAT_TO_FIX8(cx + v.x * unit * scale);
  p.y   = FLOAT_TO_FIX8(cy - v.y * unit * scale);

  return p;
}

result_t sofa_create(wuss_t *wuss, const colour_t *palette, sofa_task_t *task)
{
  wuss_task_t delegate;
  box_t       box;

  task->bg       = palette[palette_PICO8_DARK_PURPLE];
  task->line     = palette[palette_PICO8_ORANGE];
  task->angle    = 0.0;
  task->zoom     = 1.0;
  task->spinning = true;
  task->shape    = sofa_SHAPE_SOFA;

  delegate = wuss_task_start(sofa_handle, task, wuss_NO_BACKGROUND); /* sofa_redraw paints its own background every frame */
  box      = (box_t) BOX_POS_SIZE(250, 260, 180, 160);

  return wuss_window_create(wuss,
                            &box,
                            "Sofa",
                            wuss_WINDOW_NONE,
                            &delegate,
                            box.x1 - box.x0,
                            box.y1 - box.y0,
                            &task->window);
}

void sofa_destroy(sofa_task_t *task)
{
  if (task->window != NULL)
    wuss_window_destroy(task->window);
}

static result_t sofa_redraw(wuss_window_t *window,
                            screen_t      *scr,
                            const box_t   *content,
                            void          *task_data)
{
  sofa_task_t *sc;
  box_t        bounds;
  int          cx, cy;
  double       unit;
  size_t       part;

  NOT_USED(window);

  sc = task_data;

  screen_draw_rect(scr,
                   content->x0,
                   content->y0,
                   content->x1 - content->x0,
                   content->y1 - content->y0,
                   sc->bg);

  wuss_window_get_content_bounds(sc->window, &bounds);
  cx   = content->x0 + (bounds.x1 - bounds.x0) / 2;
  cy   = content->y0 + (bounds.y1 - bounds.y0) / 2;
  unit = MIN(bounds.x1 - bounds.x0, bounds.y1 - bounds.y0) * SOFA_UNIT_FRACTION * sc->zoom;

  if (sc->shape == sofa_SHAPE_SOFA)
  {
    for (part = 0; part < NELEMS(sofa_parts); part++)
    {
      vec3_t       corners[8];
      fix8_point_t screen[8];
      int          i;

      box3_corners(&sofa_parts[part], corners);
      for (i = 0; i < 8; i++)
        screen[i] = project(rotate_xy(corners[i], SOFA_TILT, sc->angle), cx, cy, unit);

      for (i = 0; i < 12; i++)
      {
        const fix8_point_t *a, *b;

        a = &screen[cube_edges[i][0]];
        b = &screen[cube_edges[i][1]];
        screen_draw_line_wu_fix8(scr, a->x, a->y, b->x, b->y, sc->line);
      }
    }
  }
  else
  {
    fix8_point_t screen[NELEMS(ship_vertices)];
    int          i;

    for (i = 0; i < (int) NELEMS(ship_vertices); i++)
      screen[i] = project(rotate_xy(ship_vertices[i], SOFA_TILT, sc->angle), cx, cy, unit);

    for (i = 0; i < (int) NELEMS(ship_edges); i++)
    {
      const fix8_point_t *a, *b;

      a = &screen[ship_edges[i][0]];
      b = &screen[ship_edges[i][1]];
      screen_draw_line_wu_fix8(scr, a->x, a->y, b->x, b->y, sc->line);
    }
  }

  return result_OK;
}

static result_t sofa_mouse(wuss_window_t *window, wuss_button_t button, void *task_data)
{
  sofa_task_t *sc;

  sc = task_data;

  if (button == wuss_BUTTON_ADJUST)
  {
    sc->shape = (sc->shape == sofa_SHAPE_SOFA) ? sofa_SHAPE_SHIP : sofa_SHAPE_SOFA;
    wuss_window_invalidate_all(window);
  }
  else
  {
    sc->spinning = !sc->spinning;
  }

  return result_OK;
}

static result_t sofa_scroll(wuss_window_t *window, int delta, void *task_data)
{
  sofa_task_t *sc;

  sc = task_data;

  sc->zoom += delta * SOFA_ZOOM_PER_NOTCH;
  sc->zoom  = CLAMP(sc->zoom, SOFA_ZOOM_MIN, SOFA_ZOOM_MAX);

  wuss_window_invalidate_all(window);

  return result_OK;
}

static result_t sofa_idle(void *task_data)
{
  sofa_task_t *task;

  task = task_data;

  if (!task->spinning)
    return result_OK;

  task->angle += SOFA_SPIN_PER_FRAME;
  if (task->angle > 2.0 * M_PI)
    task->angle -= 2.0 * M_PI;

  wuss_window_invalidate_all(task->window);

  return result_OK;
}

result_t sofa_handle(wuss_window_t     *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  sofa_task_t *sc;

  sc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return sofa_redraw(window, event->data.redraw.scr, event->data.redraw.content, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return sofa_mouse(window, event->data.mouse.button, task_data);

  case wuss_EVENT_SCROLL:
    return sofa_scroll(window, event->data.scroll.delta, task_data);

  case wuss_EVENT_IDLE:
    return sofa_idle(task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_destroy(window);
    sc->window = NULL;
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
