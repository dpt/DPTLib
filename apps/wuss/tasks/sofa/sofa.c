/* wuss/tasks/sofa/sofa.c -- rotating wireframe sofa and spaceship task */

#ifdef WUSS_APP

#include <stdlib.h>

#include <math.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "utils/fxp.h"

#include "../common/snapshot.h"
#include "sofa.h"
#include "../common/taskapp.h"
#include "../common/common.h"

/* MENU click pops this menu; the item table and wuss_menu_t live per-instance
 * in sofa_task_t, not as a file-scope static, so that each window's Info row
 * can hold its own .window pointer to the shared proginfo singleton, retargeted
 * just before wuss_menu_open */
enum
{
  SOFA_MENU_PAUSE = 0,
  SOFA_MENU_SAVE,
  SOFA_MENU_MODEL,
  SOFA_MENU_CYCLE,
  SOFA_MENU_COLOURS,
  SOFA_MENU_RANDOMISE,
  SOFA_MENU_RESET
};

#define SOFA_SAVE_NAME "sofa.png" /* Save As's initial leafname */

/* "Model" submenu rows, in sofa_shape_t order */
static const char *sofa_shape_names[sofa_SHAPE__LIMIT] =
{
  "Sofa",
  "Ship",
  "Cobra",
  "Tetrahedron",
  "Cube",
  "Octahedron",
  "Icosahedron",
  "Dodecahedron"
};

#define SOFA_VERTEX_DOT 2 /* side, px, of the white marker square drawn at each vertex */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SOFA_TILT           -0.5 /* static camera tilt, radians, so the seat is visible from above */
#define SOFA_SPIN_PER_FRAME  0.02 /* radians/frame at 60fps, one turn every ~5s */
#define SOFA_ROTATIONS_PER_MODEL 2 /* auto-cycle to the next model after this many full turns */
#define SOFA_CAMERA_DIST     4.0  /* perspective divisor: bigger = flatter */
#define SOFA_UNIT_FRACTION   0.35 /* fraction of min(width,height) per model unit, at zoom 1.0 */
#define SOFA_ZOOM_MIN        0.2
#define SOFA_ZOOM_MAX        4.0
#define SOFA_ZOOM_PER_NOTCH  0.1
#define SOFA_KEY_STEP        0.1 /* radians per Up/Down press while paused */

/* one 3D point */
typedef struct vec3 { double x, y, z; } vec3_t;

/* an axis-aligned box in model space, given as opposite corners */
typedef struct box3 { double x0, y0, z0, x1, y1, z1; } box3_t;

/* the sofa: seat, backrest and two arms, in model units (roughly -1..1) */
static const box3_t sofa_parts[] =
{
  { -0.8, -0.3, -0.5,   0.8,  0.1,  0.5 }, /* seat */
  { -1.0, -0.3,  0.5,   1.0,  0.9,  0.8 }, /* backrest */
  { -1.0, -0.3, -0.5,  -0.8,  0.5,  0.5 }, /* left arm */
  {  0.8, -0.3, -0.5,   1.0,  0.5,  0.5 }, /* right arm */
};

/* cube corner edges, indexing box3_corners' bit-numbered corners (bit0=x,
 * bit1=y, bit2=z; each pair differs in exactly one bit) */
static const int box_cube_edges[12][2] =
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

/* a Cobra Mk. 3 from Elite */
static const vec3_t cobra_vertices[28] =
{
  {  32/100.0,   0/100.0,  76/100.0 },
  { -32/100.0,   0/100.0,  76/100.0 },
  {   0/100.0,  26/100.0,  24/100.0 },
  {-120/100.0,  -3/100.0,  -8/100.0 },
  { 120/100.0,  -3/100.0,  -8/100.0 },
  { -88/100.0,  16/100.0, -40/100.0 },
  {  88/100.0,  16/100.0, -40/100.0 },
  { 128/100.0,  -8/100.0, -40/100.0 },
  {-128/100.0,  -8/100.0, -40/100.0 },
  {   0/100.0,  26/100.0, -40/100.0 },
  { -32/100.0, -24/100.0, -40/100.0 },
  {  32/100.0, -24/100.0, -40/100.0 },
  { -36/100.0,   8/100.0, -40/100.0 },
  {  -8/100.0,  12/100.0, -40/100.0 },
  {   8/100.0,  12/100.0, -40/100.0 },
  {  36/100.0,   8/100.0, -40/100.0 },
  {  36/100.0, -12/100.0, -40/100.0 },
  {   8/100.0, -16/100.0, -40/100.0 },
  {  -8/100.0, -16/100.0, -40/100.0 },
  { -36/100.0, -12/100.0, -40/100.0 },
  {   0/100.0,   0/100.0,  76/100.0 },
  {   0/100.0,   0/100.0,  90/100.0 },
  { -80/100.0,  -6/100.0, -40/100.0 },
  { -80/100.0,   6/100.0, -40/100.0 },
  { -88/100.0,   0/100.0, -40/100.0 },
  {  80/100.0,   6/100.0, -40/100.0 },
  {  88/100.0,   0/100.0, -40/100.0 },
  {  80/100.0,  -6/100.0, -40/100.0 },
};

