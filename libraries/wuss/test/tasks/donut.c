/* wuss/test/tasks/donut.c -- spinning ASCII-donut torus, rendered in pixels */

#ifdef WUSS_APP

#include <math.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "donut.h"

/* Andy Sloane's "donut.c" (https://www.a1k0n.net/2011/07/20/donut-math.html):
 * a torus (tube radius R1, ring radius R2) is swept over its two surface
 * angles theta/phi; each surface point is rotated by A (about x) and B (about
 * z), projected with a simple z/(K2+z) perspective divide, and z-buffered per
 * pixel. Luminance comes from the dot product of the surface normal with a
 * fixed light direction. The original packs this into a luminance-ramped
 * character cell; here it drives colour_grey brightness on a screen pixel
 * instead. */

#define DONUT_R1 1.0 /* tube radius */
#define DONUT_R2 2.0 /* ring radius */
#define DONUT_K2 5.0 /* viewer distance */

enum { DONUT_MENU_INFO = 0, DONUT_MENU_BACKGROUND };

/* one theta/phi surface sample, projected and shaded into out_x/out_y/
 * out_z/out_lum; returns 0 if the projected point falls outside [0,width)x
 * [0,height) so the caller can skip it */
static int donut_project(double  costheta,
                         double  sintheta,
                         double  phi,
                         double  a,
                         double  b,
                         int     width,
                         int     height,
                         double  k1,
                         int    *out_x,
                         int    *out_y,
                         double *out_z,
                         double *out_lum)
{
  double cosphi, sinphi;
  double cosa, sina, cosb, sinb;
  double circlex, circley;
  double x, y, z, ooz;
  double nx, ny;
  double lum;
  int    xp, yp;

  cosphi   = cos(phi);
  sinphi   = sin(phi);
  cosa     = cos(a);
  sina     = sin(a);
  cosb     = cos(b);
  sinb     = sin(b);

  circlex = DONUT_R2 + DONUT_R1 * costheta;
  circley = DONUT_R1 * sintheta;

  x = circlex * (cosb * cosphi + sina * sinb * sinphi) - circley * cosa * sinb;
  y = circlex * (sinb * cosphi - sina * cosb * sinphi) + circley * cosa * cosb;
  z = DONUT_K2 + cosa * circlex * sinphi + circley * sina;
  ooz = 1.0 / z;

  xp = (int) (width  / 2 + k1 * ooz * x * 2.0);
  yp = (int) (height / 2 - k1 * ooz * y);

  if (xp < 0 || xp >= width || yp < 0 || yp >= height)
    return 0;

  nx = costheta * cosphi;
  ny = costheta * sinphi;

  /* light direction (0,1,-1)/sqrt(2) rotated by the same A/B, dotted with the
   * surface normal rotated the same way -- expand directly rather than
   * building rotation matrices */
  {
    double ly, lz;

    ly = nx * (sinb * cosphi - sina * cosb * sinphi) + ny * cosa * cosb;
    lz = cosa * nx * sinphi + ny * sina;

    lum = (ly - lz) * 0.70710678;
  }

  *out_x   = xp;
  *out_y   = yp;
  *out_z   = ooz;
  *out_lum = lum;

  return 1;
}

