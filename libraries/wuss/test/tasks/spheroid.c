/* wuss/test/tasks/spheroid.c -- KPT Spheroid Designer-style lit sphere */

#ifdef WUSS_APP

#include <math.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "wuss/task.h"

#include "spheroid.h"

/* After Kai's Power Tools 3's Spheroid Designer (MetaTools, 1995): one big
 * sphere lit by up to four coloured lights, with knobs for the highlight's
 * size and sharpness, a rim glow and an ambient floor.
 *
 * Each pixel inside the sphere's disc takes its surface normal N straight
 * from its position (the sphere is viewed orthographically, so the viewer
 * direction V is always +z). Per light L:
 *
 *   diffuse  = max(0, N.L) * light colour * sphere colour
 *   specular = smoothstep(max(0, N.H) ^ e) * light colour, H = |L + V|
 *
 * where the highlight size picks the exponent e and its sharpness narrows
 * the smoothstep around 0.5. Glow adds (1 - N.V)^3 of the sphere colour at
 * the rim and ambient a flat share of it. The sum is clamped, then blended
 * over the background by the pixel's disc coverage so the edge is
 * anti-aliased.
 *
 * A light is placed by pointing at the sphere: inside the disc it takes the
 * surface normal under the pointer. A band SPHEROID_WRAP radii wide just
 * outside the disc wraps round to the back hemisphere, z falling from 0 at
 * the rim to -1 at the band's outer edge, so rim and back lighting stay in
 * reach. Markers use the inverse, so a back light's marker sits in the band
 * where the pointer that placed it was. */

#define SPHEROID_MARGIN 8    /* px between the wrap band and the window edge */
#define SPHEROID_WRAP   0.25 /* wrap band width, in sphere radii */
#define SPHEROID_MARKER 4    /* marker ring radius, px */
#define SPHEROID_GRAB   6    /* px from a marker a click selects it */

enum
{
  SPHEROID_MENU_INFO = 0,
  SPHEROID_MENU_LIGHT
};

#define SPHEROID_LIGHT_ON SPHEROID_NLIGHTS /* row after the per-light rows */

/* per-redraw constants, worked out once rather than per pixel */
typedef struct spheroid_frame
{
  double sphere[3];     /* surface colour, 0..1 */
  double background[3]; /* 0..1 */
  int    nlights;       /* only the lights that are on */
  double l[SPHEROID_NLIGHTS][3]; /* light direction */
  double h[SPHEROID_NLIGHTS][3]; /* Blinn half-vector, |L + V| */
  double c[SPHEROID_NLIGHTS][3]; /* colour * intensity, 0..2 */
  double ambient;
  double glow;
  double exponent;
  double lo, hi;        /* specular smoothstep edges */
}
spheroid_frame_t;

/* ----------------------------------------------------------------------- */

static void spheroid_rgb(colour_t c, double scale, double out[3])
{
  unsigned int r, g, b;

  colour_get_rgb(&c, &r, &g, &b);
  out[0] = r / 255.0 * scale;
  out[1] = g / 255.0 * scale;
  out[2] = b / 255.0 * scale;
}

static void spheroid_set_light(spheroid_light_t *light,
                               int               on,
                               double            x,
                               double            y,
                               double            z,
                               colour_t          colour,
                               int               intensity)
{
  double len;

  len = sqrt(x * x + y * y + z * z);

  light->on        = on;
  light->x         = x / len;
  light->y         = y / len;
  light->z         = z / len;
  light->colour    = colour;
  light->intensity = intensity;
}

static void spheroid_defaults(spheroid_task_t *task)
{
  spheroid_set_light(&task->lights[0], 1, -0.5,  0.6,  0.6,
                     colour_rgb(0xFF, 0xFF, 0xFF), 100);
  spheroid_set_light(&task->lights[1], 1,  0.7, -0.6, -0.3,
                     colour_rgb(0x40, 0x60, 0xFF), 60);
  spheroid_set_light(&task->lights[2], 0,  0.6,  0.6,  0.5,
                     colour_rgb(0xFF, 0x80, 0x40), 100);
  spheroid_set_light(&task->lights[3], 0, -0.6, -0.6,  0.5,
                     colour_rgb(0x40, 0xFF, 0x80), 100);

  task->sphere     = colour_rgb(0x80, 0x80, 0x80);
  task->background = colour_rgb(0x00, 0x00, 0x00);
  task->ambient    = 10;
  task->glow       = 15;
  task->size       = 40;
  task->sharpness  = 30;
}