static const int cobra_edges[38][2] =
{
  {  0,  1 },
  {  0,  4 },
  {  1,  3 },
  {  3,  8 },
  {  4,  7 },
  {  6,  7 },
  {  6,  9 },
  {  5,  9 },
  {  5,  8 },
  {  2,  5 },
  {  2,  6 },
  {  3,  5 },
  {  4,  6 },
  {  1,  2 },
  {  0,  2 },
  {  8, 10 },
  { 10, 11 },
  {  7, 11 },
  {  1, 10 },
  {  0, 11 },
  {  1,  5 },
  {  0,  6 },
  { 20, 21 },
  { 12, 13 },
  { 18, 19 },
  { 14, 15 },
  { 16, 17 },
  { 15, 16 },
  { 14, 17 },
  { 13, 18 },
  { 12, 19 },
  {  2,  9 },
  { 22, 24 },
  { 23, 24 },
  { 22, 23 },
  { 25, 26 },
  { 26, 27 },
  { 25, 27 },
};

/* the five Platonic solids, vertices normalised to unit circumradius */

static const vec3_t tetra_vertices[4] =
{
  {  0.577350269189626,  0.577350269189626,  0.577350269189626 },
  {  0.577350269189626, -0.577350269189626, -0.577350269189626 },
  { -0.577350269189626,  0.577350269189626, -0.577350269189626 },
  { -0.577350269189626, -0.577350269189626,  0.577350269189626 },
};
static const int tetra_edges[6][2] =
{
  { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 2 },
  { 1, 3 }, { 2, 3 },
};

static const vec3_t cube_vertices[8] =
{
  {  0.577350269189626,  0.577350269189626,  0.577350269189626 },
  {  0.577350269189626,  0.577350269189626, -0.577350269189626 },
  {  0.577350269189626, -0.577350269189626,  0.577350269189626 },
  {  0.577350269189626, -0.577350269189626, -0.577350269189626 },
  { -0.577350269189626,  0.577350269189626,  0.577350269189626 },
  { -0.577350269189626,  0.577350269189626, -0.577350269189626 },
  { -0.577350269189626, -0.577350269189626,  0.577350269189626 },
  { -0.577350269189626, -0.577350269189626, -0.577350269189626 },
};
static const int cube_edges[12][2] =
{
  { 0, 1 }, { 0, 2 }, { 0, 4 }, { 1, 3 },
  { 1, 5 }, { 2, 3 }, { 2, 6 }, { 3, 7 },
  { 4, 5 }, { 4, 6 }, { 5, 7 }, { 6, 7 },
};

static const vec3_t octa_vertices[6] =
{
  {               1,               0,               0 },
  {              -1,               0,               0 },
  {               0,               1,               0 },
  {               0,              -1,               0 },
  {               0,               0,               1 },
  {               0,               0,              -1 },
};
static const int octa_edges[12][2] =
{
  { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 },
  { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 },
  { 2, 4 }, { 2, 5 }, { 3, 4 }, { 3, 5 },
};

