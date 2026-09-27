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
 * anti-aliased. */

#define SPHEROID_MARGIN 8 /* px between the sphere and the window edge */

enum
{
  SPHEROID_MENU_INFO = 0
};

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
  int              x, y, k;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  spheroid_prepare(task, &f);

  cx = (bounds->x0 + bounds->x1) / 2.0;
  cy = (bounds->y0 + bounds->y1) / 2.0;
  r  = MIN(box_size(bounds).w, box_size(bounds).h) / 2.0 - SPHEROID_MARGIN;

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

  return result_OK;
}

static result_t spheroid_mouse(spheroid_task_t    *task,
                               wuss_mouse_action_t action,
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

  if (window != task->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  if (action != wuss_MOUSE_DOWN || !(button & wuss_BUTTON_MENU))
    return result_OK;

  wuss_proginfo_set_desc(&desc);
  task->menu_items[SPHEROID_MENU_INFO].window = wuss_proginfo_window(task->delegate);

  return wuss_menu_open(task->delegate, &task->menu,
                        wuss_get_pointer(task->wuss), &task->menu_handle);
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
                          event->data.mouse.button, window);

  case wuss_EVENT_KEY:
    if (window != task->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */
    return wuss_menu_dispatch_shortcut(task->delegate, &task->menu, event);

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