static void spheroid_prepare(const spheroid_task_t *task,
                             spheroid_frame_t      *f)
{
  const spheroid_light_t *light;
  double                  len, w;
  int                     i, n;

  spheroid_rgb(task->sphere, 1.0, f->sphere);
  spheroid_rgb(task->background, 1.0, f->background);

  n = 0;
  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    light = &task->lights[i];
    if (!light->on)
      continue;

    f->l[n][0] = light->x;
    f->l[n][1] = light->y;
    f->l[n][2] = light->z;

    /* L + V with V = (0,0,1); only degenerate for a light dead behind */
    len = sqrt(light->x * light->x + light->y * light->y +
               (light->z + 1.0) * (light->z + 1.0));
    if (len < 1e-6)
      len = 1e-6;
    f->h[n][0] = light->x / len;
    f->h[n][1] = light->y / len;
    f->h[n][2] = (light->z + 1.0) / len;

    spheroid_rgb(light->colour, light->intensity / 100.0, f->c[n]);
    n++;
  }
  f->nlights = n;

  f->ambient  = task->ambient / 100.0;
  f->glow     = task->glow / 50.0;
  f->exponent = 2.0 * pow(100.0, (100 - task->size) / 100.0); /* 200..2 */

  w     = 1.0 - task->sharpness * 0.0098; /* 1..0.02 */
  f->lo = 0.5 - w / 2.0;
  f->hi = 0.5 + w / 2.0;
}

/* the sphere's centre and radius for a content area of the given size, in
 * content-local coordinates */
static void spheroid_layout(size2d_t size, double *cx, double *cy, double *r)
{
  *cx = size.w / 2.0;
  *cy = size.h / 2.0;
  *r  = (MIN(size.w, size.h) / 2.0 - SPHEROID_MARGIN) / (1.0 + SPHEROID_WRAP);
}

/* point the light at content-local (px, py) */
static void spheroid_aim(spheroid_light_t *light,
                         double            cx,
                         double            cy,
                         double            r,
                         int               px,
                         int               py)
{
  double dx, dy, d, z, s;

  dx = (px - cx) / r;
  dy = (cy - py) / r;
  d  = sqrt(dx * dx + dy * dy);

  if (d <= 1.0)
  {
    light->x = dx;
    light->y = dy;
    light->z = sqrt(1.0 - d * d);
    return;
  }

  z = -MIN((d - 1.0) / SPHEROID_WRAP, 1.0);
  s = sqrt(1.0 - z * z);
  light->x = dx / d * s;
  light->y = dy / d * s;
  light->z = z;
}

/* the inverse of spheroid_aim: where the light's marker sits */
static void spheroid_marker_pos(const spheroid_light_t *light,
                                double                  cx,
                                double                  cy,
                                double                  r,
                                int                    *mx,
                                int                    *my)
{
  double s, d;

  if (light->z >= 0.0)
  {
    *mx = (int) floor(cx + light->x * r);
    *my = (int) floor(cy - light->y * r);
    return;
  }

  s = sqrt(light->x * light->x + light->y * light->y);
  d = (1.0 - light->z * SPHEROID_WRAP) * r;
  if (s < 1e-6)
  {
    /* dead behind: every direction round the band is equally right */
    *mx = (int) floor(cx);
    *my = (int) floor(cy - d);
    return;
  }

  *mx = (int) floor(cx + light->x / s * d);
  *my = (int) floor(cy - light->y / s * d);
}

/* a lit light's marker: a ring in its colour with a black outline for
 * contrast, dotted when behind the sphere, centre filled when current */
static void spheroid_draw_marker(screen_t               *scr,
                                 const spheroid_light_t *light,
                                 int                     x,
                                 int                     y,
                                 int                     current)
{
  colour_t black;
  int      i;

  black = colour_rgb(0x00, 0x00, 0x00);

  if (light->z >= 0.0)
  {
    screen_draw_circle(scr, x, y, SPHEROID_MARKER + 1, black);
    screen_draw_circle(scr, x, y, SPHEROID_MARKER, light->colour);
  }
  else
  {
    for (i = 0; i < 12; i++)
      screen_set_pixel(scr,
                       x + (int) floor(SPHEROID_MARKER * cos(i * M_PI / 6.0) + 0.5),
                       y + (int) floor(SPHEROID_MARKER * sin(i * M_PI / 6.0) + 0.5),
                       light->colour);
  }

  if (current)
    screen_fill_circle(scr, x, y, SPHEROID_MARKER - 2, light->colour);
}