static const vec3_t icosa_vertices[12] =
{
  {               0,  0.525731112119134,  0.85065080835204 },
  {               0,  0.525731112119134, -0.85065080835204 },
  {               0, -0.525731112119134,  0.85065080835204 },
  {               0, -0.525731112119134, -0.85065080835204 },
  {  0.525731112119134,  0.85065080835204,               0 },
  {  0.525731112119134, -0.85065080835204,               0 },
  { -0.525731112119134,  0.85065080835204,               0 },
  { -0.525731112119134, -0.85065080835204,               0 },
  {  0.85065080835204,               0,  0.525731112119134 },
  {  0.85065080835204,               0, -0.525731112119134 },
  { -0.85065080835204,               0,  0.525731112119134 },
  { -0.85065080835204,               0, -0.525731112119134 },
};
static const int icosa_edges[30][2] =
{
  { 0, 2 }, { 0, 4 }, { 0, 6 }, { 0, 8 },
  { 0, 10 }, { 1, 3 }, { 1, 4 }, { 1, 6 },
  { 1, 9 }, { 1, 11 }, { 2, 5 }, { 2, 7 },
  { 2, 8 }, { 2, 10 }, { 3, 5 }, { 3, 7 },
  { 3, 9 }, { 3, 11 }, { 4, 6 }, { 4, 8 },
  { 4, 9 }, { 5, 7 }, { 5, 8 }, { 5, 9 },
  { 6, 10 }, { 6, 11 }, { 7, 10 }, { 7, 11 },
  { 8, 9 }, { 10, 11 },
};

static const vec3_t dodeca_vertices[20] =
{
  {  0.577350269189626,  0.577350269189626,  0.577350269189626 },
  {  0.577350269189626,  0.577350269189626, -0.577350269189626 },
  {  0.577350269189626, -0.577350269189626,  0.577350269189626 },
  {  0.577350269189626, -0.577350269189626, -0.577350269189626 },
  { -0.577350269189626,  0.577350269189626,  0.577350269189626 },
  { -0.577350269189626,  0.577350269189626, -0.577350269189626 },
  { -0.577350269189626, -0.577350269189626,  0.577350269189626 },
  { -0.577350269189626, -0.577350269189626, -0.577350269189626 },
  {               0,  0.35682208977309,  0.934172358962716 },
  {               0,  0.35682208977309, -0.934172358962716 },
  {               0, -0.35682208977309,  0.934172358962716 },
  {               0, -0.35682208977309, -0.934172358962716 },
  {  0.35682208977309,  0.934172358962716,               0 },
  {  0.35682208977309, -0.934172358962716,               0 },
  { -0.35682208977309,  0.934172358962716,               0 },
  { -0.35682208977309, -0.934172358962716,               0 },
  {  0.934172358962716,               0,  0.35682208977309 },
  {  0.934172358962716,               0, -0.35682208977309 },
  { -0.934172358962716,               0,  0.35682208977309 },
  { -0.934172358962716,               0, -0.35682208977309 },
};
static const int dodeca_edges[30][2] =
{
  { 0, 8 }, { 0, 12 }, { 0, 16 }, { 1, 9 },
  { 1, 12 }, { 1, 17 }, { 2, 10 }, { 2, 13 },
  { 2, 16 }, { 3, 11 }, { 3, 13 }, { 3, 17 },
  { 4, 8 }, { 4, 14 }, { 4, 18 }, { 5, 9 },
  { 5, 14 }, { 5, 19 }, { 6, 10 }, { 6, 15 },
  { 6, 18 }, { 7, 11 }, { 7, 15 }, { 7, 19 },
  { 8, 10 }, { 9, 11 }, { 12, 14 }, { 13, 15 },
  { 16, 17 }, { 18, 19 },
};

/* a wireframe model: a vertex array plus an edge index array */
typedef struct wireframe
{
  const vec3_t *vertices;
  int           nvertices;
  const int   (*edges)[2];
  int           nedges;
}
wireframe_t;

#define WIREFRAME(v, e) { v, NELEMS(v), e, NELEMS(e) }

