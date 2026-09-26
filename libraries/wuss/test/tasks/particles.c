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

/* pointer trail, as Explosion's playground */
#define PARTICLES_TRAIL_RATE    5.0f  /* particles per second */
#define PARTICLES_TRAIL_MAX     10    /* cap per idle tick */
#define PARTICLES_TRAIL_DAMPING 0.25f /* fraction of pointer velocity kept */

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in particles_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  PARTICLES_MENU_INFO,
  PARTICLES_MENU_ADD_EMITTER,
  PARTICLES_MENU_BACKGROUND,
  PARTICLES_MENU_PAUSE,
  PARTICLES_MENU_GRAVITY,
  PARTICLES_MENU_WALLS
};

/* "Gravity" submenu rows: each scales every style's own gravity */
static const struct
{
  const char *name;
  float       scale;
}
particles_gravities[] =
{
  { "Off",       0.0f },
  { "Low",       0.5f },
  { "Normal",    1.0f },
  { "High",      2.0f },
  { "Reversed", -1.0f }
};

#define PARTICLES_GRAVITY_NORMAL 2

/* "Add emitter" submenu rows; picking one adds an emitter at that intensity.
 * wuss never picks a row that has a submenu, so the intensity rows are
 * what add the emitter rather than the "Add emitter" row itself. */
static const struct
{
  const char *name;
  float       rate; /* particles per second */
}
particles_intensities[] =
{
  { "Low",     5.0f },
  { "Medium", 20.0f },
  { "High",   80.0f }
};

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

  screen_fill_rect_value(pt->scr,
                         pt->ox + x - size / 2,
                         pt->oy + y - size / 2,
                         SIZE2D(size, size),
                         pt->pixels[palette_index]);
}

/* ----------------------------------------------------------------------- */

