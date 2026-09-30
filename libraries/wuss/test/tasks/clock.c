/* wuss/test/tasks/clock.c -- analogue clock task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <math.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/point.h"
#include "io/filetype.h"
#include "utils/fxp.h"

#include "clock.h"
#include "common.h"
#include "snapshot.h"

/* MENU click pops this menu; the item table and wuss_menu_t live per-instance
 * in clock_task_t, not as a file-scope static, so that each window's Info row
 * can hold its own .window pointer to the shared proginfo singleton, retargeted
 * just before wuss_menu_open */
enum
{
  CLOCK_MENU_INFO = 0,
  CLOCK_MENU_BACKGROUND,
  CLOCK_MENU_DIGITAL,
  CLOCK_MENU_SECONDS,
  CLOCK_MENU_12_HOUR,
  CLOCK_MENU_SAVE
};

#define CLOCK_SAVE_NAME "clock.png" /* Save As's initial leafname */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CLOCK_FACE_FRACTION  0.92 /* bezel radius as a fraction of half the smaller side */
#define CLOCK_HOUR_TICK      0.10 /* hour-tick length, fraction of face radius */
#define CLOCK_MINUTE_TICK    0.05 /* minute-tick length, fraction of face radius */
#define CLOCK_NUMERAL_RING   0.75 /* numeral-centre radius, fraction of face radius */
#define CLOCK_HOUR_HAND      0.50 /* hand lengths, fraction of face radius */
#define CLOCK_MINUTE_HAND    0.68
#define CLOCK_SECOND_HAND    0.80

/* a screen-space point in fix8_t, for screen_draw_line_wu_fix8 */
typedef struct fix8_point { fix8_t x, y; } fix8_point_t;

/* point on a circle of radius r about (cx,cy); angle 0 points straight up
 * (12 o'clock) and increases clockwise, matching a clock face */
static fix8_point_t clock_polar(double cx, double cy, double r, double angle)
{
  fix8_point_t p;

  p.x = FLOAT_TO_FIX8(cx + r * sin(angle));
  p.y = FLOAT_TO_FIX8(cy - r * cos(angle));

  return p;
}

static void clock_draw_hand(screen_t *scr,
                            double    cx,
                            double    cy,
                            double    r,
                            double    angle,
                            colour_t  colour)
{
  fix8_point_t tip;

  tip = clock_polar(cx, cy, r, angle);
  screen_draw_line_wu_fix8(scr,
                           FLOAT_TO_FIX8(cx), FLOAT_TO_FIX8(cy),
                           tip.x, tip.y,
                           colour);
}

/* draw text centred on (x,y), background transparent so it sits on the face */
static void clock_draw_centred(bmfont_t   *font,
                               screen_t   *scr,
                               const char *text,
                               double      x,
                               double      y,
                               colour_t    fg)
{
  bmfont_width_t width;
  point_t        pos;
  int            len, fh, ascent;

  len = (int) strlen(text);
  bmfont_measure(font, text, len, NULL, INT_MAX, NULL, &width);
  bmfont_get_info(font, NULL, &fh, &ascent, NULL);

  pos.x = (int) (x - width / 2.0);
  pos.y = (int) (y - fh / 2.0) + ascent;
  bmfont_draw(font, scr, text, len, fg, colour_rgba(0, 0, 0, 0), NULL, &pos,
             NULL);
}

static result_t clock_create_window(wuss_t       *wuss,
                                    clock_task_t *task,
                                    wuss_task_t  *delegate)
{
  return task_window_create(delegate, SIZE2D(160, 160), "Clock", &task->window);
}

/* wuss_saveas_save_fn_t: opaque is the clock_task_t */
static result_t clock_saveas_save(const char *path, void *opaque)
{
  clock_task_t *cc;

  cc = opaque;

  return snapshot_save_png(cc->window, clock_handle, cc, path);
}

/* the Save As dialogue's own task: forwards every event on its window into
 * wuss_saveas_handle_event. Not autoclose, and does nothing on QUIT -- the
 * clock_task_t block belongs to task->delegate's lifecycle, freed there,
 * not here. */
static result_t clock_saveas_handle(wuss_window_t      *window,
                                    const wuss_event_t *event,
                                    void               *task_data)
{
  clock_task_t *cc;

  cc = task_data;

  if (event->kind == wuss_EVENT_QUIT)
    return result_OK;

  (void) wuss_saveas_handle_event(cc->saveas, window, event);

  return result_OK;
}