static const wireframe_t polyhedra[] =
{
  WIREFRAME(tetra_vertices,  tetra_edges),
  WIREFRAME(cube_vertices,   cube_edges),
  WIREFRAME(octa_vertices,   octa_edges),
  WIREFRAME(icosa_vertices,  icosa_edges),
  WIREFRAME(dodeca_vertices, dodeca_edges),
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

/* a marker square, SOFA_VERTEX_DOT on a side, centred on each projected
 * vertex; drawn after the wireframe so the dots sit on top of the edges */
static void draw_vertex_dots(screen_t           *scr,
                             const fix8_point_t *screen,
                             int                 nvertices,
                             colour_t            colour)
{
  int half;
  int i;

  half = SOFA_VERTEX_DOT / 2;

  for (i = 0; i < nvertices; i++)
    screen_fill_square(scr,
                       FIX8_ROUND_TO_INT(screen[i].x) - half,
                       FIX8_ROUND_TO_INT(screen[i].y) - half,
                       SOFA_VERTEX_DOT,
                       colour);
}

/* wuss_saveas_save_fn_t: opaque is the sofa_task_t */
static result_t sofa_saveas_save(const char *path, void *opaque)
{
  sofa_task_t *sc;

  sc = opaque;

  return snapshot_save_png(sc->window, sofa_handle, sc, path);
}

/* the starting view, as sofa_create and Reset restore it */
static void sofa_defaults(sofa_task_t *task)
{
  task->bg       = colour_rgb(0x7E, 0x25, 0x53);
  task->line     = colour_rgb(0xFF, 0xA3, 0x00);
  task->dot      = colour_rgb(0xFF, 0xFF, 0xFF);
  task->angle    = 0.0;
  task->zoom     = 1.0;
  task->spinning = true;
  task->shape    = sofa_SHAPE_SOFA;
  task->turns    = 0;
  task->cycling  = true;
}

/* pick the model and every colour afresh */
static void sofa_randomise(sofa_task_t *task)
{
  task->shape = (sofa_shape_t) rng_range(&task->rng, sofa_SHAPE__LIMIT);
  task->turns = 0;
  task->bg    = colour_random(&task->rng);
  task->line  = colour_random_contrast(&task->rng, task->bg);
  task->dot   = colour_random_contrast(&task->rng, task->bg);
}

result_t sofa_create(wuss_t *wuss, sofa_task_t **out)
{
  result_t         rc;
  sofa_task_t     *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss     = wuss;
  rng_seed(&task->rng, (uint32_t) rand());
  sofa_defaults(task);

  /* sofa_redraw paints its own background every frame */
  delegate_desc.handle    = sofa_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "sofa";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate, SIZE2D(180, 160), "Sofa", &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  rc = snapshot_saveas_create(&task->saveas, wuss, SOFA_SAVE_NAME,
                              sofa_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  wuss_colourlist_init_pair(&task->colours, wuss, &task->line, &task->bg);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SOFA_MENU_PAUSE, "Pause",
                          wuss_MENU_ITEM_NONE, "SPACE");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SOFA_MENU_SAVE, "Save",
                          wuss_MENU_ITEM_NONE, "F3");
  /* hover opens the Save As dialogue as a submenu; F3 shows it standalone */
  task->menu_items[SOFA_MENU_SAVE].window = wuss_saveas_window(task->saveas);

  for (i = 0; i < sofa_SHAPE__LIMIT; i++)
    WUSS_MENU_ITEM(task->model_items, i, sofa_shape_names[i],
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->model_menu, "Model", task->model_items,
                 NELEMS(task->model_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, SOFA_MENU_MODEL, "Model",
                      wuss_MENU_ITEM_DASHED, &task->model_menu);

  WUSS_MENU_ITEM(task->menu_items, SOFA_MENU_CYCLE, "Auto-cycle",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_ITEM_MENU(task->menu_items, SOFA_MENU_COLOURS, "Colours",
                      wuss_MENU_ITEM_NONE, &task->colours.menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SOFA_MENU_RANDOMISE, "Randomise",
                          wuss_MENU_ITEM_DASHED, "^R");
  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SOFA_MENU_RESET, "Reset",
                          wuss_MENU_ITEM_NONE, "R");

  WUSS_MENU_TITLE(task->menu, "Sofa", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

result_t sofa_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Sofa",
    "Rotating wireframe shapes",
    (taskapp_create_fn_t) sofa_create
  };

  return taskapp_install(wuss, &desc);
}

void sofa_destroy(sofa_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task);
}

