/* wuss/tasks/doughnut.c -- spinning ASCII-doughnut torus, rendered in pixels */

#ifdef WUSS_APP

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "io/filetype.h"
#include "wuss/task.h"

#include "doughnut.h"
#include "taskapp.h"
#include "common.h"
#include "snapshot.h"

/* Andy Sloane's "donut.c" (www.a1k0n.net/2011/07/20/donut-math.html):
 * a torus (tube radius R1, ring radius R2) is swept over its two surface
 * angles theta/phi; each surface point is rotated by A (about x) and B (about
 * z), projected with a simple z/(K2+z) perspective divide, and z-buffered per
 * pixel. Luminance comes from the dot product of the surface normal with a
 * fixed light direction. The original packs this into a luminance-ramped
 * character cell; here it drives colour_grey brightness on a screen pixel
 * instead. */

#define DOUGHNUT_R2   2.0  /* ring radius */
#define DOUGHNUT_K2   5.0  /* viewer distance */
#define DOUGHNUT_KEY_STEP 0.1 /* radians per arrow press while paused */

enum
{
  DOUGHNUT_MENU_COLOURS = 0,
  DOUGHNUT_MENU_TUBE,
  DOUGHNUT_MENU_MAJOR,
  DOUGHNUT_MENU_MINOR,
  DOUGHNUT_MENU_PAUSE,
  DOUGHNUT_MENU_RANDOMISE,
  DOUGHNUT_MENU_RESET,
  DOUGHNUT_MENU_SAVE
};

#define DOUGHNUT_SAVE_NAME "doughnut.png" /* Save As's initial leafname */

/* "Tube" submenu rows: tube radius R1, kept below DOUGHNUT_R2 so the hole
 * stays open */
static const struct
{
  const char *name;
  double      r1;
}
doughnut_tubes[] =
{
  { "Thin",    0.5 },
  { "Classic", 1.0 },
  { "Plump",   1.5 },
  { "Fat",     1.9 }
};

#define DOUGHNUT_TUBE_CLASSIC 1

/* "Major density" / "Minor density" submenu rows: samples round the ring
 * (phi) and round the tube (theta) */
static const struct
{
  const char *name;
  int         major, minor;
}
doughnut_densities[] =
{
  { "Sparse",  40,  12 },
  { "Low",     80,  24 },
  { "Medium", 160,  48 },
  { "High",   314,  90 }, /* the original donut.c */
  { "Max",    628, 180 }
};

#define DOUGHNUT_DENSITY_DEFAULT 3

/* one theta/phi surface sample, projected and shaded into out_x/out_y/
 * out_z/out_lum; returns 0 if the projected point falls outside [0,width)x
 * [0,height) so the caller can skip it */