result_t clock_create(wuss_t *wuss, clock_task_t **out)
{
  result_t         rc;
  clock_task_t    *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  wuss_task_desc_t saveas_desc;
  filetype_t       png_type;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->font        = wuss_get_font_n(wuss, 0);
  task->bg          = colour_rgb(0x1D, 0x2B, 0x53);
  task->bezel       = colour_rgb(0xFF, 0xF1, 0xE8);
  task->hand        = colour_rgb(0xFF, 0xF1, 0xE8);
  task->second_hand = colour_rgb(0xFF, 0x00, 0x4D);
  task->show_second = true;

  /* clock_redraw paints its own background every frame */
  delegate_desc.handle    = clock_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "clock";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = clock_create_window(wuss, task, delegate);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  /* separate, non-autoclose task: wuss_saveas_create forbids an autoclose
   * owner, and delegate (above) is one */
  saveas_desc.handle    = clock_saveas_handle;
  saveas_desc.task_data = task;
  saveas_desc.name      = "clock-saveas";
  rc = wuss_task_create(wuss, &saveas_desc, &task->saveas_task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, task->saveas_task, &png_type,
                          CLOCK_SAVE_NAME, clock_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->saveas_task);
    wuss_task_destroy(delegate);
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, CLOCK_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in clock_mouse */

  WUSS_MENU_ITEM_MENU(task->menu_items, CLOCK_MENU_BACKGROUND, "Background",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CLOCK_MENU_DIGITAL, "Digital",
                          wuss_MENU_ITEM_NONE, "D");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CLOCK_MENU_SECONDS, "Seconds",
                          wuss_MENU_ITEM_NONE, "S");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CLOCK_MENU_12_HOUR, "12-hour",
                          wuss_MENU_ITEM_NONE, "H");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CLOCK_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");

  WUSS_MENU_TITLE(task->menu, "Clock", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void clock_destroy(clock_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  wuss_task_destroy(task->saveas_task);
  free(task);
}

static result_t clock_redraw(const wuss_event_t *event, void *task_data)
{
  clock_task_t *cc;
  screen_t     *scr;
  const box_t  *content, *bounds;
  time_t        now;
  struct tm    *lt;
  double cx, cy, r;
  double hour_angle, minute_angle, second_angle;
  int    i, sx, sy;

  cc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr,
                   content->x0,
                   content->y0, box_size(content),
                   cc->bg);

  now = time(NULL);
  lt  = localtime(&now);

  cx = bounds->x0 - sx + (bounds->x1 - bounds->x0) / 2.0;
  cy = bounds->y0 - sy + (bounds->y1 - bounds->y0) / 2.0;

  if (cc->digital)
  {
    char        buf[12]; /* "HH:MM:SS PM" */
    int         hour;
    const char *suffix;

    hour   = lt->tm_hour;
    suffix = "";
    if (cc->twelve_hour)
    {
      suffix = (hour < 12) ? " AM" : " PM";
      hour   = (hour + 11) % 12 + 1; /* 0..23 -> 12, 1..11, 12, 1..11 */
    }

    if (cc->show_second)
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d%s",
               hour, lt->tm_min, lt->tm_sec, suffix);
    else
      snprintf(buf, sizeof(buf), "%02d:%02d%s", hour, lt->tm_min, suffix);
    clock_draw_centred(cc->font, scr, buf, cx, cy, cc->hand);
    return result_OK;
  }

  r  = MIN(bounds->x1 - bounds->x0, bounds->y1 - bounds->y0)
     / 2.0 * CLOCK_FACE_FRACTION;

  /* bezel */
  screen_draw_circle(scr, (int) (cx + 0.5), (int) (cy + 0.5), (int) (r + 0.5),
                     cc->bezel);

  /* 60 minute ticks, every fifth one an hour tick drawn longer */
  for (i = 0; i < 60; i++)
  {
    double       angle, inner;
    fix8_point_t a, b;

    angle = i * 2.0 * M_PI / 60.0;
    inner = (i % 5 == 0) ? (1.0 - CLOCK_HOUR_TICK) : (1.0 - CLOCK_MINUTE_TICK);
    a = clock_polar(cx, cy, r * inner, angle);
    b = clock_polar(cx, cy, r, angle);
    screen_draw_line_wu_fix8(scr, a.x, a.y, b.x, b.y, cc->bezel);
  }

  /* the 1..12 numerals, centred on a ring inside the hour ticks */
  for (i = 1; i <= 12; i++)
  {
    double       angle;
    fix8_point_t at;
    char         buf[3];

    angle = i * 2.0 * M_PI / 12.0;
    at    = clock_polar(cx, cy, r * CLOCK_NUMERAL_RING, angle);
    snprintf(buf, sizeof(buf), "%d", i);
    clock_draw_centred(cc->font, scr, buf,
                       FIX8_ROUND_TO_INT(at.x), FIX8_ROUND_TO_INT(at.y),
                       cc->bezel);
  }

  second_angle = lt->tm_sec * 2.0 * M_PI / 60.0;
  minute_angle = (lt->tm_min + lt->tm_sec / 60.0) * 2.0 * M_PI / 60.0;
  hour_angle   = ((lt->tm_hour % 12) + lt->tm_min / 60.0) * 2.0 * M_PI / 12.0;

  clock_draw_hand(scr, cx, cy, r * CLOCK_HOUR_HAND,   hour_angle,   cc->hand);
  clock_draw_hand(scr, cx, cy, r * CLOCK_MINUTE_HAND, minute_angle, cc->hand);
  if (cc->show_second)
    clock_draw_hand(scr, cx, cy, r * CLOCK_SECOND_HAND, second_angle,
                    cc->second_hand);

  return result_OK;
}