result_t donut_create(wuss_t *wuss, donut_task_t **out)
{
  result_t         rc;
  donut_task_t    *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->bg   = colour_rgb(0x00, 0x00, 0x00);
  task->a    = 1.0;
  task->b    = 1.0;
  task->zoom = 1.0;

  /* donut_redraw paints its own background every frame */
  delegate_desc.handle    = donut_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "donut";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(200, 200),
                                 "Donut",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(200, 200),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, DONUT_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in donut_mouse */

  wuss_colourmenu_set_none(0);
  WUSS_MENU_ITEM_MENU(task->menu_items, DONUT_MENU_BACKGROUND, "Background",
                     wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  WUSS_MENU_TITLE(task->menu, "Donut", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void donut_destroy(donut_task_t *task)
{
  wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t donut_redraw(const wuss_event_t *event, donut_task_t *task)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  int          sx, sy, width, height;
  double       k1;
  double       theta, phi;
  int          x, y;
  double       z, lum;
  double      *zbuf;
  colour_t    *shade; /* one colour per z-buffer cell, painted at the end */

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  width  = box_size(bounds).w;
  height = box_size(bounds).h;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content), task->bg);

  zbuf  = calloc((size_t) (width * height), sizeof(*zbuf));
  shade = calloc((size_t) (width * height), sizeof(*shade));
  if (zbuf == NULL || shade == NULL)
  {
    free(zbuf);
    free(shade);
    return result_OOM;
  }

  k1 = width * DONUT_K2 * 3.0 / (8.0 * (DONUT_R1 + DONUT_R2)) * task->zoom;

  for (theta = 0.0; theta < 2.0 * M_PI; theta += 0.07)
  {
    double costheta, sintheta;

    costheta = cos(theta);
    sintheta = sin(theta);

    for (phi = 0.0; phi < 2.0 * M_PI; phi += 0.02)
    {
      int cell;

      if (!donut_project(costheta, sintheta, phi, task->a, task->b, width,
                         height, k1, &x, &y, &z, &lum))
        continue;

      cell = y * width + x;
      if (z > zbuf[cell])
      {
        int grey;

        zbuf[cell] = z;
        grey = (int) ((lum < 0.0 ? 0.0 : lum) * 255.0);
        if (grey > 255)
          grey = 255;
        shade[cell] = colour_rgb(grey, grey, grey);
      }
    }
  }

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
    {
      int cell;

      cell = y * width + x;
      if (zbuf[cell] > 0.0)
        screen_set_pixel(scr, bounds->x0 - sx + x, bounds->y0 - sy + y,
                         shade[cell]);
    }

  free(zbuf);
  free(shade);

  return result_OK;
}

static result_t donut_mouse(donut_task_t       *task,
                            wuss_mouse_action_t action,
                            wuss_button_t       button,
                            wuss_window_t      *window)
{
  if (window != task->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_MENU)
  {
    static const wuss_proginfo_desc_t desc =
    {
      "Donut",
      "Spinning torus, ray-marched and shaded per pixel",
      "(c) DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };
    wuss_proginfo_set_desc(&desc);
    task->menu_items[DONUT_MENU_INFO].window = wuss_proginfo_window(task->delegate);

    return wuss_menu_open(task->delegate, &task->menu,
                          wuss_get_pointer(task->wuss), &task->menu_handle);
  }

  if (button & wuss_BUTTON_SELECT)
    task->paused = !task->paused;

  return result_OK;
}

static result_t donut_scroll(donut_task_t  *task,
                             wuss_window_t *window,
                             int            delta)
{
  if (window != task->window)
    return result_OK;

  task->zoom += (delta > 0 ? 1 : -1) * DONUT_ZOOM_STEP;
  task->zoom  = CLAMP(task->zoom, DONUT_ZOOM_MIN, DONUT_ZOOM_MAX);

  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

static result_t donut_idle(donut_task_t *task)
{
  if (task->window == NULL)
    return result_OK; /* window closed but the proginfo dialogue -- a second
                       * window on this same autoclose delegate -- is still
                       * open, keeping the task alive */

  if (task->paused)
    return result_OK;

  task->a += 0.04;
  task->b += 0.02;

  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

/* The "Background" row's only submenu leaf: always hand back the shared
 * colourmenu singleton, unretargeted -- there is nothing else to pick into.
 */
static result_t donut_pre_submenu_open(donut_task_t       *task,
                                       const wuss_event_t *event)
{
  return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle,
                                    event->data.pre_submenu_open.index,
                                    wuss_colourmenu_menu(task->wuss));
}

static result_t donut_menu_select(donut_task_t       *task,
                                  const wuss_event_t *event)
{
  const colour_t *palette;
  int             npalette;
  wuss_colour_t   picked;
  int             mine;

  picked = wuss_colourmenu_selected(event, &mine);
  if (!mine)
    return result_OK;

  palette = wuss_get_palette(task->wuss, &npalette);
  if (picked < npalette)
  {
    task->bg = palette[picked];
    if (task->window != NULL)
      wuss_window_invalidate_visible(task->window);
  }

  return result_OK;
}

result_t donut_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  donut_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return donut_redraw(event, task);

  case wuss_EVENT_MOUSE:
    return donut_mouse(task, event->data.mouse.action,
                       event->data.mouse.button, window);

  case wuss_EVENT_SCROLL:
    return donut_scroll(task, window, event->data.scroll.delta);

  case wuss_EVENT_IDLE:
    return donut_idle(task);

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return donut_pre_submenu_open(task, event);

  case wuss_EVENT_MENU_SELECT:
    return donut_menu_select(task, event);

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

    if (window == task->menu_items[DONUT_MENU_INFO].window)
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
    donut_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