result_t particles_create(wuss_t *wuss, particles_task_t **out)
{
  result_t          rc;
  particles_task_t *task;
  wuss_task_t      *delegate;
  wuss_task_desc_t  delegate_desc;
  int               i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->bg   = colour_rgb(0x00, 0x00, 0x00);
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

  for (i = 0; i < NELEMS(task->emitter_items); i++)
    WUSS_MENU_ITEM(task->emitter_items, i, particles_intensities[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->emitter_menu, "Intensity", task->emitter_items,
                 NELEMS(task->emitter_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, PARTICLES_MENU_ADD_EMITTER,
                      "Add emitter", wuss_MENU_ITEM_NONE, &task->emitter_menu);

  WUSS_MENU_ITEM_MENU(task->menu_items, PARTICLES_MENU_BACKGROUND, "Background",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  WUSS_MENU_ITEM(task->menu_items, PARTICLES_MENU_PAUSE, "Pause",
                 wuss_MENU_ITEM_NONE);

  task->gravity = PARTICLES_GRAVITY_NORMAL;
  for (i = 0; i < NELEMS(task->gravity_items); i++)
    WUSS_MENU_ITEM(task->gravity_items, i, particles_gravities[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->gravity_menu, "Gravity", task->gravity_items,
                 NELEMS(task->gravity_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, PARTICLES_MENU_GRAVITY, "Gravity",
                      wuss_MENU_ITEM_NONE, &task->gravity_menu);

  WUSS_MENU_ITEM(task->menu_items, PARTICLES_MENU_WALLS, "Walls",
                 wuss_MENU_ITEM_NONE);

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
  int               i;

  pt = task_data;

  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  pt->scr = event->data.redraw.scr;
  pt->ox  = bounds->x0 - event->data.redraw.scroll.x;
  pt->oy  = bounds->y0 - event->data.redraw.scroll.y;

  /* resolve the palette once per redraw rather than once per particle; the
   * screen's own palette can change between redraws */
  for (i = 0; i < (int) NELEMS(pt->pixels); i++)
    pt->pixels[i] = screen_colour_to_pixel(pt->scr, pt->palette[i]);

  screen_fill_rect(pt->scr, content->x0, content->y0, box_size(content),
                   pt->bg);

  render_particles(&pt->ps);

  pt->scr = NULL;

  return result_OK;
}

/* note the pointer's position and velocity for particles_trail */
static void particles_track(particles_task_t *pt, int x, int y)
{
  float dt;

  dt = (pt->now_ms - pt->last_move_ms) / 1000.0f;
  if (pt->pointer_in && dt > 0.0f)
  {
    pt->mvx = (x - pt->mx) / dt;
    pt->mvy = (y - pt->my) / dt;
  }
  else if (!pt->pointer_in)
  {
    /* first move since entering: no velocity yet, and no backlog of trail
     * particles owed for the time spent outside */
    pt->mvx          = 0.0f;
    pt->mvy          = 0.0f;
    pt->last_emit_ms = pt->now_ms;
    pt->pointer_in   = 1;
  }

  pt->mx           = x;
  pt->my           = y;
  pt->last_move_ms = pt->now_ms;
}

/* emit the pointer trail, carrying some of the pointer's velocity */
static void particles_trail(particles_task_t *pt)
{
  float emit_dt;
  int   n;

  if (!pt->pointer_in)
    return;

  emit_dt = (pt->now_ms - pt->last_emit_ms) / 1000.0f;
  n       = CLAMP((int) (emit_dt * PARTICLES_TRAIL_RATE), 0,
                  PARTICLES_TRAIL_MAX);
  if (n == 0)
    return;

  while (n-- > 0)
    create_particle(&pt->ps, PARTICLES_PASTEL, pt->mx, pt->my,
                    pt->mvx * PARTICLES_TRAIL_DAMPING,
                    pt->mvy * PARTICLES_TRAIL_DAMPING);
  pt->last_emit_ms = pt->now_ms;
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

  /* x,y arrive in virtual content space, as the particles are held */
  if (action == wuss_MOUSE_MOVE)
  {
    particles_track(pt, x, y);
    return result_OK;
  }

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

    pt->menu_x = x;
    pt->menu_y = y;

    wuss_menu_tick_item(&pt->menu, PARTICLES_MENU_PAUSE, pt->paused);
    wuss_menu_tick_exclusive(&pt->gravity_menu, pt->gravity);
    wuss_menu_tick_item(&pt->menu, PARTICLES_MENU_WALLS,
                        !!(pt->ps.flags & PARTICLE_FLAG_WALLS));

    return wuss_menu_open(pt->delegate, &pt->menu,
                          wuss_get_pointer(pt->wuss), &pt->menu_handle);
  }

  if (button & wuss_BUTTON_SELECT)
    create_explosion(&pt->ps, -1, x, y, 0.0f, 0.0f, PARTICLES_BURST);
  else if (button & wuss_BUTTON_ADJUST)
    create_explosion(&pt->ps, PARTICLES_FLECK, x, y, 0.0f, 0.0f,
                     PARTICLES_BURST);

  return result_OK;
}

/* The "Background" row's submenu: the shared colourmenu singleton,
 * reconfigured here rather than at create time since other tasks retitle it
 * and toggle its None row too. */
static result_t particles_pre_submenu_open(particles_task_t   *pt,
                                           const wuss_event_t *event)
{
  const wuss_menu_t *menu;

  menu = wuss_colourmenu_menu(pt->wuss);
  wuss_colourmenu_set_none(0);
  wuss_colourmenu_set_title("Background");

  return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle,
                                    event->data.pre_submenu_open.index,
                                    menu);
}

/* a Background pick sets the fill; a "Gravity" pick sets the strength; an
 * "Add emitter" submenu pick adds a steady smoke emitter, as Explosion's
 * playground sets up, at the menu's opening point */
/* A "Gravity" pick: rebuild the styles from scratch, then scale each one's
 * own gravity, so strengths never compound. Particles already in flight pick
 * up the change on the next physics step, as the engine reads gravity from
 * the style each step. */
static void particles_set_gravity(particles_task_t   *pt,
                                  const wuss_event_t *event)
{
  int i;

  pt->gravity = event->data.menu_select.index;

  particles_init_styles(pt->styles);
  for (i = 0; i < PARTICLES_NSTYLES; i++)
    pt->styles[i].gravity *= particles_gravities[pt->gravity].scale;

  if (wuss_menu_should_keep_open(event))
    wuss_menu_tick_exclusive_live(pt->menu_handle, &pt->gravity_menu,
                                  pt->gravity);
}

static result_t particles_menu_select(particles_task_t   *pt,
                                      const wuss_event_t *event)
{
  wuss_colour_t   picked;
  int             mine;
  const colour_t *palette;
  int             npalette;
  int             index;

  picked = wuss_colourmenu_selected(event, &mine);
  if (mine)
  {
    palette = wuss_get_palette(pt->wuss, &npalette);
    if (picked < npalette)
      pt->bg = palette[picked]; /* the next idle tick repaints */
    return result_OK;
  }

  if (event->data.menu_select.menu == &pt->gravity_menu)
  {
    particles_set_gravity(pt, event);
    return result_OK;
  }

  if (event->data.menu_select.menu != &pt->emitter_menu)
    return result_OK;

  index = event->data.menu_select.index;
  if (index < 0 || index >= NELEMS(particles_intensities))
    return result_OK;

  create_emitter(&pt->ps, pt->menu_x, pt->menu_y,
                 particles_intensities[index].rate,
                 0.5f, /* jitter */
                 0.0f, /* clump: none */
                 PARTICLES_SMOKEY,
                 0); /* lifetime: forever */

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

  if (pt->paused)
    return result_OK;

  /* particles die on leaving the bounds, so track the window's size */
  wuss_window_get_content_bounds(pt->window, &content);
  pt->ps.width  = content.x1 - content.x0;
  pt->ps.height = content.y1 - content.y0;

  /* ponytail: one fixed physics step per idle tick, so speed follows the
   * frame rate; feed a real clock's delta here if that ever matters */
  pt->now_ms += 1000 / PHYSICS_FPS;
  update_particles(&pt->ps, 1.0f / PHYSICS_FPS);
  particles_trail(pt);

  if (!is_active(&pt->ps) && pt->ps.width > 0 && pt->ps.height > 0)
    create_explosion(&pt->ps, -1,
                     (int) (particles_rand(16, pt) % pt->ps.width),
                     (int) (particles_rand(16, pt) % pt->ps.height),
                     0.0f, 0.0f, PARTICLES_BURST);

  wuss_window_invalidate_visible(pt->window);

  return result_OK;
}

/* The "Pause" and "Walls" rows: Pause stops or restarts the idle animation;
 * Walls flips the engine's own flag, which takes effect on the next physics
 * step. An ADJUST pick keeps the menu open, so retick the
 * live row; a SELECT pick has already closed it. */
static result_t particles_toggle(particles_task_t   *pt,
                                 const wuss_event_t *event)
{
  int index;
  int ticked;

  index = event->data.menu_select.index;

  switch (index)
  {
  case PARTICLES_MENU_PAUSE:
    pt->paused = !pt->paused;
    ticked = pt->paused;
    break;

  case PARTICLES_MENU_WALLS:
    pt->ps.flags ^= PARTICLE_FLAG_WALLS;
    ticked = !!(pt->ps.flags & PARTICLE_FLAG_WALLS);
    break;

  default:
    return result_OK;
  }

  if (wuss_menu_should_keep_open(event))
    wuss_menu_tick_item_live(pt->menu_handle, &pt->menu, index, ticked);

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

  case wuss_EVENT_POINTER_EXIT:
    if (window == pt->window)
      pt->pointer_in = 0;
    return result_OK;

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return particles_pre_submenu_open(pt, event);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &pt->menu)
      return particles_toggle(pt, event);
    return particles_menu_select(pt, event);

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