static result_t clock_mouse(clock_task_t *cc, wuss_button_t button)
{
  if (button & wuss_BUTTON_MENU)
  {
    static const wuss_proginfo_desc_t desc =
      TASK_PROGINFO_DESC("Clock", "Analogue or digital clock");
    wuss_proginfo_set_desc(&desc);
    cc->menu_items[CLOCK_MENU_INFO].window = wuss_proginfo_window(cc->delegate);
    wuss_menu_tick_item(&cc->menu, CLOCK_MENU_DIGITAL, cc->digital);
    wuss_menu_tick_item(&cc->menu, CLOCK_MENU_SECONDS, cc->show_second);
    wuss_menu_tick_item(&cc->menu, CLOCK_MENU_12_HOUR, cc->twelve_hour);

    return wuss_menu_open_at_pointer(cc->delegate, &cc->menu,
                                     &cc->menu_handle);
  }

  if (button & wuss_BUTTON_SELECT)
  {
    cc->show_second = !cc->show_second;
    wuss_window_invalidate_visible(cc->window);
  }
  else if (button & wuss_BUTTON_ADJUST)
  {
    cc->digital = !cc->digital;
    wuss_window_invalidate_visible(cc->window);
  }

  return result_OK;
}

static result_t clock_menu_select(clock_task_t       *cc,
                                  const wuss_event_t *event)
{
  if (wuss_colourmenu_selected_rgb(event, &cc->bg))
    wuss_window_invalidate_visible(cc->window);

  return result_OK;
}

/* The "Digital" row swaps between the analogue face and a text readout;
 * the "Seconds" row shows or hides the seconds, as a Select click does; the
 * "12-hour" row switches the readout between 24-hour and 12-hour AM/PM. An
 * ADJUST pick keeps the menu open, so retick the live row. */
static result_t clock_toggle(clock_task_t *cc, const wuss_event_t *event)
{
  int   index;
  bool *flag;

  index = event->data.menu_select.index;
  if (index == CLOCK_MENU_DIGITAL)
    flag = &cc->digital;
  else if (index == CLOCK_MENU_SECONDS)
    flag = &cc->show_second;
  else
    flag = &cc->twelve_hour;
  *flag = !*flag;

  wuss_menu_tick_item_live(cc->menu_handle, &cc->menu, index, *flag);

  wuss_window_invalidate_visible(cc->window);

  return result_OK;
}

result_t clock_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  clock_task_t *cc;

  cc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return clock_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != cc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return clock_mouse(cc, event->data.mouse.button);

  case wuss_EVENT_KEY:
    if (window != cc->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */
    return wuss_menu_dispatch_shortcut(cc->delegate, &cc->menu, event);

  case wuss_EVENT_IDLE:
  {
    time_t now;

    /* the proginfo dialogue is a second window on this same (autoclose)
     * delegate, so closing the clock window alone never empties
     * task->windows and the task lingers until the dialogue closes too --
     * guard against the dangling window in the meantime */
    if (cc->window == NULL)
      return result_OK;

    /* repaint only when the time shown changes: each second with seconds
     * on, each minute without */
    now = time(NULL);
    if (!cc->show_second)
      now -= now % 60;
    if (now == cc->shown)
      return result_OK;

    cc->shown = now;
    wuss_window_invalidate_visible(cc->window);
    return result_OK;
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    /* the "Background" row: the shared colourmenu, set up per open */
    return wuss_colourmenu_open_rgb(cc->wuss, event, "Background", cc->bg);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &cc->menu &&
        (event->data.menu_select.index == CLOCK_MENU_DIGITAL ||
         event->data.menu_select.index == CLOCK_MENU_SECONDS ||
         event->data.menu_select.index == CLOCK_MENU_12_HOUR))
      return clock_toggle(cc, event);
    if (event->data.menu_select.menu == &cc->menu &&
        event->data.menu_select.index == CLOCK_MENU_SAVE)
    {
      wuss_window_t *saveas_win;

      saveas_win = wuss_saveas_window(cc->saveas);
      wuss_window_set_hidden(saveas_win, 0);
      wuss_window_restack(saveas_win, wuss_ZORDER_FRONT);

      return result_OK;
    }
    return clock_menu_select(cc, event);

  case wuss_EVENT_MENU_CLOSED:
    cc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == cc->window)
      cc->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == cc->menu_items[CLOCK_MENU_INFO].window)
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
    clock_destroy(cc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
