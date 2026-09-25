/* wuss/test/tasks/particles.c -- particle explosion task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"

#include "particles.h"

#define PARTICLES_BURST (MAX_PARTICLES / 2) /* particles per click */

/* MENU click pops this single-item menu; the item table and wuss_menu_t
 * live per-instance in particles_task_t, not as a file-scope static, so
 * that each window's Info row can hold its own .window pointer to the
 * shared proginfo singleton, retargeted just before wuss_menu_open */
enum { PARTICLES_MENU_INFO };

/* styles, in the order particles_init_styles sets them up */
enum
{
  PARTICLES_FIREY,
  PARTICLES_SMOKEY,
  PARTICLES_FLECK,
  PARTICLES_PASTEL
};

/* ----------------------------------------------------------------------- */

/* a gradient colour stop; each ramp runs from stop 0.0 to stop 1.0 */
typedef struct
{
  unsigned char r, g, b;
  float         stop;
}
particles_stop_t;

static const particles_stop_t particles_firey[] =
{
  { 255, 255, 255, 0.0f }, /* white */
  { 255, 232,   8, 0.1f }, /* yellow */
  { 255, 206,   0, 0.2f }, /* yellow-orange */
  { 255, 154,   0, 0.5f }, /* orange */
  { 255,  90,   0, 0.6f }, /* red */
  {   0,   0, 127, 1.0f }  /* dark blue */
};

static const particles_stop_t particles_smokey[] =
{
  { 255, 154,   0, 0.0f }, /* orange */
  { 127, 127, 127, 0.4f }, /* mid grey */
  {  31,  31,  31, 0.9f }, /* dark grey */
  {   0,   0,   0, 1.0f }  /* black */
};

static const particles_stop_t particles_fleck[] =
{
  { 255, 255, 255, 0.0f }, /* white */
  { 255, 255,   0, 0.2f }, /* yellow */
  {   0, 255,   0, 0.3f }, /* green */
  {   0, 127,   0, 0.5f }, /* dark green */
  {   0,   0, 127, 0.9f }, /* dark blue */
  {   0,   0,   0, 1.0f }  /* black */
};

static const particles_stop_t particles_pastel[] =
{
  { 251, 243, 185, 0.0f }, /* lemon */
  { 255, 220, 204, 0.3f }, /* peach */
  { 253, 183, 234, 0.7f }, /* pink */
  { 183, 177, 242, 1.0f }  /* mauve */
};

/* fill palette[0..PALETTE_SIZE) by sampling the stops evenly */
static void particles_ramp(const particles_stop_t *stops, colour_t *palette)
{
  int                     i;
  float                   t;
  const particles_stop_t *a, *b;
  float                   u;

  for (i = 0; i < PALETTE_SIZE; i++)
  {
    t = (float) i / (PALETTE_SIZE - 1);

    for (a = stops; t > a[1].stop; a++)
      ;
    b = a + 1;
    u = (t - a->stop) / (b->stop - a->stop);

    palette[i] = colour_rgb((unsigned int) (a->r + (b->r - a->r) * u),
                            (unsigned int) (a->g + (b->g - a->g) * u),
                            (unsigned int) (a->b + (b->b - a->b) * u));
  }
}

/* the same style set as Explosion's own playground */
static void particles_init_styles(particle_style_t *styles)
{
  float frame_ms;

  frame_ms = 1000.0f / PHYSICS_FPS;

  set_default_style(&styles[PARTICLES_FIREY], frame_ms);
  styles[PARTICLES_FIREY].probability    = 90;
  styles[PARTICLES_FIREY].palette_index  = PARTICLES_FIREY;
  styles[PARTICLES_FIREY].emit_angle     = 270.0f;
  styles[PARTICLES_FIREY].emit_range     = 90.0f;

  set_default_style(&styles[PARTICLES_SMOKEY], frame_ms);
  styles[PARTICLES_SMOKEY].probability   = 8;
  styles[PARTICLES_SMOKEY].palette_index = PARTICLES_SMOKEY;
  styles[PARTICLES_SMOKEY].emit_angle    = 270.0f;
  styles[PARTICLES_SMOKEY].emit_range    = 90.0f;
  styles[PARTICLES_SMOKEY].min_life     *= 4;
  styles[PARTICLES_SMOKEY].max_life     *= 4;
  styles[PARTICLES_SMOKEY].vel_scale     = 0.1f;
  styles[PARTICLES_SMOKEY].emit_speed    = 10;
  styles[PARTICLES_SMOKEY].min_size      = 1;
  styles[PARTICLES_SMOKEY].max_size      = 2;
  styles[PARTICLES_SMOKEY].gravity      /= -100.0f;

  set_default_style(&styles[PARTICLES_FLECK], frame_ms);
  styles[PARTICLES_FLECK].probability    = 2;
  styles[PARTICLES_FLECK].palette_index  = PARTICLES_FLECK;
  styles[PARTICLES_FLECK].emit_speed     = 200;

  set_default_style(&styles[PARTICLES_PASTEL], frame_ms);
  styles[PARTICLES_PASTEL].probability   = 0;
  styles[PARTICLES_PASTEL].palette_index = PARTICLES_PASTEL;
  styles[PARTICLES_PASTEL].min_life     /= 2;
  styles[PARTICLES_PASTEL].max_life     /= 2;
  styles[PARTICLES_PASTEL].emit_speed    = 25;
  styles[PARTICLES_PASTEL].gravity      /= 2.0f;
}

/* ----------------------------------------------------------------------- */

/* engine callbacks */

static unsigned int particles_rand(int nbits, void *opaque)
{
  particles_task_t *pt;
  uint32_t          r;

  pt = opaque;

  r = rng_xorshift32(&pt->rng);
  return (nbits >= 32) ? r : r & ((1u << nbits) - 1);
}