static int doughnut_project(double  r1,
                            double  costheta,
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

  circlex = DOUGHNUT_R2 + r1 * costheta;
  circley = r1 * sintheta;

  x = circlex * (cosb * cosphi + sina * sinb * sinphi) - circley * cosa * sinb;
  y = circlex * (sinb * cosphi - sina * cosb * sinphi) + circley * cosa * cosb;
  z = DOUGHNUT_K2 + cosa * circlex * sinphi + circley * sina;
  ooz = 1.0 / z;

  xp = (int) (width  / 2 + k1 * ooz * x);
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

/* palette[0] is the background; 1..255 ramp from black up to the foreground
 * (darkest shade is 1, not 0, since index 0 is the background) */
static void doughnut_palette_build(doughnut_task_t *task)
{
  unsigned int r, g, b;
  int          i;

  colour_get_rgb(&task->fg, &r, &g, &b);

  task->palette[0] = task->bg;
  for (i = 1; i < 256; i++)
    task->palette[i] = colour_rgb(r * i / 255, g * i / 255, b * i / 255);
}

/* wuss_saveas_save_fn_t: opaque is the doughnut_task_t */
static result_t doughnut_saveas_save(const char *path, void *opaque)
{
  doughnut_task_t *task;

  task = opaque;

  return snapshot_save_png(task->window, doughnut_handle, task, path);
}

/* the starting view, as doughnut_create and Reset restore it */
static void doughnut_defaults(doughnut_task_t *task)
{
  task->a     = 1.0;
  task->b     = 1.0;
  task->zoom  = 1.0;
  task->tube  = DOUGHNUT_TUBE_CLASSIC;
  task->major = DOUGHNUT_DENSITY_DEFAULT;
  task->minor = DOUGHNUT_DENSITY_DEFAULT;
  task->bg    = colour_rgb(0x20, 0x20, 0x20);
  task->fg    = colour_rgb(0xFF, 0xFF, 0xFF);
  doughnut_palette_build(task);
}

/* pick the angles, tube, densities and colours afresh; zoom stays */
static void doughnut_randomise(doughnut_task_t *task)
{
  task->a     = rng_range(&task->rng, 628) / 100.0;
  task->b     = rng_range(&task->rng, 628) / 100.0;
  task->tube  = rng_range(&task->rng, NELEMS(doughnut_tubes));
  task->major = rng_range(&task->rng, NELEMS(doughnut_densities));
  task->minor = rng_range(&task->rng, NELEMS(doughnut_densities));
  task->bg    = colour_random(&task->rng);
  task->fg    = colour_random_contrast(&task->rng, task->bg);
  doughnut_palette_build(task);
}

result_t doughnut_create(wuss_t *wuss, doughnut_task_t **out)
{
  result_t         rc;
  doughnut_task_t *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  filetype_t       png_type;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  rng_seed(&task->rng, (uint32_t) rand());
  doughnut_defaults(task);

  /* doughnut_redraw paints its own background every frame */
  delegate_desc.handle    = doughnut_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "doughnut";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(200, 200),
                          "Doughnut",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type,
                          DOUGHNUT_SAVE_NAME, doughnut_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  wuss_colourlist_init_pair(&task->colours, wuss, &task->fg, &task->bg);

  WUSS_MENU_ITEM_MENU(task->menu_items, DOUGHNUT_MENU_COLOURS, "Colours",
                      wuss_MENU_ITEM_NONE, &task->colours.menu);

  for (i = 0; i < NELEMS(task->tube_items); i++)
    WUSS_MENU_ITEM(task->tube_items, i, doughnut_tubes[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->tube_menu, "Tube", task->tube_items,
                 NELEMS(task->tube_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, DOUGHNUT_MENU_TUBE, "Tube",
                      wuss_MENU_ITEM_NONE, &task->tube_menu);

  for (i = 0; i < NELEMS(task->major_items); i++)
  {
    WUSS_MENU_ITEM(task->major_items, i, doughnut_densities[i].name,
                   wuss_MENU_ITEM_NONE);
    WUSS_MENU_ITEM(task->minor_items, i, doughnut_densities[i].name,
                   wuss_MENU_ITEM_NONE);
  }

  WUSS_MENU_TITLE(task->major_menu, "Major density", task->major_items,
                  NELEMS(task->major_items));
  WUSS_MENU_TITLE(task->minor_menu, "Minor density", task->minor_items,
                  NELEMS(task->minor_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, DOUGHNUT_MENU_MAJOR, "Major density",
                      wuss_MENU_ITEM_NONE, &task->major_menu);
  WUSS_MENU_ITEM_MENU(task->menu_items, DOUGHNUT_MENU_MINOR, "Minor density",
                      wuss_MENU_ITEM_NONE, &task->minor_menu);
  task->menu_items[DOUGHNUT_MENU_TUBE].shortcut  = "- +";
  task->menu_items[DOUGHNUT_MENU_MAJOR].shortcut = "[ ]";
  task->menu_items[DOUGHNUT_MENU_MINOR].shortcut = ", .";

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, DOUGHNUT_MENU_PAUSE, "Pause",
                          wuss_MENU_ITEM_NONE, "SPACE");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, DOUGHNUT_MENU_RANDOMISE,
                          "Randomise", wuss_MENU_ITEM_NONE, "^R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, DOUGHNUT_MENU_RESET, "Reset",
                          wuss_MENU_ITEM_NONE, "R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, DOUGHNUT_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");
  /* hover opens the Save As dialogue as a submenu; ^S shows it standalone */
  task->menu_items[DOUGHNUT_MENU_SAVE].window =
    wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Doughnut", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

result_t doughnut_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Doughnut",
    "Spinning ray-marched torus",
    (taskapp_create_fn_t) doughnut_create
  };

  return taskapp_install(wuss, &desc);
}

void doughnut_destroy(doughnut_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task->zbuf);
  free(task->shade);
  free(task);
}