static result_t sofa_redraw(const wuss_event_t *event, void *task_data)
{
  sofa_task_t *sc;
  screen_t    *scr;
  const box_t *content, *bounds;
  int          cx, cy, sx, sy;
  double       unit;
  size_t       part;

  sc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr,
                   content->x0,
                   content->y0, box_size(content),
                   sc->bg);

  cx   = bounds->x0 - sx + (bounds->x1 - bounds->x0) / 2;
  cy   = bounds->y0 - sy + (bounds->y1 - bounds->y0) / 2;
  unit = MIN(bounds->x1 - bounds->x0, bounds->y1 - bounds->y0) * SOFA_UNIT_FRACTION * sc->zoom;

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

        a = &screen[box_cube_edges[i][0]];
        b = &screen[box_cube_edges[i][1]];
        screen_draw_line_wu_fix8(scr, a->x, a->y, b->x, b->y, sc->line);
      }

      draw_vertex_dots(scr, screen, 8, sc->dot);
    }
  }
  else if (sc->shape == sofa_SHAPE_SHIP)
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

    draw_vertex_dots(scr, screen, (int) NELEMS(ship_vertices), sc->dot);
  }
  else if (sc->shape == sofa_SHAPE_COBRA)
  {
    fix8_point_t screen[NELEMS(cobra_vertices)];
    int          i;

    for (i = 0; i < (int) NELEMS(cobra_vertices); i++)
      screen[i] = project(rotate_xy(cobra_vertices[i], SOFA_TILT, sc->angle), cx, cy, unit);

    for (i = 0; i < (int) NELEMS(cobra_edges); i++)
    {
      const fix8_point_t *a, *b;

      a = &screen[cobra_edges[i][0]];
      b = &screen[cobra_edges[i][1]];
      screen_draw_line_wu_fix8(scr, a->x, a->y, b->x, b->y, sc->line);
    }

    draw_vertex_dots(scr, screen, (int) NELEMS(cobra_vertices), sc->dot);
  }
  else
  {
    const wireframe_t *wf;
    fix8_point_t        screen[NELEMS(dodeca_vertices)]; /* largest solid */
    int                i;

    wf = &polyhedra[sc->shape - sofa_SHAPE_TETRAHEDRON];

    for (i = 0; i < wf->nvertices; i++)
      screen[i] = project(rotate_xy(wf->vertices[i], SOFA_TILT, sc->angle), cx, cy, unit);

    for (i = 0; i < wf->nedges; i++)
    {
      const fix8_point_t *a, *b;

      a = &screen[wf->edges[i][0]];
      b = &screen[wf->edges[i][1]];
      screen_draw_line_wu_fix8(scr, a->x, a->y, b->x, b->y, sc->line);
    }

    draw_vertex_dots(scr, screen, wf->nvertices, sc->dot);
  }

  return result_OK;
}

static result_t sofa_mouse(wuss_window_t *window,
                           wuss_button_t  button,
                           void          *task_data)
{
  sofa_task_t *sc;

  sc = task_data;

  if (button & wuss_BUTTON_MENU)
  {
    wuss_menu_tick_exclusive(&sc->model_menu, sc->shape);
    wuss_menu_tick_item(&sc->menu, SOFA_MENU_PAUSE, !sc->spinning);
    wuss_menu_tick_item(&sc->menu, SOFA_MENU_CYCLE, sc->cycling);

    return wuss_menu_open_at_pointer(sc->delegate, &sc->menu,
                                     &sc->menu_handle);
  }

  if (button & wuss_BUTTON_ADJUST)
  {
    sc->shape = (sc->shape + 1) % sofa_SHAPE__LIMIT;
    sc->turns = 0;
    wuss_window_invalidate_visible(window);
  }
  else if (button & wuss_BUTTON_SELECT)
  {
    sc->spinning = !sc->spinning;
  }

  return result_OK;
}

/* Left/Right step to the previous/next model; Up/Down step the rotation
 * back/forward while paused. Anything else is passed back unclaimed. */
static result_t sofa_key(sofa_task_t *sc, int code)
{
  int dir;

  switch (code)
  {
  case wuss_KEY_UP:
  case wuss_KEY_DOWN:
    if (sc->spinning)
      return result_WUSS_KEY_UNCLAIMED;
    sc->angle += (code == wuss_KEY_UP ? -1 : 1) * SOFA_KEY_STEP;
    wuss_window_invalidate_visible(sc->window);
    return result_OK;

  case wuss_KEY_LEFT:  dir = sofa_SHAPE__LIMIT - 1; break;
  case wuss_KEY_RIGHT: dir = 1;                     break;
  default:             return result_WUSS_KEY_UNCLAIMED;
  }

  sc->shape = (sc->shape + dir) % sofa_SHAPE__LIMIT;
  sc->turns = 0;
  wuss_window_invalidate_visible(sc->window);

  return result_OK;
}

static result_t sofa_scroll(wuss_window_t *window,
                            int            delta,
                            void          *task_data)
{
  sofa_task_t *sc;

  sc = task_data;

  sc->zoom += delta * SOFA_ZOOM_PER_NOTCH;
  sc->zoom  = CLAMP(sc->zoom, SOFA_ZOOM_MIN, SOFA_ZOOM_MAX);

  wuss_window_invalidate_visible(window);

  return result_OK;
}