/* shade the surface point with normal n into rgb (0..1, clamped) */
static void spheroid_shade(const spheroid_frame_t *f,
                           const double            n[3],
                           double                  rgb[3])
{
  double ndl, ndh, spec, t, rim;
  int    i, k;

  rim = pow(1.0 - n[2], 3.0) * f->glow;
  for (k = 0; k < 3; k++)
    rgb[k] = f->sphere[k] * (f->ambient + rim);

  for (i = 0; i < f->nlights; i++)
  {
    ndl = n[0] * f->l[i][0] + n[1] * f->l[i][1] + n[2] * f->l[i][2];
    if (ndl <= 0.0)
      continue;

    ndh  = n[0] * f->h[i][0] + n[1] * f->h[i][1] + n[2] * f->h[i][2];
    spec = ndh > 0.0 ? pow(ndh, f->exponent) : 0.0;
    t    = CLAMP((spec - f->lo) / (f->hi - f->lo), 0.0, 1.0);
    spec = t * t * (3.0 - 2.0 * t);

    for (k = 0; k < 3; k++)
      rgb[k] += f->c[i][k] * (ndl * f->sphere[k] + spec);
  }

  for (k = 0; k < 3; k++)
    rgb[k] = CLAMP(rgb[k], 0.0, 1.0);
}

/* ----------------------------------------------------------------------- */

result_t spheroid_create(wuss_t *wuss, spheroid_task_t **out)
{
  result_t         rc;
  spheroid_task_t *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  spheroid_defaults(task);

  delegate_desc.handle    = spheroid_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "spheroid";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  /* no scrollbars: the sphere is laid out across the visible content, so a
   * resize must redraw all of it and doc is only the growth ceiling */
  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(240, 240),
                                 "Spheroid Designer",
                                 wuss_WINDOW_CLOSE | wuss_WINDOW_BACK |
                                 wuss_WINDOW_TOGGLE_SIZE | wuss_WINDOW_RESIZE |
                                 wuss_WINDOW_NO_RESIZE_BLIT |
                                 wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(1024, 1024),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, SPHEROID_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in spheroid_mouse */

  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    static const char *const names[SPHEROID_NLIGHTS] =
    {
      "Light 1", "Light 2", "Light 3", "Light 4"
    };
    static const char *const keys[SPHEROID_NLIGHTS] =
    {
      "1", "2", "3", "4"
    };

    WUSS_MENU_ITEM_SHORTCUT(task->light_items, i, names[i],
                            wuss_MENU_ITEM_NONE, keys[i]);
  }

  WUSS_MENU_ITEM_SHORTCUT(task->light_items, SPHEROID_LIGHT_ON, "On",
                          wuss_MENU_ITEM_DASHED, "O");

  WUSS_MENU_TITLE(task->light_menu, "Light", task->light_items,
                  NELEMS(task->light_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, SPHEROID_MENU_LIGHT, "Light",
                      wuss_MENU_ITEM_NONE, &task->light_menu);

  WUSS_MENU_TITLE(task->menu, "Spheroid", task->menu_items,
                  NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void spheroid_destroy(spheroid_task_t *task)
{
  wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t spheroid_redraw(const wuss_event_t *event,
                                spheroid_task_t    *task)
{
  screen_t        *scr;
  const box_t     *content, *bounds;
  spheroid_frame_t f;
  double           cx, cy, r;
  double           dx, dy, dist, cov, n2, len;
  double           n[3], rgb[3];
  int              x, y, k, i;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  spheroid_prepare(task, &f);

  spheroid_layout(box_size(bounds), &cx, &cy, &r);
  cx += bounds->x0;
  cy += bounds->y0;

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = content->x0; x < content->x1; x++)
    {
      dx   = x + 0.5 - cx;
      dy   = y + 0.5 - cy;
      dist = sqrt(dx * dx + dy * dy);
      cov  = CLAMP(r - dist + 0.5, 0.0, 1.0);
      if (cov == 0.0)
      {
        screen_set_pixel(scr, x, y, task->background);
        continue;
      }

      /* screen y runs down, the model's runs up; an edge pixel whose
       * centre lies just outside the disc is pulled back onto the rim */
      n[0] =  dx / r;
      n[1] = -dy / r;
      n2   = n[0] * n[0] + n[1] * n[1];
      if (n2 > 1.0)
      {
        len   = sqrt(n2);
        n[0] /= len;
        n[1] /= len;
        n2    = 1.0;
      }
      n[2] = sqrt(1.0 - n2);

      spheroid_shade(&f, n, rgb);

      for (k = 0; k < 3; k++)
        rgb[k] = rgb[k] * cov + f.background[k] * (1.0 - cov);

      screen_set_pixel(scr, x, y,
                       colour_rgb((unsigned int) (rgb[0] * 255.0 + 0.5),
                                  (unsigned int) (rgb[1] * 255.0 + 0.5),
                                  (unsigned int) (rgb[2] * 255.0 + 0.5)));
    }
  }

  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    if (!task->lights[i].on)
      continue;

    spheroid_marker_pos(&task->lights[i], cx, cy, r, &x, &y);
    spheroid_draw_marker(scr, &task->lights[i], x, y, i == task->current);
  }

  return result_OK;
}