static result_t doughnut_redraw(const wuss_event_t *event,
                                doughnut_task_t    *task)
{
  result_t       rc;
  screen_t      *scr;
  const box_t   *content, *bounds;
  int            sx, sy, width, height;
  box_t          drawn;
  size_t         ncells;
  double        *zbuf;
  unsigned char *shade; /* palette index per z-buffer cell: 0 = background */
  int            x, y;
  double         r1;
  double         k1;
  double         theta, phi, dtheta, dphi;
  double         z, lum;
  bitmap_t       bm;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  width  = box_size(bounds).w;
  height = box_size(bounds).h;

  /* the shade bitmap paints its own background, so only fill whatever part of
   * the content it doesn't reach */
  drawn.x0 = bounds->x0 - sx;
  drawn.y0 = bounds->y0 - sy;
  drawn.x1 = drawn.x0 + width;
  drawn.y1 = drawn.y0 + height;
  if (!box_contains_box(content, &drawn))
    screen_fill_rect(scr, content->x0, content->y0, box_size(content),
                     task->palette[0]);

  ncells = (size_t) (width * height);
  if (ncells > task->ncells)
  {
    free(task->zbuf);
    free(task->shade);
    task->zbuf   = malloc(ncells * sizeof(*task->zbuf));
    task->shade  = malloc(ncells * sizeof(*task->shade));
    task->ncells = ncells;
    if (task->zbuf == NULL || task->shade == NULL)
    {
      free(task->zbuf);
      free(task->shade);
      task->zbuf   = NULL;
      task->shade  = NULL;
      task->ncells = 0;
      return result_OOM;
    }
  }
  zbuf  = task->zbuf;
  shade = task->shade;
  memset(zbuf, 0, ncells * sizeof(*zbuf));
  memset(shade, 0, ncells * sizeof(*shade));

  r1 = doughnut_tubes[task->tube].r1;
  k1 = width * DOUGHNUT_K2 * 3.0 / (8.0 * (r1 + DOUGHNUT_R2)) * task->zoom;

  dtheta = 2.0 * M_PI / doughnut_densities[task->minor].minor;
  dphi   = 2.0 * M_PI / doughnut_densities[task->major].major;

  for (theta = 0.0; theta < 2.0 * M_PI; theta += dtheta)
  {
    double costheta, sintheta;

    costheta = cos(theta);
    sintheta = sin(theta);

    for (phi = 0.0; phi < 2.0 * M_PI; phi += dphi)
    {
      int cell;

      if (!doughnut_project(r1, costheta, sintheta, phi, task->a, task->b,
                            width, height, k1, &x, &y, &z, &lum))
        continue;

      cell = y * width + x;
      if (z > zbuf[cell])
      {
        zbuf[cell] = z;
        /* darkest grey is 1, not 0: index 0 is the background */
        shade[cell] = (unsigned char) CLAMP((int) (lum * 255.0), 1, 255);
      }
    }
  }

  /* one clipped blit of the whole shade buffer, rather than a clip test per
   * pixel */
  rc = bitmap_init(&bm, SIZE2D(width, height), pixelfmt_p8, width,
                   task->palette, shade);
  if (rc != result_OK)
    return rc;

  return screen_copy_bitmap(scr, drawn.x0, drawn.y0, &bm);
}

/* radians of a/b rotation per pixel of Adjust drag, chosen to roughly match
 * the idle auto-rotation's feel (0.04/0.02 rad per tick) over a normal drag
 * speed */
#define DOUGHNUT_DRAG_SCALE 0.01