static result_t sofa_idle(void *task_data)
{
  sofa_task_t *task;

  task = task_data;

  /* the shared proginfo singleton is a second window on this same
   * (autoclose) delegate while its dialogue is open, so closing the main
   * window alone doesn't necessarily empty task->windows immediately --
   * guard against the dangling window in the meantime */
  if (task->window == NULL)
    return result_OK;

  if (!task->spinning)
    return result_OK;

  task->angle += SOFA_SPIN_PER_FRAME;
  if (task->angle > 2.0 * M_PI)
  {
    task->angle -= 2.0 * M_PI;
    if (task->cycling && ++task->turns >= SOFA_ROTATIONS_PER_MODEL)
    {
      task->turns = 0;
      task->shape = (task->shape + 1) % sofa_SHAPE__LIMIT;
    }
  }

  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

static result_t sofa_menu_select(sofa_task_t *sc, const wuss_event_t *event)
{
  if (wuss_menu_select_is(event, &sc->menu, SOFA_MENU_PAUSE))
  {
    sc->spinning = !sc->spinning;
    wuss_menu_tick_item_live(sc->menu_handle, &sc->menu, SOFA_MENU_PAUSE,
                             !sc->spinning);
    return result_OK;
  }

  if (wuss_menu_select_is(event, &sc->menu, SOFA_MENU_CYCLE))
  {
    sc->cycling = !sc->cycling;
    sc->turns   = 0; /* a re-enabled cycle gives the model its full turns */
    wuss_menu_tick_item_live(sc->menu_handle, &sc->menu, SOFA_MENU_CYCLE,
                             sc->cycling);
    return result_OK;
  }

  if (event->data.menu_select.menu == &sc->menu &&
      (event->data.menu_select.index == SOFA_MENU_RANDOMISE ||
       event->data.menu_select.index == SOFA_MENU_RESET))
  {
    if (event->data.menu_select.index == SOFA_MENU_RANDOMISE)
      sofa_randomise(sc);
    else
      sofa_defaults(sc);

    wuss_menu_tick_exclusive_live(sc->menu_handle, &sc->model_menu, sc->shape);
    wuss_menu_tick_item_live(sc->menu_handle, &sc->menu, SOFA_MENU_PAUSE,
                             !sc->spinning);
    wuss_menu_tick_item_live(sc->menu_handle, &sc->menu, SOFA_MENU_CYCLE,
                             sc->cycling);
    wuss_window_invalidate_visible(sc->window);
    return result_OK;
  }

  if (wuss_menu_select_is(event, &sc->menu, SOFA_MENU_SAVE))
  {
    return wuss_saveas_open(sc->saveas);
  }

  if (event->data.menu_select.menu == &sc->model_menu)
  {
    /* restart the model's rotation count so it gets its full turns before
     * the idle auto-advance moves on */
    sc->shape = (sofa_shape_t) event->data.menu_select.index;
    sc->turns = 0;
    wuss_menu_tick_exclusive_live(sc->menu_handle, &sc->model_menu, sc->shape);
    wuss_window_invalidate_visible(sc->window);
    return result_OK;
  }

  if (wuss_colourlist_selected(&sc->colours, event))
    wuss_window_invalidate_visible(sc->window);

  return result_OK;
}

/* Every submenu leaf fires wuss_EVENT_PRE_SUBMENU_OPEN -- "Model" too, which
 * opens unchanged. The foreground is the wireframe's edge colour. */
static result_t sofa_pre_submenu_open(sofa_task_t        *sc,
                                      const wuss_event_t *event)
{
  result_t rc;
  int      index;

  if (wuss_colourlist_pre_open(&sc->colours, sc->wuss, event, &rc))
    return rc;

  index = event->data.pre_submenu_open.index;

  return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle, index,
                                    sc->menu_items[index].submenu);
}

result_t sofa_handle(wuss_window_t      *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  sofa_task_t *sc;

  sc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return sofa_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != sc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return sofa_mouse(window, event->data.mouse.button, task_data);

  case wuss_EVENT_SCROLL:
    return sofa_scroll(window, event->data.scroll.delta, task_data);

  case wuss_EVENT_IDLE:
    return sofa_idle(task_data);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != sc->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(sc->delegate, &sc->menu, event, &rc))
      return rc;

    return sofa_key(sc, event->data.key.code);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return sofa_pre_submenu_open(sc, event);

  case wuss_EVENT_MENU_SELECT:
    return sofa_menu_select(sc, event);

  case wuss_EVENT_MENU_CLOSED:
    sc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == sc->window)
      sc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    sofa_destroy(sc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
