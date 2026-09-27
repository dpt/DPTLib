/* wuss/test/tasks/ball.c -- bouncing ball task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "ball.h"

#define BALL_BASE_RADIUS 8 /* +/-50% at spawn -> 4..12 */
#define BALL_MAX_DY     16 /* cap on vertical speed under gravity */

/* MENU click pops this menu; the item table and wuss_menu_t live per-instance
 * in ball_task_t, not as a file-scope static, so that each window's Info row
 * can hold its own .window pointer to the shared proginfo singleton, retargeted
 * just before wuss_menu_open */
enum
{
  BALL_MENU_INFO = 0,
  BALL_MENU_BACKGROUND,
  BALL_MENU_PAUSE,
  BALL_MENU_CLEAR,
  BALL_MENU_GRAVITY
};

/* a fresh radius in [BALL_BASE_RADIUS/2, BALL_BASE_RADIUS*3/2] */
static int ball_random_radius(void)
{
  return BALL_BASE_RADIUS / 2 + rand() % (BALL_BASE_RADIUS + 1);
}

/* a fresh fully-opaque colour, red channel is at least 0x40 so it stays
 * visible against the red background */
static colour_t ball_random_colour(void)
{
  int i;
  
  i = 0x40 + rand() % 0xC0;
  return colour_rgb(0xFF, i, i);
}

/* Invalidation box (virtual content space, as ball positions are held)
 * covering a ball -- or its swept range -- whose centre spans
 * [vx0,vx1] x [vy0,vy1]. wuss_window_invalidate maps this to the screen and
 * applies scroll, so this must not. The circle occupies [c-radius,c+radius]
 * inclusive, i.e. 2*radius+1 pixels; box_t is half-open so x1/y1 get the
 * extra 1. */
static box_t ball_local_box(int vx0, int vy0, int vx1, int vy1, int radius)
{
  box_t local;

  local.x0 = vx0 - radius;
  local.y0 = vy0 - radius;
  local.x1 = vx1 + radius + 1;
  local.y1 = vy1 + radius + 1;

  return local;
}