static result_t doughnut_mouse(doughnut_task_t    *task,
                               wuss_mouse_action_t action,
                               int                 x,
                               int                 y,
                               wuss_button_t       button,
                               wuss_window_t      *window)
{
  if (window != task->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  switch (action)
  {
  case wuss_MOUSE_DOWN:
    if (button & wuss_BUTTON_MENU)
    {
      wuss_menu_tick_exclusive(&task->tube_menu, task->tube);
      wuss_menu_tick_exclusive(&task->major_menu, task->major);
      wuss_menu_tick_exclusive(&task->minor_menu, task->minor);
      wuss_menu_tick_item(&task->menu, DOUGHNUT_MENU_PAUSE, task->paused);

      return wuss_menu_open_at_pointer(task->delegate, &task->menu,
                                       &task->menu_handle);
    }

    if (button & wuss_BUTTON_SELECT)
      task->paused = !task->paused;

    if (button & wuss_BUTTON_ADJUST)
    {
      task->dragging = 1;
      task->drag_x   = x;
      task->drag_y   = y;
    }
    break;

  case wuss_MOUSE_MOVE:
    if (!task->dragging)
      break;
    task->b += (x - task->drag_x) * DOUGHNUT_DRAG_SCALE;
    task->a += (y - task->drag_y) * DOUGHNUT_DRAG_SCALE;
    task->drag_x = x;
    task->drag_y = y;
    wuss_window_invalidate_visible(window);
    break;

  case wuss_MOUSE_UP:
    task->dragging = 0;
    break;
  }

  return result_OK;
}

static result_t doughnut_scroll(doughnut_task_t *task,
                                wuss_window_t   *window,
                                int              delta)
{
  if (window != task->window)
    return result_OK;

  task->zoom += (delta > 0 ? 1 : -1) * DOUGHNUT_ZOOM_STEP;
  task->zoom  = CLAMP(task->zoom, DOUGHNUT_ZOOM_MIN, DOUGHNUT_ZOOM_MAX);

  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

static result_t doughnut_idle(doughnut_task_t *task)
{
  if (task->window == NULL)
    return result_OK; /* window closed but the proginfo dialogue -- a second
                       * window on this same autoclose delegate -- is still
                       * open, keeping the task alive */

  if (task->paused || task->dragging)
    return result_OK; /* an Adjust drag is driving a/b directly */

  task->a += 0.04;
  task->b += 0.02;

  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

/* set *field (task->major, ->minor or ->tube) to index, clamped below
 * count, refreshing the live menu tick and the picture */
static result_t doughnut_density_set(doughnut_task_t *task,
                                     int             *field,
                                     wuss_menu_t     *menu,
                                     int              count,
                                     int              index)
{
  *field = CLAMP(index, 0, count - 1);

  if (task->menu_handle != NULL)
    wuss_menu_tick_exclusive_live(task->menu_handle, menu, *field);
  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

/* '[' ']' step the major density, ',' '.' the minor and '-' '+' the tube,
 * spinning or not. While paused, the arrow keys step the rotation: Up/Down about x (a),
 * Left/Right about z (b). Anything else, or an arrow while spinning, is
 * passed back unclaimed. */
static result_t doughnut_key(doughnut_task_t *task, int code)
{
  switch (code)
  {
  case '[': return doughnut_density_set(task, &task->major, &task->major_menu,
                                         NELEMS(doughnut_densities),
                                         task->major - 1);
  case ']': return doughnut_density_set(task, &task->major, &task->major_menu,
                                         NELEMS(doughnut_densities),
                                         task->major + 1);
  case ',': return doughnut_density_set(task, &task->minor, &task->minor_menu,
                                         NELEMS(doughnut_densities),
                                         task->minor - 1);
  case '.': return doughnut_density_set(task, &task->minor, &task->minor_menu,
                                         NELEMS(doughnut_densities),
                                         task->minor + 1);
  case '-': return doughnut_density_set(task, &task->tube, &task->tube_menu,
                                         NELEMS(doughnut_tubes),
                                         task->tube - 1);
  case '=':
  case '+': return doughnut_density_set(task, &task->tube, &task->tube_menu,
                                         NELEMS(doughnut_tubes),
                                         task->tube + 1);
  default:  break;
  }

  if (!task->paused)
    return result_WUSS_KEY_UNCLAIMED;

  switch (code)
  {
  case wuss_KEY_UP:    task->a -= DOUGHNUT_KEY_STEP; break;
  case wuss_KEY_DOWN:  task->a += DOUGHNUT_KEY_STEP; break;
  case wuss_KEY_LEFT:  task->b -= DOUGHNUT_KEY_STEP; break;
  case wuss_KEY_RIGHT: task->b += DOUGHNUT_KEY_STEP; break;
  default:             return result_WUSS_KEY_UNCLAIMED;
  }

  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

static result_t doughnut_menu_select(doughnut_task_t    *task,
                                     const wuss_event_t *event)
{
  if (event->data.menu_select.menu == &task->menu &&
      event->data.menu_select.index == DOUGHNUT_MENU_PAUSE)
  {
    task->paused = !task->paused;
    wuss_menu_tick_item_live(task->menu_handle, &task->menu,
                             DOUGHNUT_MENU_PAUSE, task->paused);
    return result_OK;
  }

  if (event->data.menu_select.menu == &task->menu &&
      (event->data.menu_select.index == DOUGHNUT_MENU_RANDOMISE ||
       event->data.menu_select.index == DOUGHNUT_MENU_RESET))
  {
    if (event->data.menu_select.index == DOUGHNUT_MENU_RANDOMISE)
      doughnut_randomise(task);
    else
      doughnut_defaults(task);

    if (task->menu_handle != NULL)
    {
      wuss_menu_tick_exclusive_live(task->menu_handle, &task->tube_menu,
                                    task->tube);
      wuss_menu_tick_exclusive_live(task->menu_handle, &task->major_menu,
                                    task->major);
      wuss_menu_tick_exclusive_live(task->menu_handle, &task->minor_menu,
                                    task->minor);
    }
    wuss_window_invalidate_visible(task->window);
    return result_OK;
  }

  if (event->data.menu_select.menu == &task->menu &&
      event->data.menu_select.index == DOUGHNUT_MENU_SAVE)
  {
    return wuss_saveas_open(task->saveas);
  }

  if (event->data.menu_select.menu == &task->major_menu)
    return doughnut_density_set(task, &task->major, &task->major_menu,
                                NELEMS(doughnut_densities),
                                event->data.menu_select.index);

  if (event->data.menu_select.menu == &task->minor_menu)
    return doughnut_density_set(task, &task->minor, &task->minor_menu,
                                NELEMS(doughnut_densities),
                                event->data.menu_select.index);

  if (event->data.menu_select.menu == &task->tube_menu)
    return doughnut_density_set(task, &task->tube, &task->tube_menu,
                                NELEMS(doughnut_tubes),
                                event->data.menu_select.index);

  if (wuss_colourlist_selected(&task->colours, event))
  {
    doughnut_palette_build(task);
    wuss_window_invalidate_visible(task->window);
  }

  return result_OK;
}

/* Every submenu leaf fires wuss_EVENT_PRE_SUBMENU_OPEN -- "Tube" too, which
 * opens unchanged. */
static result_t doughnut_pre_submenu_open(doughnut_task_t    *task,
                                          const wuss_event_t *event)
{
  result_t rc;
  int      index;

  if (wuss_colourlist_pre_open(&task->colours, task->wuss, event, &rc))
    return rc;

  index = event->data.pre_submenu_open.index;

  return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle, index,
                                    task->menu_items[index].submenu);
}

result_t doughnut_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  doughnut_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return doughnut_redraw(event, task);

  case wuss_EVENT_MOUSE:
    return doughnut_mouse(task, event->data.mouse.action,
                       event->data.mouse.point.x, event->data.mouse.point.y,
                       event->data.mouse.button, window);

  case wuss_EVENT_SCROLL:
    return doughnut_scroll(task, window, event->data.scroll.delta);

  case wuss_EVENT_IDLE:
    return doughnut_idle(task);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != task->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(task->delegate, &task->menu, event, &rc))
      return rc;

    return doughnut_key(task, event->data.key.code);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return doughnut_pre_submenu_open(task, event);

  case wuss_EVENT_MENU_SELECT:
    return doughnut_menu_select(task, event);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == task->window)
      task->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    doughnut_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