static unsigned int particles_time(void *opaque)
{
  particles_task_t *pt;

  pt = opaque;

  return pt->now_ms;
}

/* a filled square centred on (x,y), as Explosion's playground draws it */
static void particles_render(int   x,
                             int   y,
                             int   size,
                             int   palette_index,
                             void *opaque)
{
  particles_task_t *pt;

  pt = opaque;

  screen_fill_rect(pt->scr,
                   pt->ox + x - size / 2,
                   pt->oy + y - size / 2,
                   SIZE2D(size, size),
                   pt->palette[palette_index]);
}

/* ----------------------------------------------------------------------- */

result_t particles_create(wuss_t *wuss, particles_task_t **out)
{
  result_t          rc;
  particles_task_t *task;
  wuss_task_t      *delegate;
  wuss_task_desc_t  delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  rng_seed(&task->rng, (uint32_t) rand());

  particles_ramp(particles_firey,  &task->palette[PALETTE_SIZE * PARTICLES_FIREY]);
  particles_ramp(particles_smokey, &task->palette[PALETTE_SIZE * PARTICLES_SMOKEY]);
  particles_ramp(particles_fleck,  &task->palette[PALETTE_SIZE * PARTICLES_FLECK]);
  particles_ramp(particles_pastel, &task->palette[PALETTE_SIZE * PARTICLES_PASTEL]);

  particles_init_styles(task->styles);
  init_particle_system(&task->ps,
                       0,
                       task->styles,
                       PARTICLES_NSTYLES,
                       0.2f,
                       particles_rand,
                       particles_time,
                       particles_render,
                       task);

  /* particles_redraw paints its own background every frame */
  delegate_desc.handle    = particles_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "particles";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(WIDTH, HEIGHT),
                                 "Particles",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(WIDTH, HEIGHT),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, PARTICLES_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * particles_mouse */

  WUSS_MENU_TITLE(task->menu, "Particles", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void particles_destroy(particles_task_t *task)
{
  wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t particles_redraw(const wuss_event_t *event, void *task_data)
{
  particles_task_t *pt;
  const box_t      *content, *bounds;

  pt = task_data;

  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  pt->scr = event->data.redraw.scr;
  pt->ox  = bounds->x0 - event->data.redraw.scroll.x;
  pt->oy  = bounds->y0 - event->data.redraw.scroll.y;

  screen_fill_rect(pt->scr, content->x0, content->y0, box_size(content),
                   colour_rgb(0x00, 0x00, 0x00));

  render_particles(&pt->ps);

  pt->scr = NULL;

  return result_OK;
}

static result_t particles_mouse(wuss_window_t      *window,
                                wuss_mouse_action_t action,
                                int                 x,
                                int                 y,
                                wuss_button_t       button,
                                void               *task_data)
{
  particles_task_t *pt;

  pt = task_data;

  if (window != pt->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_MENU)
  {
    static const wuss_proginfo_desc_t desc =
    {
      "Particles",
      "Retro explosion particle system",
      "© David Thomas",
      "1.0 (" __DATE__ ")"
    };
    wuss_proginfo_set_desc(&desc);
    pt->menu_items[PARTICLES_MENU_INFO].window =
      wuss_proginfo_window(pt->delegate);

    return wuss_menu_open(pt->delegate, &pt->menu,
                          wuss_get_pointer(pt->wuss), &pt->menu_handle);
  }

  /* x,y arrive in virtual content space, as the particles are held */
  if (button & wuss_BUTTON_SELECT)
    create_explosion(&pt->ps, -1, x, y, 0.0f, 0.0f, PARTICLES_BURST);
  else if (button & wuss_BUTTON_ADJUST)
    create_explosion(&pt->ps, PARTICLES_FLECK, x, y, 0.0f, 0.0f,
                     PARTICLES_BURST);

  return result_OK;
}

static result_t particles_idle(void *task_data)
{
  particles_task_t *pt;
  box_t             content;

  pt = task_data;

  /* the proginfo dialogue is a second window on this same (autoclose)
   * delegate, so closing the main window alone never empties task->windows
   * and the task lingers until the dialogue closes too -- guard against the
   * dangling window in the meantime */
  if (pt->window == NULL)
    return result_OK;

  /* particles die on leaving the bounds, so track the window's size */
  wuss_window_get_content_bounds(pt->window, &content);
  pt->ps.width  = content.x1 - content.x0;
  pt->ps.height = content.y1 - content.y0;

  /* ponytail: one fixed physics step per idle tick, so speed follows the
   * frame rate; feed a real clock's delta here if that ever matters */
  pt->now_ms += 1000 / PHYSICS_FPS;
  update_particles(&pt->ps, 1.0f / PHYSICS_FPS);

  if (!is_active(&pt->ps) && pt->ps.width > 0 && pt->ps.height > 0)
    create_explosion(&pt->ps, -1,
                     (int) (particles_rand(16, pt) % pt->ps.width),
                     (int) (particles_rand(16, pt) % pt->ps.height),
                     0.0f, 0.0f, PARTICLES_BURST);

  wuss_window_invalidate_visible(pt->window);

  return result_OK;
}

result_t particles_handle(wuss_window_t      *window,
                          const wuss_event_t *event,
                          void               *task_data)
{
  particles_task_t *pt;

  pt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return particles_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return particles_mouse(window, event->data.mouse.action,
                           event->data.mouse.point.x,
                           event->data.mouse.point.y,
                           event->data.mouse.button, task_data);

  case wuss_EVENT_IDLE:
    return particles_idle(task_data);

  case wuss_EVENT_MENU_CLOSED:
    pt->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == pt->window)
      pt->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == pt->menu_items[PARTICLES_MENU_INFO].window)
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
    particles_destroy(pt);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