result_t ball_create(wuss_t *wuss, ball_task_t **out)
{
  result_t         rc;
  ball_task_t     *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss   = wuss;
  task->bg     = colour_rgb(0xFF, 0x00, 0x00);
  task->nballs = 1;

  task->balls[0].x      = 50;
  task->balls[0].y      = 50;
  task->balls[0].dx     = 3;
  task->balls[0].dy     = 2;
  task->balls[0].radius = ball_random_radius();
  task->balls[0].colour = colour_rgb(0xFF, 0xFF, 0xFF); /* white */

  /* ball_redraw paints its own background every frame */
  delegate_desc.handle    = ball_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "ball";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(200, 160),
                                 "Bouncing Ball",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(200, 160),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, BALL_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in ball_mouse */

  WUSS_MENU_ITEM_MENU(task->menu_items, BALL_MENU_BACKGROUND, "Background",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  WUSS_MENU_ITEM(task->menu_items, BALL_MENU_PAUSE, "Pause",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_ITEM(task->menu_items, BALL_MENU_CLEAR, "Clear",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_ITEM(task->menu_items, BALL_MENU_GRAVITY, "Gravity",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->menu, "Bouncing Ball", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void ball_destroy(ball_task_t *task)
{
  wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t ball_redraw(const wuss_event_t *event, void *task_data)
{
  ball_task_t *bc;
  screen_t    *scr;
  const box_t *content, *bounds;
  int          sx, sy;
  int          i;

  bc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content),
                   bc->bg);

  for (i = 0; i < bc->nballs; i++)
  {
    const ball_t *b;

    b = &bc->balls[i];

    screen_fill_circle(scr, bounds->x0 - sx + b->x,
                            bounds->y0 - sy + b->y,
                            b->radius,
                            b->colour);
  }

  return result_OK;
}

/* match the window and menu titles to the ball count */
static void ball_set_title(ball_task_t *bc)
{
  const char *title;

  title = (bc->nballs > 1) ? "Bouncing Balls" : "Bouncing Ball";
  wuss_window_set_title(bc->window, title);
  bc->menu.title = title; /* read on the next wuss_menu_open */
}

static result_t ball_mouse(wuss_window_t      *window,
                           wuss_mouse_action_t action,
                           int                 x,
                           int                 y,
                           wuss_button_t       button,
                           void               *task_data)
{
  ball_task_t *bc;
  box_t        local;

  bc = task_data;

  if (window != bc->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_MENU)
  {
    /* static: proginfo only copies the fields when it is next shown */
    static const wuss_proginfo_desc_t desc[2] =
    {
      {
        "Bouncing Ball",
        "Balls bouncing off content edges",
        "© DPTLib contributors",
        "1.0 (" __DATE__ ")"
      },
      {
        "Bouncing Balls",
        "Balls bouncing off content edges",
        "© DPTLib contributors",
        "1.0 (" __DATE__ ")"
      }
    };
    wuss_proginfo_set_desc(&desc[bc->nballs > 1]);
    bc->menu_items[BALL_MENU_INFO].window = wuss_proginfo_window(bc->delegate);

    wuss_menu_tick_item(&bc->menu, BALL_MENU_PAUSE, bc->paused);
    wuss_menu_tick_item(&bc->menu, BALL_MENU_GRAVITY, bc->gravity);

    /* Clear has nothing to do while only the first ball remains */
    if (bc->nballs > 1)
      bc->menu_items[BALL_MENU_CLEAR].flags &= ~wuss_MENU_ITEM_DISABLED;
    else
      bc->menu_items[BALL_MENU_CLEAR].flags |= wuss_MENU_ITEM_DISABLED;

    return wuss_menu_open(bc->delegate, &bc->menu,
                          wuss_get_pointer(bc->wuss), &bc->menu_handle);
  }

  if (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST))
  {
    ball_t *b;

    if (button & wuss_BUTTON_SELECT)
    {
      if (bc->nballs >= BALL_MAX)
        return result_OK;
      
      /* x,y already arrive in virtual content space, as ball positions are
       * held; only the invalidation boxes below need the scroll offset taking
       * back off to reach window-local coordinates. */
      b         = &bc->balls[bc->nballs++];
      b->x      = x;
      b->y      = y;
      b->dx     = (bc->nballs & 1) ? 3 : -3;
      b->dy     = (bc->nballs & 2) ? 2 : -2;
      b->radius = ball_random_radius();
      b->colour = ball_random_colour();
    }
    else if (button & wuss_BUTTON_ADJUST)
    {
      if (bc->nballs <= 1)
        return result_OK; /* keep at least one ball on screen */
      
      b = &bc->balls[--bc->nballs];
    }
    
    local = ball_local_box(b->x, b->y, b->x, b->y, b->radius);
    wuss_window_invalidate(bc->window, &local);

    ball_set_title(bc);
  }

  return result_OK;
}

static result_t ball_idle(void *task_data)
{
  ball_task_t *bc;
  box_t        content;
  point_t      scroll;
  int          width, height;
  int          i;

  bc = task_data;

  /* the proginfo dialogue is a second window on this same (autoclose)
   * delegate, so closing the ball window alone never empties task->windows
   * and the task lingers until the dialogue closes too -- guard against the
   * dangling window in the meantime */
  if (bc->window == NULL)
    return result_OK;

  if (bc->paused)
    return result_OK;

  wuss_window_get_content_bounds(bc->window, &content);
  wuss_window_get_scroll(bc->window, &scroll);
  width  = content.x1 - content.x0;
  height = content.y1 - content.y0;

  for (i = 0; i < bc->nballs; i++)
  {
    ball_t *b;
    box_t   local;
    int     old_x, old_y;

    b = &bc->balls[i];

    old_x = b->x;
    old_y = b->y;

    if (bc->gravity)
      b->dy = MIN(b->dy + 1, BALL_MAX_DY);

    b->x += b->dx;
    b->y += b->dy;

    if (b->x - b->radius < scroll.x)               { b->x = scroll.x + b->radius;              b->dx = -b->dx; }
    else if (b->x + b->radius >= scroll.x + width) { b->x = scroll.x + width - 1 - b->radius;  b->dx = -b->dx; }
    if (b->y - b->radius < scroll.y)               { b->y = scroll.y + b->radius;              b->dy = -b->dy; }
    else if (b->y + b->radius >= scroll.y + height){ b->y = scroll.y + height - 1 - b->radius; b->dy = -b->dy; }

    local = ball_local_box(MIN(old_x, b->x), MIN(old_y, b->y),
                           MAX(old_x, b->x), MAX(old_y, b->y),
                           b->radius);

    wuss_window_invalidate(bc->window, &local);
  }

  return result_OK;
}

/* The "Background" row's submenu: the shared colourmenu singleton,
 * reconfigured here rather than at create time since other tasks retitle it
 * and toggle its None row too. */
static result_t ball_pre_submenu_open(ball_task_t        *bc,
                                      const wuss_event_t *event)
{
  const wuss_menu_t *menu;

  menu = wuss_colourmenu_menu(bc->wuss);
  wuss_colourmenu_set_none(0);
  wuss_colourmenu_set_title("Background");

  return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle,
                                    event->data.pre_submenu_open.index,
                                    menu);
}

static result_t ball_menu_select(ball_task_t *bc, const wuss_event_t *event)
{
  const colour_t *palette;
  int             npalette;
  wuss_colour_t   picked;
  int             mine;

  picked = wuss_colourmenu_selected(event, &mine);
  if (!mine)
    return result_OK;

  palette = wuss_get_palette(bc->wuss, &npalette);
  if (picked < npalette)
  {
    bc->bg = palette[picked];
    if (bc->window != NULL)
      wuss_window_invalidate_visible(bc->window);
  }

  return result_OK;
}

/* The "Pause" row stops or restarts the idle animation; the "Gravity" row
 * pulls the balls downward each tick. An ADJUST pick keeps the menu open,
 * so retick the live row; a SELECT pick has already closed it. */
static result_t ball_toggle(ball_task_t *bc, const wuss_event_t *event)
{
  int  index;
  int *flag;

  index = event->data.menu_select.index;
  flag  = (index == BALL_MENU_PAUSE) ? &bc->paused : &bc->gravity;
  *flag = !*flag;

  if (wuss_menu_should_keep_open(event))
    wuss_menu_tick_item_live(bc->menu_handle, &bc->menu, index, *flag);

  return result_OK;
}

/* The "Clear" row: remove every ball but the first. */
static result_t ball_clear(ball_task_t *bc)
{
  if (bc->window == NULL || bc->nballs <= 1)
    return result_OK;

  bc->nballs = 1;
  ball_set_title(bc);
  wuss_window_invalidate_visible(bc->window);

  return result_OK;
}

result_t ball_handle(wuss_window_t      *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  ball_task_t *bc;

  bc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return ball_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return ball_mouse(window, event->data.mouse.action, event->data.mouse.point.x,
                      event->data.mouse.point.y, event->data.mouse.button, task_data);

  case wuss_EVENT_IDLE:
    return ball_idle(task_data);

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return ball_pre_submenu_open(bc, event);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &bc->menu &&
        (event->data.menu_select.index == BALL_MENU_PAUSE ||
         event->data.menu_select.index == BALL_MENU_GRAVITY))
      return ball_toggle(bc, event);
    if (event->data.menu_select.menu == &bc->menu &&
        event->data.menu_select.index == BALL_MENU_CLEAR)
      return ball_clear(bc);
    return ball_menu_select(bc, event);

  case wuss_EVENT_MENU_CLOSED:
    bc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == bc->window)
      bc->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == bc->menu_items[BALL_MENU_INFO].window)
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
    ball_destroy(bc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