/* the Light submenu's ticks: the current light's row, and On if it's lit */
static unsigned int spheroid_light_ticks(const spheroid_task_t *task)
{
  unsigned int ticks;

  ticks = 1u << task->current;
  if (task->lights[task->current].on)
    ticks |= 1u << SPHEROID_LIGHT_ON;

  return ticks;
}

/* the lit light whose marker is within SPHEROID_GRAB px of (px, py), or -1 */
static int spheroid_hit_marker(const spheroid_task_t *task,
                               double                 cx,
                               double                 cy,
                               double                 r,
                               int                    px,
                               int                    py)
{
  int i, mx, my;

  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    if (!task->lights[i].on)
      continue;

    spheroid_marker_pos(&task->lights[i], cx, cy, r, &mx, &my);
    if ((px - mx) * (px - mx) + (py - my) * (py - my) <=
        SPHEROID_GRAB * SPHEROID_GRAB)
      return i;
  }

  return -1;
}

static result_t spheroid_mouse(spheroid_task_t    *task,
                               wuss_mouse_action_t action,
                               point_t             p,
                               wuss_button_t       button,
                               wuss_window_t      *window)
{
  static const wuss_proginfo_desc_t desc =
  {
    "Spheroid Designer",
    "Lit sphere after Kai's Power Tools",
    "© DPTLib contributors",
    "1.0 (" __DATE__ ")"
  };

  box_t  content;
  double cx, cy, r;
  int    hit;

  if (window != task->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  if (action == wuss_MOUSE_UP)
  {
    task->dragging = 0;
    return result_OK;
  }

  if (action == wuss_MOUSE_DOWN && (button & wuss_BUTTON_MENU))
  {
    wuss_proginfo_set_desc(&desc);
    task->menu_items[SPHEROID_MENU_INFO].window = wuss_proginfo_window(task->delegate);
    wuss_menu_tick_set(&task->light_menu, spheroid_light_ticks(task));

    return wuss_menu_open(task->delegate, &task->menu,
                          wuss_get_pointer(task->wuss), &task->menu_handle);
  }

  if (action == wuss_MOUSE_DOWN && !(button & wuss_BUTTON_SELECT))
    return result_OK;

  if (action == wuss_MOUSE_MOVE && !task->dragging)
    return result_OK;

  wuss_window_get_content_bounds(window, &content);
  spheroid_layout(box_size(&content), &cx, &cy, &r);

  if (action == wuss_MOUSE_DOWN)
  {
    task->dragging = 1;

    hit = spheroid_hit_marker(task, cx, cy, r, p.x, p.y);
    if (hit >= 0)
    {
      task->current = hit; /* grab it where it is; a move then drags it */
      wuss_window_invalidate_visible(window);
      return result_OK;
    }
  }

  task->lights[task->current].on = 1;
  spheroid_aim(&task->lights[task->current], cx, cy, r, p.x, p.y);
  wuss_window_invalidate_visible(window);

  return result_OK;
}

static result_t spheroid_menu_select(spheroid_task_t    *task,
                                     const wuss_event_t *event)
{
  int index;

  if (event->data.menu_select.menu != &task->light_menu)
    return result_OK;

  index = event->data.menu_select.index;
  if (index == SPHEROID_LIGHT_ON)
    task->lights[task->current].on = !task->lights[task->current].on;
  else
    task->current = index;

  if (wuss_menu_should_keep_open(event))
    wuss_menu_tick_set_live(task->menu_handle, &task->light_menu,
                            spheroid_light_ticks(task));

  if (task->window != NULL)
    wuss_window_invalidate_visible(task->window);

  return result_OK;
}

result_t spheroid_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  spheroid_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return spheroid_redraw(event, task);

  case wuss_EVENT_MOUSE:
    return spheroid_mouse(task, event->data.mouse.action,
                          event->data.mouse.point, event->data.mouse.button,
                          window);

  case wuss_EVENT_KEY:
    if (window != task->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */
    return wuss_menu_dispatch_shortcut(task->delegate, &task->menu, event);

  case wuss_EVENT_MENU_SELECT:
    return spheroid_menu_select(task, event);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == task->window)
      task->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == task->menu_items[SPHEROID_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
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
    spheroid_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
