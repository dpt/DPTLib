/* wuss/test/wuss-test.c -- wuss - minimal window manager */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "framebuf/palettes.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#ifdef WUSS_MENUS
#include "wuss/menu.h"
#include "wuss/menu-desc.h"
#endif
#ifdef WUSS_COMPONENTS
#include "wuss/component/fontmenu.h"
#include "wuss/component/colourmenu.h"
#endif

/* white-box: the menu-flash test drives picks through the icon layer and
 * reads back struct wuss__menu / struct wuss_icon state directly; the
 * furniture hit-test sweep calls wuss__furniture_hit_test and the box
 * helpers from impl.h / furniture.h directly */
#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)
#include "../core/impl.h"
#endif
#if defined(WUSS_ICONS)
#include "../icon.h"
#endif

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* The SDL interactive driver that used to live here (spawn_* callbacks, the
 * launcher table and the run_wuss loop) is now the standalone `wuss` app
 * in apps/wuss/main.c. This file is the Wuss unit test only. */

/* ----------------------------------------------------------------------- */

typedef struct test_task
{
  int                 redraw_count;
  int                 mouse_count;
  wuss_mouse_action_t last_action;
  int                 last_x, last_y;
  wuss_button_t       last_button;
  int                 last_scroll_x, last_scroll_y;
  int                 close_count;
  int                 pre_close_count;
  int                 show_count;
  int                 pre_show_count;
  int                 stop_count;
  int                 open_count;
  int                 palette_count;
  int                 idle_count;
  int                 veto_pre_close;
  int                 veto_pre_show;
  int                 open_window_via_handle;
  const wuss_menu_t  *submenu_to_open;
  int                 pre_submenu_open_count;
  int                 menu_select_count;
  int                 last_menu_index;
}
test_task_t;

static result_t test_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  test_task_t *tc;

  tc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    tc->redraw_count++;
    break;

  case wuss_EVENT_MOUSE:
    tc->mouse_count++;
    tc->last_action = event->data.mouse.action;
    tc->last_x      = event->data.mouse.point.x;
    tc->last_y      = event->data.mouse.point.y;
    tc->last_button = event->data.mouse.button;
    break;

  case wuss_EVENT_SCROLL:
    tc->last_scroll_x = event->data.scroll.point.x;
    tc->last_scroll_y = event->data.scroll.point.y;
    break;

  case wuss_EVENT_CLOSE:
    tc->close_count++;
    break;

  case wuss_EVENT_PRE_CLOSE:
    tc->pre_close_count++;
    if (tc->veto_pre_close)
      return result_BAD_ARG; /* any non-OK return vetoes the close */
    break;

  case wuss_EVENT_SHOW:
    tc->show_count++;
    break;

  case wuss_EVENT_PRE_SHOW:
    tc->pre_show_count++;
    if (tc->veto_pre_show)
      break; /* not calling wuss_window_reveal_now still shows: default proceed */
    if (tc->open_window_via_handle)
    {
      /* the correct guard: a plain window's PRE_SHOW carries handle == NULL
       * (already proceeding by default), and calling
       * wuss_menu_open_window_now with a NULL/non-menu handle is a caller
       * error -- regression coverage for a real crash where saturn.c and
       * apps/wuss/tasks.c called it unconditionally */
      if (event->data.pre_show.handle == NULL)
        return result_OK;
      return wuss_menu_open_window_now(event->data.pre_show.handle,
                                       event->data.pre_show.index);
    }
    return wuss_window_reveal_now(window);

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    tc->pre_submenu_open_count++;
    if (tc->submenu_to_open == NULL)
      break; /* this only fires for a flagged row; not calling
              * wuss_menu_open_submenu_now leaves it inert */
    return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle,
                                      event->data.pre_submenu_open.index,
                                      tc->submenu_to_open);

  case wuss_EVENT_MENU_SELECT:
    tc->menu_select_count++;
    tc->last_menu_index = event->data.menu_select.index;
    break;

  case wuss_EVENT_QUIT:
    tc->stop_count++;
    break;

  case wuss_EVENT_OPEN:
    tc->open_count++;
    break;

  case wuss_EVENT_PALETTE:
    tc->palette_count++;
    break;

  case wuss_EVENT_IDLE:
    tc->idle_count++;
    break;

  default:
    break;
  }

  return result_OK;
}

/* An IDLE handler that closes a window the first time it is broadcast to. On
 * an autoclose task with that as its only window, the close reaps the task
 * and frees its list node from inside the wuss_idle / wuss_set_palette walk;
 * the test checks the walk survives it and still reaches the tasks behind. */
static wuss_window_t *g_close_on_idle_win;

static result_t close_on_idle_handle(wuss_window_t      *window,
                                     const wuss_event_t *event,
                                     void               *task_data)
{
  test_task_t *tc;

  NOT_USED(window);

  tc = task_data;

  if (event->kind == wuss_EVENT_IDLE)
  {
    tc->idle_count++;
    if (g_close_on_idle_win != NULL)
    {
      wuss_window_t *doomed;

      doomed              = g_close_on_idle_win;
      g_close_on_idle_win = NULL;
      wuss_window_close(doomed); /* reaps this task mid-walk */
    }
    return result_OK;
  }

  if (event->kind == wuss_EVENT_QUIT)
    tc->stop_count++;

  return result_OK;
}

/* A redraw handler that paints its content as one-pixel horizontal lines
 * whose colour encodes the document-space Y of each row, so a test can read
 * the framebuffer back and tell not just whose pixels are where but whether
 * a blit slid them by the right amount. task_data points to a colour_t used
 * only as a base hue (its blue channel is replaced per row). NULL task_data
 * means flood with a single fixed colour instead. */
static result_t paint_handle(wuss_window_t      *window,
                             const wuss_event_t *event,
                             void               *task_data)
{
  const box_t *clip;
  const box_t *bounds;
  point_t      scroll;
  int          y, doc_y;

  NOT_USED(window);

  if (event->kind != wuss_EVENT_REDRAW)
    return result_OK;

  clip   = event->data.redraw.content;
  bounds = event->data.redraw.bounds;
  scroll = event->data.redraw.scroll;

  if (task_data == NULL)
  {
    screen_fill_rect(event->data.redraw.scr, clip->x0, clip->y0,
                     SIZE2D(clip->x1 - clip->x0, clip->y1 - clip->y0),
                     colour_rgb(0xcc, 0xdd, 0xee));
    return result_OK;
  }

  for (y = clip->y0; y < clip->y1; y++)
  {
    doc_y = y - bounds->y0 + scroll.y;
    screen_fill_rect(event->data.redraw.scr, clip->x0, y,
                     SIZE2D(clip->x1 - clip->x0, 1),
                     colour_rgb(0x20, 0x40, doc_y & 0xff));
  }

  return result_OK;
}

/* A redraw handler that ignores "clip" and always floods its whole content
 * box with the colour_t task_data points to, relying entirely on wuss-core's
 * own clipping (scr->clip during the redraw dispatch) to keep the fill
 * inside the window -- matching a task such as saturn.c that redraws its
 * full fixed sample space every call rather than restricting itself to the
 * dirty piece it was handed. */
static result_t flood_full_bounds_handle(wuss_window_t      *window,
                                         const wuss_event_t *event,
                                         void               *task_data)
{
  const box_t    *bounds;
  const colour_t *colour;

  NOT_USED(window);

  if (event->kind != wuss_EVENT_REDRAW)
    return result_OK;

  bounds = event->data.redraw.bounds;
  colour = task_data;
  screen_fill_rect(event->data.redraw.scr, bounds->x0, bounds->y0,
                   SIZE2D(bounds->x1 - bounds->x0, bounds->y1 - bounds->y0),
                   *colour);

  return result_OK;
}

/* ----------------------------------------------------------------------- */

/* Most tests declare their test_task_t on the block stack. A registered
 * task outlives that block (wuss only sweeps at wuss_destroy), so once the
 * block exits the task's task_data dangles. Broadcast events (PALETTE,
 * IDLE) walk every registered task and would read that freed stack.
 *
 * mk_task records every task it makes; reap_test_tasks destroys them all.
 * Call reap_test_tasks() before any broadcast test and before
 * wuss_destroy, so no stale stack is ever delivered to. */
#define MK_TASK_MAX 64
static wuss_task_t *mk_task_reg[MK_TASK_MAX];
static int          mk_task_count;

/* Register a task in one call. Returns NULL on OOM; callers goto Failure. */
static wuss_task_t *mk_task(wuss_t           *wuss,
                            wuss_window_fn_t *handle,
                            void             *task_data)
{
  wuss_task_desc_t desc;
  wuss_task_t     *task;

  desc.handle    = handle;
  desc.task_data = task_data;
  desc.name      = "wuss-test";

  if (wuss_task_create(wuss, &desc, &task) != result_OK)
    return NULL;

  if (mk_task_count < MK_TASK_MAX)
    mk_task_reg[mk_task_count++] = task;

  return task;
}

/* Destroy every task mk_task created and forget them. Each
 * wuss_task_destroy closes that task's windows and fires one QUIT. */
static void reap_test_tasks(void)
{
  int i;

  for (i = 0; i < mk_task_count; i++)
    wuss_task_destroy(mk_task_reg[i]);
  mk_task_count = 0;
}

/* Drop the registry without destroying anything -- for a block that hands
 * its tasks straight to wuss_destroy and wants that teardown path exercised
 * rather than wuss_task_destroy's. */
static void forget_test_tasks(void)
{
  mk_task_count = 0;
}

/* ----------------------------------------------------------------------- */

#if defined(WUSS_MENUS) && defined(WUSS_ICONS)
/* Click menu row `row` of the open chain level `level` with `button`: a
 * MOUSE_DOWN then MOUSE_UP at the row's on-screen centre, exactly as the
 * real event pump would deliver them. */
static void flash_pick_row(wuss_t            *wuss,
                           struct wuss__menu *level,
                           int                row,
                           wuss_button_t      button)
{
  box_t   content;
  box_t   bbox;
  box_t   screen_box;
  point_t scroll;
  point_t at;

  wuss_window_get_content_bounds(level->window, &content);
  wuss_window_get_scroll(level->window, &scroll);
  wuss_icon_get_bbox(level->icons[row], &bbox);
  wuss__icon_box_to_screen(&content, scroll, &bbox, &screen_box);

  at.x = (screen_box.x0 + screen_box.x1) / 2;
  at.y = (screen_box.y0 + screen_box.y1) / 2;

  wuss_mouse_click(wuss, at, button, wuss_MOUSE_DOWN, NULL);
  wuss_mouse_click(wuss, at, button, wuss_MOUSE_UP, NULL);
}

/* Move the pointer over row `row` of menu level `level`: to the text column
 * (`over_arrow` == 0) or into the submenu-arrow gutter at the row's right edge
 * (`over_arrow` == 1). Used to drive submenu open/close from the tests. */
static void menu_move_over_row(wuss_t            *wuss,
                               struct wuss__menu *level,
                               int                row,
                               int                over_arrow)
{
  box_t   content;
  box_t   bbox;
  box_t   screen_box;
  point_t scroll;
  point_t at;

  wuss_window_get_content_bounds(level->window, &content);
  wuss_window_get_scroll(level->window, &scroll);
  wuss_icon_get_bbox(level->icons[row], &bbox);
  wuss__icon_box_to_screen(&content, scroll, &bbox, &screen_box);

  at.x = over_arrow ? screen_box.x1 - 4
                    : screen_box.x0 + 4;
  at.y = (screen_box.y0 + screen_box.y1) / 2;

  wuss_mouse_move(wuss, at, NULL);
}

/* A task that opens its own menu on a MENU press over its window, the way a
 * real content task (e.g. greeble) does. task_data points to a menu_task_t
 * so the test can see the press arrived and which menu it raised. */
typedef struct menu_task
{
  wuss_task_t       *self;
  const wuss_menu_t *menu;
  int                menu_press_count;
  wuss_menu_handle_t menu_handle;      /* last chain this task opened */
  int                menu_closed_count; /* wuss_EVENT_MENU_CLOSED deliveries */
  int                close_chain_on_quit; /* mimic image.c: close a still-open
                                           * chain from the QUIT handler */
}
menu_task_t;

static result_t menu_open_handle(wuss_window_t      *window,
                                 const wuss_event_t *event,
                                 void               *task_data)
{
  menu_task_t *mt;

  NOT_USED(window);

  mt = task_data;

  if (event->kind == wuss_EVENT_MOUSE                     &&
      event->data.mouse.action == wuss_MOUSE_DOWN         &&
      (event->data.mouse.button & wuss_BUTTON_MENU))
  {
    mt->menu_press_count++;
    return wuss_menu_open(mt->self, mt->menu,
                          event->data.mouse.point, &mt->menu_handle);
  }

  if (event->kind == wuss_EVENT_MENU_CLOSED)
  {
    mt->menu_closed_count++;
    mt->menu_handle = NULL; /* chain freed under us; handle now stale */
  }

  if (event->kind == wuss_EVENT_QUIT && mt->close_chain_on_quit)
    wuss_menu_close(mt->menu_handle); /* against the menu task, still live */

  return result_OK;
}
#endif

/* ----------------------------------------------------------------------- */

/* Total area covered by the dirty list, counting overlapped pixels once.
 * Summing each region's area instead would double-count wherever two
 * invalidations overlap, which they legitimately do. */
static int dirty_union_area(wuss_t *wuss, const box_t *bounds)
{
  static unsigned char covered[512 * 512];

  box_t                region;
  int                  w, h, i, x, y, area;

  w = bounds->x1 - bounds->x0;
  h = bounds->y1 - bounds->y0;
  if (w <= 0 || h <= 0 || w > 512 || h > 512)
    return -1;

  memset(covered, 0, (size_t) w * h);

  for (i = 0; i < wuss_get_dirty_count(wuss); i++)
  {
    wuss_get_dirty(wuss, i, &region);
    for (y = MAX(region.y0, bounds->y0); y < MIN(region.y1, bounds->y1); y++)
      for (x = MAX(region.x0, bounds->x0); x < MIN(region.x1, bounds->x1); x++)
        covered[(y - bounds->y0) * w + (x - bounds->x0)] = 1;
  }

  area = 0;
  for (i = 0; i < w * h; i++)
    area += covered[i];

  return area;
}

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

/* Sweep every pixel of a window's visible box through wuss__furniture_hit_test
 * and check the tiling invariant: no pixel is wuss_FURNITURE_NONE, and a pixel
 * reports wuss_FURNITURE_CONTENT if and only if it lies inside the drawn
 * content box -- with one documented exception, the bare outline band of a
 * fully chromeless window (no titlebar, no scrollbars, no resize), where
 * content_may_leak is passed non-zero to allow CONTENT on 1px-frame pixels
 * outside the content box too. Returns 1 on pass, 0 on the first breach
 * (printing the offending pixel). */
static int furniture_hit_sweep(const wuss_window_t *window,
                               int                  content_may_leak)
{
  box_t visible, content;
  int   x, y, inside;

  wuss_window_get_visible_bounds((wuss_window_t *) window, &visible);
  wuss__content_box(window, &content);

  for (y = visible.y0; y < visible.y1; y++)
  {
    for (x = visible.x0; x < visible.x1; x++)
    {
      wuss_furniture_region_t region;

      region = wuss__furniture_hit_test(window, POINT(x, y));
      inside = box_contains_point(&content, x, y);

      if (region == wuss_FURNITURE_NONE)
      {
        printf("wuss_test: hit sweep: (%d,%d) is FURNITURE_NONE\n", x, y);
        return 0;
      }

      if (inside && region != wuss_FURNITURE_CONTENT)
      {
        printf("wuss_test: hit sweep: content pixel (%d,%d) reported %d\n",
               x, y, (int) region);
        return 0;
      }

      if (!inside && region == wuss_FURNITURE_CONTENT && !content_may_leak)
      {
        printf("wuss_test: hit sweep: chrome pixel (%d,%d) reported CONTENT\n",
               x, y);
        return 0;
      }
    }
  }

  return 1;
}

result_t wuss_test(const char *resources)
{
  result_t       rc;
  int            rowbytes;
  void          *pixels;
  bitmap_t       bm;
  screen_t       scr;
  wuss_t        *wuss;
  wuss_config_t  bad_config;
  wuss_t        *bad_wuss;
  test_task_t    tc_a, tc_b, tc_c, tc_d;
  wuss_task_t   *delegate_a, *delegate_b, *delegate_c, *delegate_d;
  box_t          box_a, box_b, box_c, box_d;
  wuss_window_t *win_a, *win_b, *win_c, *win_d;
  wuss_window_t *hit;
  box_t          visible, content;
  int            before_a, before_b;
  int            width, height;
  const colour_t custom_palette[2] = { 0, 0 };

#if !(defined(WUSS_MENUS) && defined(WUSS_ICONS))
  NOT_USED(resources); /* only the menu-flash test reads it */
#endif

  rowbytes = 200 * 4;
  pixels = malloc(rowbytes * 200);
  if (pixels == NULL)
    goto Failure;

  rc = bitmap_init(&bm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes, NULL, pixels);
  if (rc != result_OK)
    goto Failure;

  screen_for_bitmap(&scr, &bm);

  printf("test: wuss_create with bad titlebar colour index\n");

  bad_config.titlebar_height         = 0;
  bad_config.furniture.title.bg        = 100; /* in range as a byte, but well past any test palette and below wuss_COLOUR_SYMBOLIC */
  bad_config.furniture.title.fg        = 0;
  bad_config.furniture.outline         = 0;
  bad_config.furniture.back            = 0;
  bad_config.furniture.close           = 0;
  bad_config.furniture.toggle          = 0;
  bad_config.furniture.resize          = 0;
  bad_config.furniture.scroll.arrows   = 0;
  bad_config.furniture.scroll.wells    = 0;
  bad_config.furniture.scroll.sausages = 0;
  bad_config.bevel.light               = 0;
  bad_config.bevel.dark                = 0;
  bad_config.bevel.divider             = 0;
  bad_config.button.bg                 = 0;
  bad_config.button.fg                 = 0;
  bad_config.button.pressed            = 0;
  bad_config.accent.colour             = 0;
  rc = wuss_create(&scr, NULL, 0, NULL, 0, &bad_config, NULL, NULL, &bad_wuss);
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  printf("test: wuss_create with custom palette, no config\n");

  {
    wuss_t *custom_wuss;

    rc = wuss_create(&scr, NULL, 0, custom_palette, 2, NULL, NULL, NULL, &custom_wuss);
    if (rc != result_OK)
      goto Failure;
    wuss_destroy(custom_wuss);
  }

  printf("test: wuss_create with default palette\n");

  rc = wuss_create(&scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: symbolic wuss_colour_t resolves against palette and config\n");

  {
    /* red, green, blue, white, black (colour_t primary is 0xAABBGGRR) --
     * enough for the named symbolics to land on distinct, checkable
     * indices. */
    static const colour_t sympal[5] =
    {
      { 0xFF0000FF }, { 0xFF00FF00 }, { 0xFFFF0000 },
      { 0xFFFFFFFF }, { 0xFF000000 }
    };

    wuss_config_t  symcfg;
    wuss_t        *symw;
    wuss_window_t *symwin;
    wuss_task_t   *symdel;
    box_t          symbox;

    memset(&symcfg, 0, sizeof(symcfg));
    symcfg.furniture.title.bg = wuss_COLOUR_BLUE;   /* -> index 2 */
    symcfg.furniture.title.fg = wuss_COLOUR_WHITE;  /* -> index 3 */
    symcfg.backdrop = wuss_BACKDROP_COLOUR(wuss_COLOUR_GREEN); /* -> 1 */
    symcfg.body.window = wuss_COLOUR_RED;           /* -> index 0 */
    symcfg.body.menu   = wuss_COLOUR_WHITE;         /* -> index 3 */

    rc = wuss_create(&scr, NULL, 0, sympal, 5, &symcfg, NULL, NULL, &symw);
    if (rc != result_OK)
      goto Failure;

    /* named colours resolve to nearest-palette-entry; raw indices and
     * wuss_NO_BACKGROUND pass straight through; chrome roles echo the
     * resolved config. */
    if (wuss__resolve_colour(symw, wuss_COLOUR_RED)   != 0 ||
        wuss__resolve_colour(symw, wuss_COLOUR_GREEN) != 1 ||
        wuss__resolve_colour(symw, wuss_COLOUR_BLUE)  != 2 ||
        wuss__resolve_colour(symw, wuss_COLOUR_WHITE) != 3 ||
        wuss__resolve_colour(symw, wuss_COLOUR_BLACK) != 4 ||
        wuss__resolve_colour(symw, 2) != 2 ||
        wuss__resolve_colour(symw, wuss_NO_BACKGROUND) != wuss_NO_BACKGROUND ||
        wuss__resolve_colour(symw, wuss_COLOUR_TITLE_BG) != 2 ||
        wuss__resolve_colour(symw, wuss_COLOUR_TITLE_FG) != 3 ||
        wuss__resolve_colour(symw, wuss_COLOUR_BACKDROP) != 1 ||
        wuss__resolve_colour(symw, wuss_COLOUR_WINDOW) != 0 ||
        wuss__resolve_colour(symw, wuss_COLOUR_MENU) != 3)
    {
      wuss_destroy(symw);
      goto Failure;
    }

    /* a symbolic colour is accepted (and stored resolved) where a client
     * passes a wuss_colour_t. */
    symdel = mk_task(symw, NULL, NULL);
    if (symdel == NULL) { wuss_destroy(symw); goto Failure; }
    symbox.x0 = 0; symbox.y0 = 0; symbox.x1 = 80; symbox.y1 = 80;
    rc = wuss_window_create(symdel, &symbox, "sym", wuss_WINDOW_DEFAULT,
                            wuss_BACKDROP_COLOUR(wuss_COLOUR_RED),
                            SIZE2D(80, 80), SIZE2D(80, 80), &symwin);
    if (rc != result_OK) { wuss_destroy(symw); goto Failure; }
    if (symwin->bg.colour != 0) /* wuss_COLOUR_RED -> palette index 0 */
    {
      wuss_destroy(symw);
      goto Failure;
    }

    wuss_destroy(symw); /* sweeps symdel too */
    mk_task_count = 0;  /* drop the now-stale registry entry */
  }

  printf("test: window_create too small\n");

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 0; /* zero-height content is invalid regardless of furniture */
  rc = wuss_window_create(mk_task(wuss, NULL, NULL),
                          &box_a,
                          "toosmall",
                          wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP,
                          box_size(&box_a),
                          SIZE2D(0, 0),
                          &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  memset(&tc_a, 0, sizeof(tc_a));
  delegate_a = mk_task(wuss, test_handle, &tc_a);
  if (delegate_a == NULL) goto Failure;

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 100;
  rc = wuss_window_create(delegate_a,
                          &box_a,
                          "A",
                          wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                          ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                          ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                          wuss_NO_BACKDROP,
                          box_size(&box_a),
                          SIZE2D(0, 0),
                          &win_a);
  if (rc != result_OK)
    goto Failure;

  memset(&tc_b, 0, sizeof(tc_b));
  delegate_b = mk_task(wuss, test_handle, &tc_b);
  if (delegate_b == NULL) goto Failure;

  box_b.x0 = 50;
  box_b.y0 = 50;
  box_b.x1 = 150;
  box_b.y1 = 150;
  rc = wuss_window_create(delegate_b,
                          &box_b,
                          "B",
                          wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                          ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                          ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                          wuss_NO_BACKDROP,
                          box_size(&box_b),
                          SIZE2D(0, 0),
                          &win_b);
  if (rc != result_OK)
    goto Failure;

  printf("test: redraw\n");

  rc = wuss_redraw(wuss);
  if (rc != result_OK)
    goto Failure;
  /* A's visible footprint is L-shaped (B covers its bottom-right corner),
   * so it's redrawn as two non-overlapping pieces; B is unoccluded, one */
  if (tc_a.redraw_count != 2 || tc_b.redraw_count != 1)
    goto Failure;

  printf("test: invalidating an area of A fully covered by topmost B is discarded\n");

  {
    box_t local;

    local.x0 = 60;
    local.y0 = 60;
    local.x1 = 90;
    local.y1 = 90; /* well within B's (50,50)-(150,150)+furniture visible footprint */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    local.x0 = 0;
    local.y0 = 0;
    local.x1 = 20;
    local.y1 = 20; /* outside B's footprint entirely: nothing to clip away */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
  }

  printf("test: a piece straddling B's edge redraws A but not B\n");

  {
    box_t local;

    before_a = tc_a.redraw_count;
    before_b = tc_b.redraw_count;

    local.x0 = 30;
    local.y0 = 60;
    local.x1 = 70;
    local.y1 = 90; /* straddles B's left edge (x=49): only x:30-49 survives clipping */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_a.redraw_count != before_a + 1)
      goto Failure;
    if (tc_b.redraw_count != before_b)
      goto Failure; /* B not touched by the surviving piece: must not be redrawn */
  }

  printf("test: z-order hit test and local coordinate translation (B on top)\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;
  if (tc_b.last_action != wuss_MOUSE_DOWN || tc_b.last_x != 25 || tc_b.last_y != 25)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: close icon runs wuss_window_try_close; a PRE_CLOSE veto keeps the window\n");

  tc_a.pre_close_count = 0;
  tc_a.close_count     = 0;
  tc_a.veto_pre_close  = 1;
  rc = wuss_mouse_click(wuss, POINT(6, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's close icon */
  if (rc != result_BAD_ARG)
    goto Failure; /* the veto's non-OK return propagates out of the click */
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 36), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit); /* if the close click had started a drag, this would move A */
  if (rc != result_OK)
    goto Failure;

  if (tc_a.pre_close_count != 1)
    goto Failure; /* the close icon asked the task first */
  if (tc_a.close_count != 0)
    goto Failure; /* vetoed: no CLOSE, window not torn down */

  wuss_window_get_visible_bounds(win_a, &visible); /* still alive */
  if (visible.x0 != 0 || visible.y0 != 0)
    goto Failure; /* unmoved: no drag was started by the close click */

  tc_a.veto_pre_close = 0;

  printf("test: click-to-front changes subsequent overlap hits\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(31, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's titlebar, above its content, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 11), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click does not change z-order\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* B's content, only within B */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A still topmost: B's content click above didn't bring it to front */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1 || tc_a.last_x != 74 || tc_a.last_y != 54)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: titlebar click starts a drag, not delivered as content\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(31, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's titlebar, A already topmost, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 0)
    goto Failure;

  printf("test: drag-move updates visible bounds and invalidates the affected region\n");

  rc = wuss_redraw_dirty(wuss); /* flush the click-to-front's leftover dirty region first */
  if (rc != result_OK)
    goto Failure;

  before_a = tc_a.redraw_count;
  before_b = tc_b.redraw_count;
  rc = wuss_mouse_move(wuss, POINT(31, 36), &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.redraw_count != before_a || tc_b.redraw_count != before_b)
    goto Failure; /* invalidated, not yet redrawn */

  if (wuss_get_dirty_count(wuss) == 0)
    goto Failure;

  rc = wuss_redraw_dirty(wuss);
  if (rc != result_OK)
    goto Failure;
  if (tc_a.redraw_count != before_a || tc_b.redraw_count != before_b)
    goto Failure; /* blitted, not redrawn: A's own pixels moved without a task
                    * callback, and the vacated sliver behind its old position
                    * exposes only background, not B */

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 25)
    goto Failure;

  printf("test: mouse-up ends the drag\n");

  rc = wuss_mouse_click(wuss, POINT(31, 36), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  printf("test: Adjust-drag moves a window without bringing it to front\n");

  rc = wuss_mouse_click(wuss, POINT(140, 35), wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* B's titlebar, clear of A */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_move(wuss, POINT(145, 60), &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  wuss_window_get_visible_bounds(win_b, &visible);
  if (visible.x0 != 54 || visible.y0 != 54)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(145, 60), wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* within both A and B; A still topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_move(wuss, POINT(200, 200), &hit); /* off all windows, drag must have ended */
  if (rc != result_OK)
    goto Failure;
  if (hit != NULL)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 25)
    goto Failure;

  if (tc_a.open_count != 1)
    goto Failure; /* wuss_EVENT_OPEN sent once for the drag-move above */

  printf("test: window_resize valid and too-small cases\n");

  rc = wuss_window_resize(win_a, SIZE2D(50, 0)); /* zero-height content is invalid */
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;
  if (tc_a.open_count != 1)
    goto Failure; /* rejected resize: no wuss_EVENT_OPEN */

  rc = wuss_window_resize(win_a, SIZE2D(50, 50));
  if (rc != result_OK)
    goto Failure;
  if (tc_a.open_count != 2)
    goto Failure; /* wuss_EVENT_OPEN sent for the successful resize */

  /* content ends up exactly the requested size... */
  wuss_window_get_content_bounds(win_a, &content);
  width  = content.x1 - content.x0;
  height = content.y1 - content.y0;
  if (width != 50 || height != 50)
    goto Failure;

  /* ...with the titlebar/outline furniture added on top of that */
  wuss_window_get_visible_bounds(win_a, &visible);
  width  = visible.x1 - visible.x0;
  height = visible.y1 - visible.y0;
  if (width != 52 || height != 72)
    goto Failure;

  printf("test: window_resize honours the requested size even past the "
         "screen edge\n");

  /* screen is 200x200; win_a's visible box sits at (0,25) with a 1px
   * outline all round and a 20px titlebar. asking for a 500x500 content
   * area is honoured verbatim; the visible box simply overhangs the
   * screen (width 500+2, height 500+2+20). */
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 500 || content.y1 - content.y0 != 500)
    goto Failure;
  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x1 != 502 || visible.y1 != 547)
    goto Failure;

  /* a smaller request is likewise honoured verbatim */
  rc = wuss_window_resize(win_a, SIZE2D(60, 40));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 60 || content.y1 - content.y0 != 40)
    goto Failure;

  /* restore for the tests that follow */
  rc = wuss_window_resize(win_a, SIZE2D(50, 50));
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create can never make a window bigger than the "
         "screen\n");

  {
    box_t          box_big;
    wuss_window_t *win_big;

    /* content box asks for 10,10..400,400; the on-screen nudge pulls the
     * visible top-left (10-1 outline) back to (0,0), then the clamp caps
     * the content at 200 - 2*1 - 20(titlebar) tall, 200 - 2*1 wide. */
    box_big.x0 = 10; box_big.y0 = 10; box_big.x1 = 400; box_big.y1 = 400;
    rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_big, "BIG",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_big);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_big, &visible);
    if (visible.x0 != 0 || visible.y0 != 0 ||
        visible.x1 != 200 || visible.y1 != 200)
      goto Failure;
    wuss_window_close(win_big);
  }

  printf("test: title-less, no-outline window has no furniture, so visible == content\n");

  tc_d.redraw_count = 0;
  tc_d.mouse_count  = 0;
  delegate_d = mk_task(wuss, test_handle, &tc_d);
  if (delegate_d == NULL) goto Failure;

  box_d.x0 = 0;  box_d.y0 = 160;
  box_d.x1 = 30; box_d.y1 = 175; /* shorter than the 20px titlebar_height, still valid: no titlebar to fit */
  rc = wuss_window_create(delegate_d,
                          &box_d,
                          "ignored",
                          /* fully chromeless: no titlebar, outline, scrollbars or resize */
                          wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                          wuss_NO_BACKDROP,
                          box_size(&box_d),
                          SIZE2D(0, 0),
                          &win_d);
  if (rc != result_OK)
    goto Failure;

  wuss_window_get_visible_bounds(win_d, &visible);
  if (visible.x0 != box_d.x0 || visible.y0 != box_d.y0 ||
      visible.x1 != box_d.x1 || visible.y1 != box_d.y1)
    goto Failure;

  printf("test: click within a title-less window's top edge is delivered as content, not a drag\n");

  rc = wuss_mouse_click(wuss, POINT(5, 165), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_d)
    goto Failure;
  if (tc_d.mouse_count != 1 || tc_d.last_action != wuss_MOUSE_DOWN || tc_d.last_x != 5 || tc_d.last_y != 5)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(5, 165), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click on a title-less window does not change z-order\n");

  {
    static test_task_t tc_e, tc_f;
    wuss_task_t       *delegate_e, *delegate_f;
    box_t              box_e, box_f;
    wuss_window_t     *win_e, *win_f;

    tc_e.redraw_count = 0;
    tc_e.mouse_count  = 0;
    delegate_e = mk_task(wuss, test_handle, &tc_e);
    if (delegate_e == NULL) goto Failure;

    box_e.x0 = 100; box_e.y0 = 0;
    box_e.x1 = 150; box_e.y1 = 50;
    rc = wuss_window_create(delegate_e,
                            &box_e,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR,
                            wuss_NO_BACKDROP,
                            box_size(&box_e),
                            SIZE2D(0, 0),
                            &win_e);
    if (rc != result_OK)
      goto Failure;

    tc_f.redraw_count = 0;
    tc_f.mouse_count  = 0;
    delegate_f = mk_task(wuss, test_handle, &tc_f);
    if (delegate_f == NULL) goto Failure;

    box_f.x0 = 130; box_f.y0 = 20;
    box_f.x1 = 180; box_f.y1 = 70;
    rc = wuss_window_create(delegate_f,
                            &box_f,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR,
                            wuss_NO_BACKDROP,
                            box_size(&box_f),
                            SIZE2D(0, 0),
                            &win_f);
    if (rc != result_OK)
      goto Failure;

    /* F was created after E, so F is topmost; clicking E's exposed content
     * (outside the overlap) is delivered to E but must not raise it */
    rc = wuss_mouse_click(wuss, POINT(110, 10), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* within E only */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_e)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(110, 10), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(135, 25), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: F still on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_f)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(135, 25), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_e);
    wuss_window_close(win_f);
  }

  printf("test: wuss_window_set_background\n");

  rc = wuss_window_set_background(win_d, wuss_BACKDROP_COLOUR(100));
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  rc = wuss_window_set_background(win_d, wuss_BACKDROP_COLOUR(1));
  if (rc != result_OK)
    goto Failure;

  wuss_window_close(win_d);

  printf("test: moving/resizing a window entirely behind an occluder has no visible effect\n");

  {
    static test_task_t tc_h, tc_g;
    wuss_task_t       *delegate_h, *delegate_g;
    box_t              box_h, box_g;
    wuss_window_t     *win_h, *win_g;
    int                before_h, before_g;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h = mk_task(wuss, test_handle, &tc_h);
    if (delegate_h == NULL) goto Failure;

    box_h.x0 = 10; box_h.y0 = 10;
    box_h.x1 = 30; box_h.y1 = 30;
    rc = wuss_window_create(delegate_h,
                            &box_h,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_h),
                            SIZE2D(0, 0),
                            &win_h);
    if (rc != result_OK)
      goto Failure;

    tc_g.redraw_count = 0;
    tc_g.mouse_count  = 0;
    delegate_g = mk_task(wuss, test_handle, &tc_g);
    if (delegate_g == NULL) goto Failure;

    box_g.x0 = 0;   box_g.y0 = 0;
    box_g.x1 = 150; box_g.y1 = 150; /* G is created after H, so G is topmost and fully covers H */
    rc = wuss_window_create(delegate_g,
                            &box_g,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_g),
                            SIZE2D(0, 0),
                            &win_g);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidations before measuring */
    if (rc != result_OK)
      goto Failure;

    before_h = tc_h.redraw_count;
    before_g = tc_g.redraw_count;

    wuss_window_move(win_h, POINT(60, 60)); /* still entirely within G's footprint */
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_h.redraw_count != before_h || tc_g.redraw_count != before_g)
      goto Failure; /* nothing visible changed: no redraw of either window */

    rc = wuss_window_resize(win_h, SIZE2D(25, 25)); /* still entirely within G's footprint */
    if (rc != result_OK)
      goto Failure;
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_h.redraw_count != before_h || tc_g.redraw_count != before_g)
      goto Failure;

    wuss_window_close(win_h);
    wuss_window_close(win_g);
  }

  printf("test: bring-to-front only invalidates the newly-uncovered part\n");

  {
    static test_task_t tc_i, tc_j;
    wuss_task_t       *delegate_i, *delegate_j;
    box_t              box_i, box_j, dirty;
    wuss_window_t     *win_i, *win_j;

    tc_i.redraw_count = 0;
    tc_i.mouse_count  = 0;
    delegate_i = mk_task(wuss, test_handle, &tc_i);
    if (delegate_i == NULL) goto Failure;

    box_i.x0 = 0; box_i.y0 = 0;
    box_i.x1 = 100; box_i.y1 = 100;
    rc = wuss_window_create(delegate_i,
                            &box_i,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_i),
                            SIZE2D(0, 0),
                            &win_i);
    if (rc != result_OK)
      goto Failure;

    tc_j.redraw_count = 0;
    tc_j.mouse_count  = 0;
    delegate_j = mk_task(wuss, test_handle, &tc_j);
    if (delegate_j == NULL) goto Failure;

    box_j.x0 = 50; box_j.y0 = 0;
    box_j.x1 = 150; box_j.y1 = 100; /* J created after I, so J is topmost, covering I's right half */
    rc = wuss_window_create(delegate_j,
                            &box_j,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_j),
                            SIZE2D(0, 0),
                            &win_j);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidations before measuring */
    if (rc != result_OK)
      goto Failure;

    wuss_window_restack(win_i, wuss_ZORDER_FRONT);

    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    wuss_get_dirty(wuss, 0, &dirty);
    if (dirty.x0 != 50 || dirty.y0 != 0 || dirty.x1 != 100 || dirty.y1 != 100)
      goto Failure; /* only I's previously-hidden right half, not its whole 0..100 footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_i);
    wuss_window_close(win_j);
  }

  printf("test: dragging off-screen and back on repaints the reappearing edge\n");

  {
    static test_task_t tc_m;
    wuss_task_t       *delegate_m;
    box_t              box_m;
    wuss_window_t     *win_m;
    int                before_m;

    tc_m.redraw_count = 0;
    tc_m.mouse_count  = 0;
    delegate_m = mk_task(wuss, test_handle, &tc_m);
    if (delegate_m == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 60; box_m.y1 = 60; /* 50x50, fully on-screen, topmost (created last) */
    rc = wuss_window_create(delegate_m,
                            &box_m,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_m),
                            SIZE2D(0, 0),
                            &win_m);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidation, paint M's initial content */
    if (rc != result_OK)
      goto Failure;

    wuss_window_move(win_m, POINT(-40, 10)); /* slide left until half of M is off the left edge */
    rc = wuss_redraw_dirty(wuss); /* flush the vacated-sliver repaint from this move */
    if (rc != result_OK)
      goto Failure;

    before_m = tc_m.redraw_count;

    wuss_window_move(win_m, POINT(10, 10)); /* slide back: the part that re-enters the screen was
                                       * never blitted (its source pixels were off-screen),
                                       * so it must be a real task redraw, not a blit */
    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_m.redraw_count != before_m + 1)
      goto Failure; /* M must get a genuine redraw call to repaint the reappeared part */

    wuss_window_close(win_m);
  }

  printf("test: Adjust-click on a window's back icon brings it to front\n");

  {
    static test_task_t tc_g, tc_h;
    wuss_task_t       *delegate_g, *delegate_h;
    box_t              box_g, box_h;
    wuss_window_t     *win_g, *win_h;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h = mk_task(wuss, test_handle, &tc_h);
    if (delegate_h == NULL) goto Failure;

    box_h.x0 = 130; box_h.y0 = 50;
    box_h.x1 = 190; box_h.y1 = 100;
    rc = wuss_window_create(delegate_h,
                            &box_h,
                            "H",
                            wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_h),
                            SIZE2D(0, 0),
                            &win_h);
    if (rc != result_OK)
      goto Failure;

    tc_g.redraw_count = 0;
    tc_g.mouse_count  = 0;
    delegate_g = mk_task(wuss, test_handle, &tc_g);
    if (delegate_g == NULL) goto Failure;

    box_g.x0 = 110; box_g.y0 = 30;
    box_g.x1 = 160; box_g.y1 = 80;
    rc = wuss_window_create(delegate_g,
                            &box_g,
                            "G",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL,
                            wuss_NO_BACKDROP,
                            box_size(&box_g),
                            SIZE2D(0, 0),
                            &win_g);
    if (rc != result_OK)
      goto Failure;

    /* G is topmost here, overlapping H; G's own back icon (top-left
     * corner) never falls under H, so it stays clickable either way */
    wuss_window_get_visible_bounds(win_g, &visible);

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap of G and H */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 5, visible.y0 + 5), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's back icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 5, visible.y0 + 5), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: H now on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_h) /* G was sent to back */
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 5, visible.y0 + 5), wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* Adjust-click G's back icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 5, visible.y0 + 5), wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: G back on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g) /* Adjust-click on the back icon brought G back to front */
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    printf("test: drag-resize stops at doc_width/doc_height, not just WUSS_MIN_CONTENT\n");

    wuss_window_get_content_bounds(win_g, &content); /* G's doc_width/doc_height are 50x50, same as its initial content size */

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(content.x0 + 50, content.y0 + 50), &hit); /* drag to exactly the doc extent */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(content.x0 + 50, content.y0 + 50), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_g, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's resize icon again */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(content.x0 + 500, content.y0 + 500), &hit); /* drag far past the doc extent */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(content.x0 + 500, content.y0 + 500), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_g, &content);
    if (content.x1 - content.x0 != width || content.y1 - content.y0 != height)
      goto Failure; /* clamped to the same size as dragging to exactly the doc extent: not left to grow past it */

    wuss_window_close(win_g);
    wuss_window_close(win_h);
  }

  printf("test: drag-resize stops at min_doc, not just WUSS_MIN_CONTENT\n");

  {
    static test_task_t tc_m;
    wuss_task_t       *delegate_m;
    box_t              box_m, content, visible;
    wuss_window_t     *win_m;

    tc_m.redraw_count    = 0;
    tc_m.mouse_count     = 0;
    delegate_m = mk_task(wuss, test_handle, &tc_m);
    if (delegate_m == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 210; box_m.y1 = 210; /* 200x200 content, floored at 80x60 */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(200, 200), SIZE2D(80, 60),
                            &win_m);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_m, &visible);
    wuss_window_get_content_bounds(win_m, &content);

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* M's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_m)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(content.x0 + 5, content.y0 + 5), &hit); /* drag far inside min_doc */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(content.x0 + 5, content.y0 + 5), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_m, &content);
    if (content.x1 - content.x0 != 80 || content.y1 - content.y0 != 60)
      goto Failure; /* floored at min_doc, not squeezed down to WUSS_MIN_CONTENT */

    wuss_window_close(win_m);
  }

  printf("test: drag-resize never grows a window's total size past the screen's, on either axis\n");

  {
    static test_task_t tc_s;
    wuss_task_t       *delegate_s;
    box_t              box_s, content, visible;
    wuss_window_t     *win_s;

    tc_s.redraw_count = 0;
    tc_s.mouse_count  = 0;
    delegate_s = mk_task(wuss, test_handle, &tc_s);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 30; box_s.y1 = 30; /* 20x20 content; doc is 1000x1000, far bigger than the 200x200 screen */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_s, &visible);

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* S's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_s)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(1000, 1000), &hit); /* drag far past the 200x200 screen */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(1000, 1000), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_s, &visible);
    if (visible.x1 - visible.x0 > 200 || visible.y1 - visible.y0 > 200)
      goto Failure; /* total footprint capped at the screen size, wherever the window sits */

    wuss_window_close(win_s);
  }

  {
    /* Same cap, but for a window moved partway off the top-left of the
     * screen: the mouse pointer itself is clamped to the screen (see
     * wuss_mouse_move), so a positive content.x0 can never see a resize
     * drag reach far enough to overhang -- the gap only shows up once
     * content.x0 is negative, letting the drag's on-screen pointer range
     * translate into a width bigger than the screen itself. */
    static test_task_t tc_e;
    wuss_task_t       *delegate_e;
    box_t              box_e, visible;
    wuss_window_t     *win_e;

    tc_e.redraw_count = 0;
    tc_e.mouse_count  = 0;
    delegate_e = mk_task(wuss, test_handle, &tc_e);
    if (delegate_e == NULL) goto Failure;

    box_e.x0 = 10; box_e.y0 = 10;
    box_e.x1 = 30; box_e.y1 = 30; /* 20x20 content; doc is 1000x1000, far bigger than the 200x200 screen */
    rc = wuss_window_create(delegate_e, &box_e, "E", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(0, 0), &win_e);
    if (rc != result_OK)
      goto Failure;

    wuss_window_move(win_e, POINT(-100, -100)); /* content top-left off-screen, top-left corner */

    wuss_window_get_visible_bounds(win_e, &visible);

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* E's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_e)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(1000, 1000), &hit); /* drag to the far corner; the pointer itself clamps to (199,199) */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(1000, 1000), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_e, &visible);
    if (visible.x1 - visible.x0 > 200 || visible.y1 - visible.y0 > 200)
      goto Failure; /* total footprint still capped at the screen size, even with a negative x0/y0 */

    wuss_window_close(win_e);
  }

  printf("test: toggle-size blits rather than redrawing the whole window\n");

  {
    static test_task_t tc_t;
    wuss_task_t       *delegate_t;
    box_t              box_t_win, before, after, titlebar, toggle;
    wuss_window_t     *win_t;
    int                outline_px, titlebar_height, inset, icon;
    int                i, dirty_area, full_area, cx, cy, old_icon_x, old_icon_y, found;
    int                interior_x, interior_y, interior_dirty;
    int                old_vscroll_x, old_vscroll_y, old_vscroll_found;

    tc_t.redraw_count = 0;
    tc_t.mouse_count  = 0;
    delegate_t = mk_task(wuss, test_handle, &tc_t);
    if (delegate_t == NULL) goto Failure;

    box_t_win.x0 = 10; box_t_win.y0 = 10;
    box_t_win.x1 = 50; box_t_win.y1 = 50; /* 40x40 content, room to grow to a 150x150 doc without the toggled box needing to be repositioned off (10,10) -- this test is about the in-place grow blit */
    rc = wuss_window_create(delegate_t, &box_t_win, "T", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(150, 150), SIZE2D(0, 0), &win_t);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    /* toggle icon: top-right of the titlebar, inset by 3px, sized 20 - 2*3
     * (default titlebar height 20, WUSS_BUTTON_INSET 3), matching
     * wuss__toggle_box's formula -- mirrored here since the test only sees
     * the public API */
    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_t, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;
    old_icon_x = cx; old_icon_y = cy; /* pre-grow icon centre: ends up mid-titlebar once the window widens */

    /* pre-grow content interior, well clear of outline/titlebar/scrollbar
     * furniture on every side (carve.x == carve.y == icon here, since both
     * scrollbars are enabled) -- a point the blit must genuinely have
     * preserved, unlike a naive summed-region-area comparison, which
     * overcounts once furniture invalidation adds several regions that
     * overlap each other and the grown-edge region without merging (only
     * exact-edge-aligned boxes merge; see box_merge in invalidate.c) */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    /* pre-grow vscroll column, at the *old* right edge: once the window
     * widens this sits mid-content rather than at the (now further right)
     * new column, inside the region the blit reuses as valid pixels --
     * the content redraw never touches it, so only a forced old-furniture
     * invalidate stops the old scrollbar glyph being left behind there */
    old_vscroll_x = before.x1 - outline_px - icon / 2;
    old_vscroll_y = interior_y;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* T's toggle-size icon: grow */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_t)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    found             = 0;
    interior_dirty    = 0;
    old_vscroll_found = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, old_icon_x, old_icon_y))
        found = 1;
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
      if (box_contains_point(&region, old_vscroll_x, old_vscroll_y))
        old_vscroll_found = 1;
    }
    if (!old_vscroll_found)
      goto Failure; /* the old vscroll column, now mid-content rather than at
                      * the (further right) new right edge, falls inside both
                      * "before" and the grown "visible" same as the toggle
                      * icon above -- the blit alone leaves its stale pixels
                      * on screen unless the old furniture position is also
                      * forced dirty */
    if (!found)
      goto Failure; /* the old toggle-icon glyph, now mid-titlebar rather than
                      * at its corner, falls inside both "before" and the
                      * grown "visible" -- the content blit alone would leave
                      * it un-redrawn as a ghost; furniture must be forced
                      * dirty separately since its layout depends on size */

    if (interior_dirty)
      goto Failure; /* interior content pixel, untouched by any furniture or
                      * grown-edge region: the blit must have reused it */

    wuss_window_get_visible_bounds(win_t, &after);
    if (after.x1 > 200 || after.y1 > 200)
      goto Failure; /* toggling must leave the window fully on-screen */
    if (after.x0 != before.x0 || after.y0 != before.y0)
      goto Failure; /* a 150x150 doc toggled from (10,10) fits without moving,
                      * so the top-left stays put and the in-place grow blit
                      * (asserted below) applies */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. Icon moved with the grown titlebar, so recompute. */
    wuss_window_get_visible_bounds(win_t, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* T's toggle-size icon: shrink */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_t)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    dirty_area = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      dirty_area += (region.x1 - region.x0) * (region.y1 - region.y0);
    }

    full_area = (before.x1 - before.x0) * (before.y1 - before.y0); /* the grown box: what a full-union invalidate would have covered */
    if (dirty_area >= full_area)
      goto Failure; /* vacated edge only, not the whole grown footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_t);
  }

  printf("test: toggle-size that forces a scroll re-clamp invalidates the content it's about to redraw at the new offset, not just the blit's edge sliver\n");

  {
    static test_task_t tc_r;
    wuss_task_t       *delegate_r;
    box_t              box_r, before, titlebar, toggle;
    wuss_window_t     *win_r;
    int                outline_px, titlebar_height, inset, icon;
    int                i, interior_x, interior_y, interior_dirty, cx, cy;

    tc_r.redraw_count = 0;
    tc_r.mouse_count  = 0;
    delegate_r = mk_task(wuss, test_handle, &tc_r);
    if (delegate_r == NULL) goto Failure;

    box_r.x0 = 10; box_r.y0 = 10;
    box_r.x1 = 50; box_r.y1 = 50; /* 40x40 content; doc bigger than that, so it starts scrollable */
    rc = wuss_window_create(delegate_r, &box_r, "R", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(70, 70), SIZE2D(0, 0), &win_r);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_r, POINT(0, 15)); /* within range: max_y = 70 - 40 = 30 */

    rc = wuss_redraw_dirty(wuss); /* flush the scroll's own invalidate */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_r, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    /* interior of the OLD content box, clear of outline/titlebar/scrollbar
     * furniture -- well inside "before", so a plain grow (no re-clamp) would
     * leave it untouched by the blit-reuse optimisation, same as the
     * "toggle-size blits" test above. But this window starts scrolled, and
     * growing to doc_height (70) here forces content_size up to doc_size,
     * clamping scroll.y back to 0 -- the content this point shows is stale
     * regardless of the blit, so it must be invalidated outright. */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* R's toggle-size icon: grow past doc_height, forcing a scroll re-clamp */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_r)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (!interior_dirty)
      goto Failure; /* the re-clamp changed scroll.y, so every pixel in the
                      * content box is now showing the wrong offset -- if
                      * this interior point (untouched by the toggle's own
                      * blit-reuse/furniture invalidation) isn't marked
                      * dirty, the fix has regressed to relying on
                      * wuss_window_set_scroll's live blit-and-shift, which
                      * is invalid mid-toggle: the screen doesn't reflect
                      * the new geometry yet at that point */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_r);
  }

  printf("test: wuss_window_resize on a topmost window at max scroll blits the still-valid content rather than redrawing it all\n");

  {
    /* Unlike the toggle-size path above, a direct wuss_window_resize (the
     * interactive drag-resize path) runs with the screen still showing this
     * window's content at the old geometry and old scroll offset. When the
     * grow forces a scroll re-clamp, the overlap of the old and new content
     * boxes is genuine on-screen content that just needs sliding by the
     * scroll delta -- only the newly-exposed strip needs a real repaint. */
    static test_task_t tc_d;
    wuss_task_t       *delegate_d;
    box_t              box_d, content_before;
    wuss_window_t     *win_d;
    point_t            scroll_d;
    int                i;
    int                top_x, top_y, mid_x, mid_y, bottom_x, bottom_y;
    int                top_dirty, mid_dirty, bottom_dirty;

    tc_d.redraw_count = 0;
    tc_d.mouse_count  = 0;
    delegate_d = mk_task(wuss, test_handle, &tc_d);
    if (delegate_d == NULL) goto Failure;

    box_d.x0 = 10; box_d.y0 = 10;
    box_d.x1 = 90; box_d.y1 = 90; /* 80x80 content; doc taller, so it starts scrollable */
    rc = wuss_window_create(delegate_d, &box_d, "D", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 140), SIZE2D(0, 0), &win_d);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_d, &content_before);

    /* scroll to the bottom: max_y = doc.h (140) - content height (80) = 60 */
    wuss_window_set_scroll(win_d, POINT(0, 60));
    rc = wuss_redraw_dirty(wuss); /* flush the scroll's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_scroll(win_d, &scroll_d);
    if (scroll_d.y != 60)
      goto Failure;

    /* Three probe points at the same x, well clear of the scrollbar column.
     * The re-clamp drops scroll.y from 60 to 0, so on-screen content slides
     * DOWN by 60px: the top ~60px of the content box is newly revealed and
     * must be repainted, everything below it was already on screen and must
     * be reused by the blit. */
    top_x    = content_before.x0 + 8;
    top_y    = content_before.y0 + 8;   /* < 60px down: in the revealed strip */
    mid_x    = content_before.x0 + 8;
    mid_y    = content_before.y0 + 70;  /* > 60px down: reused */
    bottom_x = content_before.x0 + 8;
    bottom_y = content_before.y1 - 8;   /* near old viewport bottom: reused */

    /* grow the content taller than the doc: content height goes to >= 140,
     * so max_y drops to 0 and scroll.y is clamped back from 60 to 0 */
    rc = wuss_window_resize(win_d, SIZE2D(80, 160));
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_scroll(win_d, &scroll_d);
    if (scroll_d.y != 0)
      goto Failure; /* the grow must have re-clamped the offset */

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    top_dirty    = 0;
    mid_dirty    = 0;
    bottom_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, top_x, top_y))
        top_dirty = 1;
      if (box_contains_point(&region, mid_x, mid_y))
        mid_dirty = 1;
      if (box_contains_point(&region, bottom_x, bottom_y))
        bottom_dirty = 1;
    }

    if (!top_dirty)
      goto Failure; /* the newly-revealed strip must be repainted */

    if (mid_dirty || bottom_dirty)
      goto Failure; /* content that stayed on screen must be reused by the
                      * blit-and-shift, not redrawn -- a full content-box
                      * invalidate has regressed the optimisation */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_d);
  }

  printf("test: wuss_window_resize on a non-topmost window blits the un-occluded content and never dirties the occluder\n");

  {
    /* Another window covers the right half of D's content. The blit source
     * is the old content box MINUS what the occluder covered, so the
     * un-occluded left half is slid into place; the right half still shows
     * the occluder's own correct pixels, so it must stay clean -- dirtying
     * it would redraw the occluding window for nothing. Only the un-occluded
     * newly-revealed top strip needs a repaint. */
    static test_task_t tc_d, tc_o;
    wuss_task_t       *delegate_d, *delegate_o;
    box_t              box_d, box_o, content_before;
    wuss_window_t     *win_d, *win_o;
    point_t            scroll_d;
    int                i, split_x;
    int                occluded_x, occluded_y, kept_x, kept_y, strip_x, strip_y;
    int                occluded_dirty, kept_dirty, strip_dirty;

    tc_d.redraw_count = 0; tc_d.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_d = mk_task(wuss, test_handle, &tc_d);
    if (delegate_d == NULL) goto Failure;
    delegate_o = mk_task(wuss, test_handle, &tc_o);
    if (delegate_o == NULL) goto Failure;

    box_d.x0 = 10; box_d.y0 = 10;
    box_d.x1 = 110; box_d.y1 = 90; /* 100x80 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_d, &box_d, "D", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(100, 140), SIZE2D(0, 0), &win_d);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_d, &content_before);
    split_x = (content_before.x0 + content_before.x1) / 2;

    /* occluder covering the right half of D's content box and beyond */
    box_o.x0 = split_x; box_o.y0 = content_before.y0 - 5;
    box_o.x1 = content_before.x1 + 40; box_o.y1 = content_before.y1 + 40;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_o.x1 - box_o.x0, box_o.y1 - box_o.y0),
                            SIZE2D(0, 0), &win_o);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates */
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_d, POINT(0, 60)); /* max_y = 140 - 80 = 60 */
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_scroll(win_d, &scroll_d);
    if (scroll_d.y != 60)
      goto Failure;

    /* occluded: under O -- must stay clean. kept: un-occluded, below the
     * ~60px revealed strip -- reused by the blit, must stay clean. strip:
     * un-occluded, in the newly-revealed top band -- must be repainted. */
    occluded_x = split_x + 8;
    occluded_y = content_before.y1 - 8;
    kept_x     = content_before.x0 + 8;
    kept_y     = content_before.y1 - 8;
    strip_x    = content_before.x0 + 8;
    strip_y    = content_before.y0 + 8;

    rc = wuss_window_resize(win_d, SIZE2D(100, 160)); /* grow past doc height */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_scroll(win_d, &scroll_d);
    if (scroll_d.y != 0)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    occluded_dirty = 0;
    kept_dirty     = 0;
    strip_dirty    = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, occluded_x, occluded_y))
        occluded_dirty = 1;
      if (box_contains_point(&region, kept_x, kept_y))
        kept_dirty = 1;
      if (box_contains_point(&region, strip_x, strip_y))
        strip_dirty = 1;
    }

    if (occluded_dirty)
      goto Failure; /* under the occluder: its pixels are already correct,
                      * dirtying this would redraw the occluding window */

    if (kept_dirty)
      goto Failure; /* un-occluded content must be reused by the blit */

    if (!strip_dirty)
      goto Failure; /* the un-occluded newly-revealed strip must be repainted */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_o);
    wuss_window_close(win_d);
  }

  printf("test: wuss_window_set_scroll on a non-topmost window blits the un-occluded content and never dirties the occluder\n");

  {
    /* A window covered on its right half by another still blits its
     * un-occluded left half on a programmatic scroll. The right half shows
     * the occluder's own correct pixels, so it must stay clean -- dirtying
     * it would redraw the occluding window. Only the un-occluded
     * newly-exposed edge strip needs a repaint. */
    static test_task_t tc_s, tc_o;
    wuss_task_t       *delegate_s, *delegate_o;
    box_t              box_s, box_o, content;
    wuss_window_t     *win_s, *win_o;
    int                i, split_x;
    int                occluded_x, occluded_y, kept_x, kept_y, strip_x, strip_y;
    int                occluded_dirty, kept_dirty, strip_dirty;

    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_s = mk_task(wuss, test_handle, &tc_s);
    if (delegate_s == NULL) goto Failure;
    delegate_o = mk_task(wuss, test_handle, &tc_o);
    if (delegate_o == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 110; box_s.y1 = 90; /* 100x80 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(100, 200), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_s, &content);
    split_x = (content.x0 + content.x1) / 2;

    box_o.x0 = split_x; box_o.y0 = content.y0 - 5;
    box_o.x1 = content.x1 + 40; box_o.y1 = content.y1 + 40;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_o.x1 - box_o.x0, box_o.y1 - box_o.y0),
                            SIZE2D(0, 0), &win_o);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates */
    if (rc != result_OK)
      goto Failure;

    /* kept: un-occluded, near the bottom -- content there just slides up by
     * the scroll delta, so the blit must reuse it. occluded: under O -- must
     * stay clean. strip: un-occluded, in the newly-exposed bottom band --
     * must be repainted. */
    kept_x     = content.x0 + 8;
    kept_y     = content.y0 + 8;
    occluded_x = split_x + 8;
    occluded_y = content.y1 - 20;
    strip_x    = content.x0 + 8;
    strip_y    = content.y1 - 6;

    wuss_window_set_scroll(win_s, POINT(0, 12));

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    kept_dirty     = 0;
    occluded_dirty = 0;
    strip_dirty    = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, kept_x, kept_y))
        kept_dirty = 1;
      if (box_contains_point(&region, occluded_x, occluded_y))
        occluded_dirty = 1;
      if (box_contains_point(&region, strip_x, strip_y))
        strip_dirty = 1;
    }

    if (occluded_dirty)
      goto Failure; /* under the occluder: its pixels are already correct,
                      * dirtying this would redraw the occluding window */

    if (kept_dirty)
      goto Failure; /* un-occluded content must be reused by the blit */

    if (!strip_dirty)
      goto Failure; /* the un-occluded newly-exposed strip must be repainted */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_o);
    wuss_window_close(win_s);
  }

  printf("test: wuss_window_set_scroll never blits a scrolled window's content over a mid-content occluder\n");

  {
    /* Regression: a small window O sits in the middle of a larger scrollable
     * window M's content, covering neither M's top nor bottom edge. A
     * programmatic vertical scroll of M takes a blit source slice from below
     * O and shifts it up by the scroll delta, so its destination overlaps O.
     * screen_copy_rect only clips to the content box, so without the fix it
     * paints M's content over O and marks that area "copied" -- excluding it
     * from the repaint set, so O's pixels stay overpainted with M's colour.
     * With the fix each destination is clipped against the occluders and only
     * the un-occluded sub-pieces are blitted, leaving O's own pixels intact.
     * Read the framebuffer behind O to check. */
    colour_t           cm;
    uint32_t           fb_m, fb_o;
    static test_task_t tc_m, tc_o;
    wuss_task_t       *delegate_m, *delegate_o;
    box_t              box_m, box_o, content;
    wuss_window_t     *win_m, *win_o;
    int                occ_x, occ_y, mid_x, mid_y, delta;

    cm = colour_rgb(0x11, 0x22, 0x33);
    tc_m.redraw_count = 0; tc_m.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_m = mk_task(wuss, paint_handle, &cm);
    if (delegate_m == NULL) goto Failure;
    delegate_o = mk_task(wuss, paint_handle, NULL);
    if (delegate_o == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 110; box_m.y1 = 110; /* 100x100 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(100, 300), SIZE2D(0, 0), &win_m);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_m, &content);

    /* occluder spanning M's full content width, a band in the vertical
     * middle -- so wuss__clip_to_visible splits M's blit source into a top
     * and a bottom band with nothing behind O. Scrolling down slides the
     * bottom band up; part of its destination lands behind O, yet no dirty
     * repaint region touches O, so without the fix O stays overpainted. */
    box_o.x0 = content.x0 - 5;  box_o.y0 = content.y0 + 40;
    box_o.x1 = content.x1 + 5;  box_o.y1 = content.y0 + 70;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_o.x1 - box_o.x0, box_o.y1 - box_o.y0),
                            SIZE2D(0, 0), &win_o);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates: M then O paint */
    if (rc != result_OK)
      goto Failure;

    /* sample the framebuffer: occ_* sits behind O, mid_* sits in M's content
     * clear of O -- record what each colour actually renders as */
    occ_x = (box_o.x0 + box_o.x1) / 2;
    occ_y = box_o.y1 - 3; /* near O's bottom: the bottom band's blit
                           * destination reaches up to here */
    mid_x = content.x0 + 5;
    mid_y = content.y0 + 5;
    fb_o  = ((const uint32_t *) pixels)[occ_y * 200 + occ_x];
    fb_m  = ((const uint32_t *) pixels)[mid_y * 200 + mid_x];
    if (fb_o == fb_m)
      goto Failure; /* the two windows must render distinguishable pixels */

    delta = 20;
    wuss_window_set_scroll(win_m, POINT(0, delta));
    rc = wuss_redraw_dirty(wuss); /* apply the scroll's blit + any repaint */
    if (rc != result_OK)
      goto Failure;

    /* the pixel behind O must still be O's, not M's content blitted over it */
    if (((const uint32_t *) pixels)[occ_y * 200 + occ_x] != fb_o)
      goto Failure;

    wuss_window_close(win_o);
    wuss_window_close(win_m);
  }

  printf("test: wuss_window_set_scroll orders its blit sub-pieces so one never clobbers another's source\n");

  {
    /* Regression: a small window O floats in the middle of a larger
     * scrollable window M, with a gap all round it. wuss__clip_to_visible
     * carves M's blittable content into bands around O; a vertical scroll
     * shifts each band by the same delta, and one band's shifted
     * destination lands on another band's still-unread source. Blitting the
     * bands in clip-emit order corrupts M's own content -- pixels near O's
     * bottom edge end up double-shifted. The blit must be ordered (or fall
     * back). paint_handle paints M as one-pixel rows whose blue channel
     * encodes document Y, so a mis-shifted row is detectable exactly. */
    colour_t           cm;
    static test_task_t tc_m, tc_o;
    wuss_task_t       *delegate_m, *delegate_o;
    box_t              box_m, box_o, content, ovis;
    wuss_window_t     *win_m, *win_o;
    int                gx, gy, delta, bad;

    cm = colour_rgb(0x11, 0x22, 0x33);
    tc_m.redraw_count = 0; tc_m.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_m = mk_task(wuss, paint_handle, &cm);
    if (delegate_m == NULL) goto Failure;
    delegate_o = mk_task(wuss, paint_handle, NULL);
    if (delegate_o == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 110; box_m.y1 = 110; /* 100x100 content; doc taller, scrollable */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(100, 300), SIZE2D(0, 0), &win_m);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_m, &content);

    /* O strictly inside M's content, gap on every side */
    box_o.x0 = content.x0 + 25; box_o.y0 = content.y0 + 25;
    box_o.x1 = content.x0 + 75; box_o.y1 = content.y0 + 65;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_o.x1 - box_o.x0, box_o.y1 - box_o.y0),
                            SIZE2D(0, 0), &win_o);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates */
    if (rc != result_OK)
      goto Failure;

    /* probe the gap column just right of O: pure M content, must slide up by
     * exactly the scroll delta with no discontinuity */
    wuss_window_get_visible_bounds(win_o, &ovis);
    gx    = (ovis.x1 + content.x1) / 2;
    delta = 20;

    wuss_window_set_scroll(win_m, POINT(0, delta));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* every row in the gap column, across O's vertical span, must show the
     * document Y that scrolling put there: doc_y = (screen_y - content.y0) +
     * delta, low byte carried in blue by paint_handle */
    bad = 0;
    for (gy = ovis.y0 + 2; gy < ovis.y1 - 2; gy++)
    {
      uint32_t px, blue;

      if (gy < content.y0 || gy >= content.y1)
        continue;
      px   = ((const uint32_t *) pixels)[gy * 200 + gx];
      blue = px & 0xff;
      if (blue != (uint32_t) ((gy - content.y0 + delta) & 0xff))
        bad = 1;
    }
    if (bad)
      goto Failure; /* a band blit clobbered another band's source */

    wuss_window_close(win_o);
    wuss_window_close(win_m);
  }

  printf("test: scrolling a window shrunk below its document size never marks a neighbour's screen area dirty\n");

  {
    /* Regression: a window resized down below its document extent (so it
     * becomes scrollable), then scrolled by wheel/scrollbar, must not touch
     * a neighbour window's own screen area. flood_full_bounds_handle ignores
     * "clip" entirely, mirroring a real task (e.g. saturn.c) that redraws
     * its full sample space every call rather than honouring clip -- the
     * wuss-core clip machinery, not the task, must be what keeps such a
     * task's pixels inside its own window. */
    colour_t           s_colour, n_colour;
    static test_task_t tc_s, tc_n;
    wuss_task_t       *delegate_s, *delegate_n;
    box_t              box_s, box_n, content_n;
    wuss_window_t     *win_s, *win_n;
    int                nx, ny, bad;

    s_colour = colour_rgb(0xcc, 0xdd, 0xee);
    n_colour = colour_rgb(0x40, 0x60, 0x80);
    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    tc_n.redraw_count = 0; tc_n.mouse_count = 0;
    delegate_s = mk_task(wuss, flood_full_bounds_handle, &s_colour);
    if (delegate_s == NULL) goto Failure;
    delegate_n = mk_task(wuss, flood_full_bounds_handle, &n_colour);
    if (delegate_n == NULL) goto Failure;

    /* S starts full-sized (matches its document, so no scrollbars yet), then
     * gets shrunk -- exercising wuss_window_resize's scroll-reclamp path,
     * distinct from creating it small to begin with. */
    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 90; box_s.y1 = 90;
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 80), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    /* N sits immediately to the right of S's outer box, close enough that a
     * blit destination sliding even a few pixels past S's own bounds lands
     * on N. */
    box_n.x0 = box_s.x1 + 2; box_n.y0 = 10;
    box_n.x1 = box_n.x0 + 40; box_n.y1 = 90;
    rc = wuss_window_create(delegate_n, &box_n, "N", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_n.x1 - box_n.x0, box_n.y1 - box_n.y0),
                            SIZE2D(0, 0), &win_n);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates */
    if (rc != result_OK)
      goto Failure;

    /* shrink S well below its 80x80 document -- this window is now
     * scrollable */
    rc = wuss_window_resize(win_s, SIZE2D(30, 30));
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* scroll it, as a wheel/scrollbar-drag would */
    wuss_window_set_scroll(win_s, POINT(20, 20));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* every pixel of N's own content (not its titlebar/outline chrome) must
     * still show exactly N's own flood colour */
    wuss_window_get_content_bounds(win_n, &content_n);
    bad = 0;
    for (ny = content_n.y0; ny < content_n.y1 && !bad; ny++)
      for (nx = content_n.x0; nx < content_n.x1; nx++)
      {
        uint32_t px;

        px = ((const uint32_t *) pixels)[ny * 200 + nx];
        if ((px & 0xffffff) != 0x406080)
        {
          bad = 1;
          break;
        }
      }
    if (bad)
      goto Failure; /* S's scroll blit painted over N's screen area */

    wuss_window_close(win_n);
    wuss_window_close(win_s);
  }

  printf("test: shrinking a window repaints its content right up to the new content box's own edge\n");

  {
    /* Regression: dragging a resize handle to make a window smaller must
     * leave no stale sliver between the client's repainted content and the
     * furniture (scrollbar/outline) now sitting at the new, smaller edge. */
    static test_task_t tc_s;
    wuss_task_t       *delegate_s;
    box_t              box_s, content_s;
    wuss_window_t     *win_s;
    colour_t           s_colour;
    int                nx, ny, bad;

    s_colour = colour_rgb(0xcc, 0xdd, 0xee);
    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    delegate_s = mk_task(wuss, flood_full_bounds_handle, &s_colour);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 90; box_s.y1 = 90;
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 80), SIZE2D(200, 200), &win_s);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss); /* flush the create, paint S's initial content */
    if (rc != result_OK)
      goto Failure;

    /* scroll before shrinking -- the shrink must not leave a stale sliver
     * at the new content edge regardless of scroll offset */
    wuss_window_set_scroll(win_s, POINT(20, 20));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* an interactive drag delivers many small pointer-move events, each
     * calling wuss_window_resize, before the display ever gets to redraw --
     * shrink one pixel at a time with no flush between them, matching that;
     * throw in an overshoot-and-correct wobble too, as a real drag does */
    {
      int step, w;

      for (step = 0, w = 80; w > 30; w--, step++)
      {
        rc = wuss_window_resize(win_s, SIZE2D(w, w));
        if (rc != result_OK)
          goto Failure;
      }
      rc = wuss_window_resize(win_s, SIZE2D(35, 35)); /* overshoot back out */
      if (rc != result_OK)
        goto Failure;
      rc = wuss_window_resize(win_s, SIZE2D(30, 30)); /* settle */
      if (rc != result_OK)
        goto Failure;
    }
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* every pixel of S's own (new, smaller) content box must show S's
     * flood colour right up to content_s.x1-1 / content_s.y1-1 -- no gap
     * between the repainted content and the furniture now abutting it */
    wuss_window_get_content_bounds(win_s, &content_s);
    bad = 0;
    for (ny = content_s.y0; ny < content_s.y1 && !bad; ny++)
      for (nx = content_s.x0; nx < content_s.x1; nx++)
      {
        uint32_t px;

        px = ((const uint32_t *) pixels)[ny * 200 + nx];
        if ((px & 0xffffff) != 0xccddee)
        {
          bad = 1;
          break;
        }
      }
    if (bad)
      goto Failure; /* stale sliver left along the new content box's edge */

    wuss_window_close(win_s);
  }

  printf("test: a fast resize-grow past the scroll clamp never blits a not-yet-painted sliver\n");

  {
    /* Regression: wuss_window_resize's scroll-reclamp blit (window scrolled
     * to the document's bottom/right, then grown so the bigger viewport
     * needs less scroll and the clamp pulls the offset back) reads its blit
     * source straight from wuss__clip_to_visible over the *old* content box,
     * unlike wuss_window_move/wuss_window_set_scroll which both also strip
     * anything still sitting in wuss->dirty[] (queued by an earlier call this
     * same frame, not yet painted). An interactive drag delivers several
     * resize calls before a redraw ever runs: one grow queues its
     * newly-exposed sliver dirty without painting it; that sliver sits
     * inside the *next* call's "old content box", so the next call's
     * scroll-reclamp blit slides it as if it were this window's own settled
     * rendering -- smearing a stale/undrawn sliver that no later invalidate
     * ever repaints. paint_handle's per-row blue-encoded doc_y makes a
     * wrongly-slid row detectable exactly. */
    static test_task_t tc_s;
    wuss_task_t       *delegate_s;
    box_t              box_s, content_s;
    wuss_window_t     *win_s;
    colour_t           s_colour;
    point_t            scroll;
    int                sy, sx, bad;

    s_colour = colour_rgb(0x11, 0x22, 0x33);
    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    delegate_s = mk_task(wuss, paint_handle, &s_colour);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 40; box_s.y1 = 40; /* 30x30 content; a doc far bigger than the
                                   * screen so growing 1px at a time pulls the
                                   * scroll clamp back by exactly 1px per
                                   * call, keeping it non-zero (and so
                                   * re-triggering the reclamp path) across
                                   * the whole grow range below */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(30, 30), &win_s);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss); /* flush the create, paint S's initial content */
    if (rc != result_OK)
      goto Failure;

    /* scroll to the document's bottom-right: no further offset possible at
     * this size */
    wuss_window_set_scroll(win_s, POINT(970, 970));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* grow two calls at a time with no flush between them, as an interactive
     * drag delivers several pointer-move events per rendered frame -- each
     * bigger size pulls the scroll clamp back by 1px, so every single call
     * re-enters the reclamp path, and the second call of each pair reads its
     * blit source over ground the first call of the pair only just
     * invalidated (its own grown sliver) but never painted. Flushing after
     * every pair (rather than only once at the end) matches a real redraw
     * loop -- it also stops one pair's leftover full-footprint dirty rect
     * from coalescing over a later pair's corruption and masking it. */
    {
      int w;

      for (w = 30; w <= 90; w += 2)
      {
        rc = wuss_window_resize(win_s, SIZE2D(w, w));
        if (rc != result_OK)
          goto Failure;
        rc = wuss_window_resize(win_s, SIZE2D(w + 1, w + 1));
        if (rc != result_OK)
          goto Failure;

        rc = wuss_redraw_dirty(wuss);
        if (rc != result_OK)
          goto Failure;

        /* every row of S's current content box must show exactly the
         * document Y that its current scroll offset puts there -- a
         * stale-sliver blit leaves some row(s) showing a stale doc_y (or
         * whatever else was sitting in the framebuffer) instead */
        wuss_window_get_content_bounds(win_s, &content_s);
        wuss_window_get_scroll(win_s, &scroll);
        bad = 0;
        for (sy = content_s.y0; sy < content_s.y1 && !bad; sy++)
        {
          uint32_t px, blue, want;

          want = (uint32_t) ((sy - content_s.y0 + scroll.y) & 0xff);
          for (sx = content_s.x0; sx < content_s.x1; sx++)
          {
            px   = ((const uint32_t *) pixels)[sy * 200 + sx];
            blue = px & 0xff;
            if (blue != want)
            {
              bad = 1;
              break;
            }
          }
        }
        if (bad)
          goto Failure; /* a not-yet-painted sliver got blitted as if settled */
      }
    }

    wuss_window_close(win_s);
  }

  printf("test: a fast resize drag that jumps size and reverses direction never blits a not-yet-painted sliver\n");

  {
    /* Same hazard as the paired-grow test above, but matching a real
     * interactive drag more closely: the pointer can jump the size by a
     * large delta in one event (not just 1px), and can reverse from
     * growing to shrinking and back within the same unflushed batch. A
     * shrink never re-enters the reclamp path itself (it only ever raises
     * the clamp ceiling), but it still queues its own shrunk-away sliver
     * dirty -- and the very next call, if it grows back past the clamp,
     * must not treat that still-unpainted sliver as settled ground either. */
    static test_task_t tc_e;
    wuss_task_t       *delegate_e;
    box_t              box_e, content_e;
    wuss_window_t     *win_e;
    colour_t           e_colour;
    point_t            scroll;
    int                sy, sx, bad, step;
    int                sizes[] = { 30, 90, 35, 120, 31, 90, 30, 60, 110, 30 };

    e_colour = colour_rgb(0x11, 0x22, 0x33);
    tc_e.redraw_count = 0; tc_e.mouse_count = 0;
    delegate_e = mk_task(wuss, paint_handle, &e_colour);
    if (delegate_e == NULL) goto Failure;

    box_e.x0 = 10; box_e.y0 = 10;
    box_e.x1 = 40; box_e.y1 = 40;
    rc = wuss_window_create(delegate_e, &box_e, "E", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(30, 30), &win_e);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_e, POINT(970, 970));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    for (step = 0; step < 9; step += 2)
    {
      rc = wuss_window_resize(win_e, SIZE2D(sizes[step], sizes[step]));
      if (rc != result_OK)
        goto Failure;
      rc = wuss_window_resize(win_e, SIZE2D(sizes[step + 1], sizes[step + 1]));
      if (rc != result_OK)
        goto Failure;

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_content_bounds(win_e, &content_e);
      wuss_window_get_scroll(win_e, &scroll);
      content_e.x1 = MIN(content_e.x1, 200);
      content_e.y1 = MIN(content_e.y1, 200);
      bad = 0;
      for (sy = content_e.y0; sy < content_e.y1 && !bad; sy++)
      {
        uint32_t px, blue, want;

        want = (uint32_t) ((sy - content_e.y0 + scroll.y) & 0xff);
        for (sx = content_e.x0; sx < content_e.x1; sx++)
        {
          px   = ((const uint32_t *) pixels)[sy * 200 + sx];
          blue = px & 0xff;
          if (blue != want)
          {
            bad = 1;
            break;
          }
        }
      }
      if (bad)
        goto Failure; /* a not-yet-painted sliver got blitted as if settled */
    }

    wuss_window_close(win_e);
  }

  printf("test: a long unflushed batch of resize calls (matching a high-poll-rate mouse) never blits a not-yet-painted sliver\n");

  {
    /* The tests above batch only 1-2 resize calls per redraw; a real SDL
     * drag drains its whole event queue before ever calling
     * wuss_redraw_dirty (see apps/wuss/main.c's wuss_frame), so a fast
     * high-poll-rate mouse can deliver dozens of resize calls in one
     * batch. Stress that scale directly, using a cheap deterministic
     * pseudo-random walk rather than a fixed table, so the sequence covers
     * far more shapes (including piece-budget overflow) than a short
     * hand-picked list ever would. */
    static test_task_t tc_f, tc_occ;
    wuss_task_t       *delegate_f, *delegate_occ;
    box_t              box_f, box_occ, content_f;
    wuss_window_t     *win_f, *win_occ;
    colour_t           f_colour;
    point_t            scroll;
    unsigned int       seed;
    int                sy, sx, bad, batch, step, sizew, sizeh;

    f_colour = colour_rgb(0x44, 0x55, 0x66);
    tc_f.redraw_count = 0; tc_f.mouse_count = 0;
    delegate_f = mk_task(wuss, paint_handle, &f_colour);
    if (delegate_f == NULL) goto Failure;

    box_f.x0 = 10; box_f.y0 = 10;
    box_f.x1 = 40; box_f.y1 = 40;
    rc = wuss_window_create(delegate_f, &box_f, "F", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(30, 30), &win_f);
    if (rc != result_OK)
      goto Failure;

    /* An occluder created after (so it lands on top of) win_f, sat over its
     * corner: real desktops are rarely a single window, and
     * wuss__clip_to_visible's occlusion-carving path (untouched by every
     * test above) can only interact with the reclamp blit's stale-region
     * exclusion when there is something to occlude. New windows land at the
     * z_order head (topmost) -- see wuss_window_create. A NULL task_data
     * gives paint_handle's flat-fill branch, so the occluded area's colour
     * can be told apart from win_f's doc-row pattern by eye if this ever
     * fails; the pixel check below instead uses win_occ->visible (its real,
     * furniture-adjusted footprint) rather than the requested box, since
     * wuss__nudge_visible_onscreen/carve can move or grow it. */
    tc_occ.redraw_count = 0; tc_occ.mouse_count = 0;
    delegate_occ = mk_task(wuss, paint_handle, NULL);
    if (delegate_occ == NULL) goto Failure;

    box_occ.x0 = 25; box_occ.y0 = 25;
    box_occ.x1 = 60; box_occ.y1 = 60;
    rc = wuss_window_create(delegate_occ, &box_occ, "OCC", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(20, 20), SIZE2D(20, 20), &win_occ);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_f, POINT(970, 970));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    seed  = 12345u;
    sizew = 30;
    sizeh = 30;
    for (batch = 0; batch < 20; batch++)
    {
      for (step = 0; step < 25; step++)
      {
        seed   = seed * 1103515245u + 12345u;
        sizew += (int) (seed >> 24) % 21 - 10; /* +/-10 px per call */
        seed   = seed * 1103515245u + 12345u;
        sizeh += (int) (seed >> 24) % 21 - 10;
        sizew  = CLAMP(sizew, 30, 150);
        sizeh  = CLAMP(sizeh, 30, 150);

        rc = wuss_window_resize(win_f, SIZE2D(sizew, sizeh));
        if (rc != result_OK)
          goto Failure;
      }

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_content_bounds(win_f, &content_f);
      wuss_window_get_scroll(win_f, &scroll);
      content_f.x1 = MIN(content_f.x1, 200);
      content_f.y1 = MIN(content_f.y1, 200);
      bad = 0;
      for (sy = content_f.y0; sy < content_f.y1 && !bad; sy++)
      {
        uint32_t px, blue, want;

        want = (uint32_t) ((sy - content_f.y0 + scroll.y) & 0xff);
        for (sx = content_f.x0; sx < content_f.x1; sx++)
        {
          /* skip ground legitimately covered by the occluder's real,
           * furniture-adjusted footprint -- it shows win_occ's own colour
           * there, not win_f's doc-row pattern */
          if (sx >= win_occ->visible.x0 && sx < win_occ->visible.x1 &&
              sy >= win_occ->visible.y0 && sy < win_occ->visible.y1)
            continue;

          px   = ((const uint32_t *) pixels)[sy * 200 + sx];
          blue = px & 0xff;
          if (blue != want)
          {
            bad = 1;
            break;
          }
        }
      }
      if (bad)
        goto Failure; /* a not-yet-painted sliver got blitted as if settled */
    }

    wuss_window_close(win_occ);
    wuss_window_close(win_f);
  }

  printf("test: wuss_window_invalidate_visible on a shrunk, scrolled window only dirties its own visible box\n");

  {
    /* Regression: wuss_window_invalidate(window, NULL) built its "whole
     * visible rect" box already in screen space (offset by content.x0/y0),
     * then translated it a second time by "content.x0 - scroll", so a
     * scrolled window's dirty rect ended up shifted by -scroll off to one
     * side of the window rather than sitting over it -- e.g. saturn.c's
     * per-idle-tick wuss_window_invalidate_visible(), once its window was
     * shrunk below its document and then scrolled, queued a dirty rect
     * outside the window's own screen area (fill_backdrop_excluding_content
     * then repaints that stray area as bare desktop every frame). */
    static test_task_t tc_s;
    wuss_task_t       *delegate_s;
    box_t              box_s, dirty;
    wuss_window_t     *win_s;
    colour_t           s_colour;

    s_colour = colour_rgb(0xcc, 0xdd, 0xee);
    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    delegate_s = mk_task(wuss, flood_full_bounds_handle, &s_colour);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 60; box_s.y0 = 60;
    box_s.x1 = 140; box_s.y1 = 140;
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 80), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss); /* flush the create */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_window_resize(win_s, SIZE2D(30, 30));
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_s, POINT(20, 20));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* mirrors saturn_idle: invalidate the whole visible rect with no
     * explicit local_box, exactly what a task's idle handler calls every
     * frame -- inspect the queued dirty rect directly rather than via
     * pixels, since every window (including S itself) re-clips to its own
     * content box on redraw regardless of what the dirty list says, so a
     * stray dirty rect only ever shows up as a wasted backdrop repaint, not
     * as a colour mismatch. */
    wuss_window_invalidate_visible(win_s);
    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;
    wuss_get_dirty(wuss, 0, &dirty);
    if (!box_contains_box(&dirty, &win_s->visible))
      goto Failure; /* dirty rect strayed outside S's own visible box */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_s);
  }

  printf("test: wuss_WINDOW_NO_RESIZE_BLIT redraws the whole window instead of blitting\n");

  {
    static test_task_t tc_nb;
    wuss_task_t       *delegate_nb;
    box_t              box_nb, before, titlebar, toggle;
    wuss_window_t     *win_nb;
    int                outline_px, titlebar_height, inset, icon;
    int                i, cx, cy, interior_x, interior_y, interior_dirty;

    tc_nb.redraw_count = 0;
    tc_nb.mouse_count  = 0;
    delegate_nb = mk_task(wuss, test_handle, &tc_nb);
    if (delegate_nb == NULL) goto Failure;

    box_nb.x0 = 10; box_nb.y0 = 10;
    box_nb.x1 = 50; box_nb.y1 = 50; /* 40x40 content, room to grow to a 200x200 doc */
    rc = wuss_window_create(delegate_nb, &box_nb, "NB",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_NO_BACKDROP,
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_nb);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_nb, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    /* same pre-grow interior point as the blit test above -- there, the
     * blit must preserve it (never dirty); here, the flag must force a full
     * redraw instead of a blit, so this point must come out dirty */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* NB's toggle-size icon: grow */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_nb)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (!interior_dirty)
      goto Failure; /* wuss_WINDOW_NO_RESIZE_BLIT must skip the blit path
                      * entirely, so even an interior pixel the blit would
                      * otherwise have preserved comes out dirty */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_nb);
  }

  printf("test: toggle-size maximize accounts for scrollbar furniture, not just outline/titlebar\n");

  {
    static test_task_t tc_u;
    wuss_task_t       *delegate_u;
    box_t              box_u, ub, titlebar, toggle;
    wuss_window_t     *win_u;
    int                outline_px, titlebar_height, inset, icon;
    int                cx, cy;

    tc_u.redraw_count = 0;
    tc_u.mouse_count  = 0;
    delegate_u = mk_task(wuss, test_handle, &tc_u);
    if (delegate_u == NULL) goto Failure;

    box_u.x0 = 80; box_u.y0 = 80;
    box_u.x1 = 120; box_u.y1 = 120; /* 40x40 content */
    rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_u, "U", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(70, 70), SIZE2D(0, 0), &win_u); /* doc size well within the 200x200 screen: growth is doc-limited, not screen-limited */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_u, &ub);
    titlebar.x0 = ub.x0 + outline_px;
    titlebar.x1 = ub.x1 - outline_px;
    titlebar.y0 = ub.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* U's toggle-size icon: grow to doc size */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_u)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_u, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;
    if (width != 70 || height != 70)
      goto Failure; /* visible must grow by the scrollbar breadth too, on top
                      * of outline/titlebar, or content ends up icon-size
                      * short of doc_width/doc_height */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. Icon moved with the grown titlebar, so recompute. */
    wuss_window_get_visible_bounds(win_u, &ub);
    titlebar.x0 = ub.x0 + outline_px;
    titlebar.x1 = ub.x1 - outline_px;
    titlebar.y0 = ub.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* U's toggle-size icon: shrink back */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_u)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_u, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;
    if (width != 40 || height != 40)
      goto Failure; /* restores exactly the pre-toggle content size */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_u);
  }

  printf("test: toggle-size maximize stays a valid box for a window dragged off the right/bottom edge\n");

  {
    static test_task_t tc_v;
    wuss_task_t       *delegate_v;
    box_t              box_v, vb, titlebar, toggle;
    wuss_window_t     *win_v;
    int                outline_px, titlebar_height, inset, icon;
    int                cx, cy;

    memset(&tc_v, 0, sizeof(tc_v));
    tc_v.veto_pre_close = 1; /* this test pokes furniture with a window in a
                              * deliberately broken (inverted) box; a stray
                              * hit on the close icon must not tear win_v down
                              * mid-test */
    delegate_v = mk_task(wuss, test_handle, &tc_v);
    if (delegate_v == NULL) goto Failure;

    box_v.x0 = 10; box_v.y0 = 10;
    box_v.x1 = 70; box_v.y1 = 70; /* 60x60 content -- wide enough titlebar that
                                    * back/close/toggle icons don't overlap
                                    * (a titlebar much narrower than that makes
                                    * the toggle icon's box overlap close's, so
                                    * a click meant for toggle lands on close
                                    * instead, since hit-test checks close
                                    * first -- a separate, pre-existing issue
                                    * unrelated to toggle-size specifically).
                                    * Doc big enough that maximize is
                                    * screen-limited, not doc-limited. */
    rc = wuss_window_create(delegate_v, &box_v, "V", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_v);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* drag well past the right/bottom screen edge (200x200 screen) -- an
     * ordinary, already-supported position (see the drag-off-screen test
     * above): titlebar drag calls wuss_window_move with no clamping. From
     * here, "available space to the screen edge" (scr_width - visible.x0 -
     * furniture) goes negative, further than furniture alone can absorb. */
    wuss_window_move(win_v, POINT(310, 310));

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_v, &vb);
    titlebar.x0 = vb.x0 + outline_px;
    titlebar.x1 = vb.x1 - outline_px;
    titlebar.y0 = vb.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* V's toggle-size icon: maximize while off-screen */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_v)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_v, &vb);
    if (vb.x1 <= vb.x0 || vb.y1 <= vb.y0)
      goto Failure; /* a naive "space from x0 to the screen edge" sum goes
                      * negative from off-screen, producing an inverted box --
                      * un-hit-testable forever after, since box_contains_point
                      * can never match x0>x1: the window would be stuck,
                      * unclickable, unclosable */
    if (vb.x0 < 0 || vb.y0 < 0 || vb.x1 > 200 || vb.y1 > 200)
      goto Failure; /* toggle repositions toward the origin by the minimum
                      * needed, so even a window dragged right off the screen
                      * comes back fully on-screen when maximized */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. With the window clipped this narrow its toggle
     * icon overlaps close's box (see the box_v comment above), so the click
     * meant for toggle may land on close instead; hit-test checks close
     * first. That routes through wuss_window_try_close, and this task's
     * PRE_CLOSE veto (set at task creation) both keeps the window alive and
     * makes the click return the veto's non-OK -- accept that here. The
     * point of the check is line-below: the box must still be valid. */
    wuss_window_get_visible_bounds(win_v, &vb);
    titlebar.x0 = vb.x0 + outline_px;
    titlebar.x1 = vb.x1 - outline_px;
    titlebar.y0 = vb.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* V's toggle-size icon: shrink back */
    if (rc != result_OK && rc != result_BAD_ARG)
      goto Failure;
    if (hit != win_v)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK && rc != result_BAD_ARG)
      goto Failure;

    wuss_window_get_visible_bounds(win_v, &vb);
    if (vb.x1 <= vb.x0 || vb.y1 <= vb.y0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_v);
  }

  printf("test: a toggle-size that repositions the window blits the overlapping content rather than repainting it all, and restores the exact pre-toggle box\n");

  {
    wuss_task_t   *delegate_p;
    box_t          box_p, before, after, titlebar, toggle;
    wuss_window_t *win_p;
    int            outline_px, titlebar_height, inset, icon, cx, cy;
    int            i, dx, dy, kept_x, kept_y, kept_dirty;
    int            ghost_x, ghost_y, ghost_dirty;

    delegate_p = mk_task(wuss, NULL, NULL);
    if (delegate_p == NULL) goto Failure;

    box_p.x0 = 110; box_p.y0 = 110;
    box_p.x1 = 190; box_p.y1 = 190; /* 80x80 content, well down the bottom-right
                                      * of the 200x200 screen -- big enough to
                                      * have a real interior the blit can keep;
                                      * doc big enough that maximize is
                                      * screen-limited so the window must move */
    rc = wuss_window_create(delegate_p, &box_p, "P", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_p);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_p, &before);
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_p)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_p, &after);
    if (after.x0 >= before.x0 || after.y0 >= before.y0)
      goto Failure; /* the window had to move toward the origin to fit its
                      * maximized box on screen */
    if (after.x0 < 0 || after.y0 < 0 || after.x1 > 200 || after.y1 > 200)
      goto Failure; /* ...but only as far as needed, and it stays on-screen */
    if (after.x1 - after.x0 < 190 || after.y1 - after.y0 < 190)
      goto Failure; /* it genuinely fills (nearly) the whole screen */

    dx = after.x0 - before.x0;
    dy = after.y0 - before.y0;

    /* a point in the old content interior, clear of outline/titlebar/
     * scrollbar furniture on every side: after the toggle's shift by
     * (dx,dy) it lands in the new content interior, and the blit must have
     * carried its pixels there -- so it must NOT be in the dirty list.
     * (This window starts screen-clamped hard against the bottom-right, so
     * the maximized footprint is a superset of the old one -- there is no
     * vacated backdrop strip to check, only the preserved interior.) */
    kept_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2 + dx;
    kept_y = (before.y0 + outline_px + titlebar_height
              + before.y1 - outline_px - icon) / 2 + dy;

    /* where the OLD vertical scrollbar column lands once shifted by (dx,dy):
     * a whole-footprint blit would drag that stale scrollbar strip here,
     * into what is now interior content. The blit must be content-only, so
     * this point must be in the dirty list to be repainted as content. */
    ghost_x = before.x1 - outline_px - icon / 2 + dx;
    ghost_y = (before.y0 + outline_px + titlebar_height
               + before.y1 - outline_px) / 2 + dy;

    kept_dirty  = 0;
    ghost_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, kept_x, kept_y))
        kept_dirty = 1;
      if (box_contains_point(&region, ghost_x, ghost_y))
        ghost_dirty = 1;
    }
    if (kept_dirty)
      goto Failure; /* the shifted interior content was blitted, not repainted */
    if (!ghost_dirty)
      goto Failure; /* the old scrollbar's shifted position must repaint as
                      * content -- the blit must not have carried furniture */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: the icon moved with the grown/repositioned titlebar */
    wuss_window_get_visible_bounds(win_p, &after);
    titlebar.x1 = after.x1 - outline_px;
    titlebar.y0 = after.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_p)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_p, &after);
    if (after.x0 != before.x0 || after.y0 != before.y0 ||
        after.x1 != before.x1 || after.y1 != before.y1)
      goto Failure; /* restore returns the exact pre-toggle box, position too */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_p);
  }

  printf("test: toggle-size fills the screen for a window with no document extent (doc == 0), and per-axis where only one axis has a doc\n");

  {
    wuss_task_t   *delegate_z;
    box_t          box_z, before, after, titlebar, toggle;
    wuss_window_t *win_z;
    int            outline_px, titlebar_height, inset, icon, cx, cy, axis;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    /* axis 0: doc == (0,0) -> both axes fill the screen.
     * axis 1: doc == (60,0) -> width caps at ~60, height fills the screen. */
    for (axis = 0; axis < 2; axis++)
    {
      delegate_z = mk_task(wuss, NULL, NULL);
      if (delegate_z == NULL) goto Failure;

      box_z.x0 = 20; box_z.y0 = 20;
      box_z.x1 = 60; box_z.y1 = 60; /* 40x40 content */
      rc = wuss_window_create(delegate_z, &box_z, "Z", wuss_WINDOW_DEFAULT,
                              wuss_NO_BACKDROP,
                              axis == 0 ? SIZE2D(0, 0) : SIZE2D(60, 0),
                              SIZE2D(0, 0), &win_z);
      if (rc != result_OK)
        goto Failure;

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_visible_bounds(win_z, &before);
      titlebar.x1 = before.x1 - outline_px;
      titlebar.y0 = before.y0 + outline_px;
      toggle.x1 = titlebar.x1 - inset;
      toggle.x0 = toggle.x1 - icon;
      toggle.y0 = titlebar.y0 + inset;
      toggle.y1 = toggle.y0 + icon;
      cx = (toggle.x0 + toggle.x1) / 2;
      cy = (toggle.y0 + toggle.y1) / 2;

      rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
      if (rc != result_OK || hit != win_z)
        goto Failure;
      rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_content_bounds(win_z, &content);
      width  = content.x1 - content.x0;
      height = content.y1 - content.y0;

      if (height < 150)
        goto Failure; /* the y axis has no doc cap in either case -> fills the
                        * screen, not stuck at the 40px starting height */
      if (axis == 0 && width < 150)
        goto Failure; /* doc == 0 -> the x axis fills the screen too */
      if (axis == 1 && width != 60)
        goto Failure; /* doc.w == 60 -> the x axis caps at the doc extent */

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      /* toggle back restores the starting box */
      wuss_window_get_visible_bounds(win_z, &after);
      titlebar.x1 = after.x1 - outline_px;
      titlebar.y0 = after.y0 + outline_px;
      toggle.x1 = titlebar.x1 - inset;
      toggle.x0 = toggle.x1 - icon;
      toggle.y0 = titlebar.y0 + inset;
      toggle.y1 = toggle.y0 + icon;
      cx = (toggle.x0 + toggle.x1) / 2;
      cy = (toggle.y0 + toggle.y1) / 2;

      rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
      if (rc != result_OK || hit != win_z)
        goto Failure;
      rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_visible_bounds(win_z, &after);
      if (after.x0 != before.x0 || after.y0 != before.y0 ||
          after.x1 != before.x1 || after.y1 != before.y1)
        goto Failure;

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      wuss_window_close(win_z);
    }
  }

  printf("test: dragging a back-most window with nothing above it still blits\n");

  {
    static test_task_t tc_k, tc_l;
    wuss_task_t       *delegate_k, *delegate_l;
    box_t              box_k, box_l;
    wuss_window_t     *win_k, *win_l;
    int                before_k, before_l;

    tc_k.redraw_count = 0;
    tc_k.mouse_count  = 0;
    delegate_k = mk_task(wuss, test_handle, &tc_k);
    if (delegate_k == NULL) goto Failure;

    box_k.x0 = 0; box_k.y0 = 140; /* clear of the still-open A/B windows above */
    box_k.x1 = 50; box_k.y1 = 175;
    rc = wuss_window_create(delegate_k,
                            &box_k,
                            "K",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            box_size(&box_k),
                            SIZE2D(0, 0),
                            &win_k);
    if (rc != result_OK)
      goto Failure;

    tc_l.redraw_count = 0;
    tc_l.mouse_count  = 0;
    delegate_l = mk_task(wuss, test_handle, &tc_l);
    if (delegate_l == NULL) goto Failure;

    box_l.x0 = 120; box_l.y0 = 140; /* well clear of K, so never overlaps it */
    box_l.x1 = 170; box_l.y1 = 175;
    rc = wuss_window_create(delegate_l,
                            &box_l,
                            "L",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            box_size(&box_l),
                            SIZE2D(0, 0),
                            &win_l);
    if (rc != result_OK)
      goto Failure;

    wuss_window_restack(win_k, wuss_ZORDER_BACK); /* K is no longer topmost, but L never overlaps it */

    rc = wuss_redraw_dirty(wuss); /* flush the restack's own dirty region first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_k, &visible);

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 31, visible.y0 + 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* K's titlebar, clear of the close icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_k)
      goto Failure;

    before_k = tc_k.redraw_count;
    before_l = tc_l.redraw_count;
    rc = wuss_mouse_move(wuss, POINT(visible.x0 + 45, visible.y0 + 21), &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_k)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_k.redraw_count != before_k || tc_l.redraw_count != before_l)
      goto Failure; /* blitted, not redrawn: nothing above K overlapped its old
                      * footprint, so the move fast path must still apply even
                      * though K isn't topmost */

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 45, visible.y0 + 21), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_k);
    wuss_window_close(win_l);
  }

  printf("test: resizing a window only invalidates the grown/shrunk sliver\n");

  {
    static test_task_t tc_m2;
    wuss_task_t       *delegate_m2;
    box_t              box_m2, before2, after2, region;
    wuss_window_t     *win_m2;
    int                i, dirty_area, full_area, interior_x, interior_y, interior_dirty;

    tc_m2.redraw_count = 0;
    tc_m2.mouse_count  = 0;
    delegate_m2 = mk_task(wuss, test_handle, &tc_m2);
    if (delegate_m2 == NULL) goto Failure;

    box_m2.x0 = 0; box_m2.y0 = 0;
    box_m2.x1 = 40; box_m2.y1 = 40;
    rc = wuss_window_create(delegate_m2,
                            &box_m2,
                            "M2",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL,
                            wuss_NO_BACKDROP,
                            box_size(&box_m2),
                            SIZE2D(0, 0),
                            &win_m2);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the creation invalidation first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_m2, &before2);
    interior_x = before2.x0 + 2;  /* inside the untouched left edge */
    interior_y = before2.y0 + 25; /* below the titlebar, in plain content */

    rc = wuss_window_resize(win_m2, SIZE2D(80, 80)); /* grow */
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (interior_dirty)
      goto Failure; /* untouched top-left corner, unchanged by growing bottom-right */

    wuss_window_get_visible_bounds(win_m2, &after2);
    full_area  = (after2.x1 - after2.x0) * (after2.y1 - after2.y0);
    dirty_area = dirty_union_area(wuss, &after2);
    if (dirty_area < 0 || dirty_area >= full_area)
      goto Failure; /* must be less than a full redraw of the grown footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    before2 = after2;

    rc = wuss_window_resize(win_m2, SIZE2D(40, 40)); /* shrink back */
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (interior_dirty)
      goto Failure; /* still untouched: the corner that remains after shrinking */

    full_area  = (before2.x1 - before2.x0) * (before2.y1 - before2.y0);
    dirty_area = dirty_union_area(wuss, &before2);
    if (dirty_area < 0 || dirty_area >= full_area)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_m2);
  }

  printf("test: resizing a wuss_WINDOW_NO_RESIZE_BLIT window redraws it fully\n");

  {
    static test_task_t tc_nb2;
    wuss_task_t       *delegate_nb2;
    box_t              box_nb2, before3, after3, region;
    wuss_window_t     *win_nb2;
    int                i, dirty_area, full_area;

    tc_nb2.redraw_count = 0;
    tc_nb2.mouse_count  = 0;
    delegate_nb2 = mk_task(wuss, test_handle, &tc_nb2);
    if (delegate_nb2 == NULL) goto Failure;

    box_nb2.x0 = 0; box_nb2.y0 = 0;
    box_nb2.x1 = 40; box_nb2.y1 = 40;
    rc = wuss_window_create(delegate_nb2, &box_nb2, "NB2",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_NO_BACKDROP,
                            box_size(&box_nb2),
                            SIZE2D(0, 0),
                            &win_nb2);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the creation invalidation first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_nb2, &before3);

    rc = wuss_window_resize(win_nb2, SIZE2D(80, 80)); /* grow */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_nb2, &after3);
    full_area  = (after3.x1 - after3.x0) * (after3.y1 - after3.y0);
    dirty_area = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      dirty_area += (region.x1 - region.x0) * (region.y1 - region.y0);
    }
    if (dirty_area < full_area)
      goto Failure; /* NO_RESIZE_BLIT must fully redraw, not just the sliver */

    wuss_window_close(win_nb2);
  }

  printf("test: resizing a wuss_WINDOW_NO_RESIZE_BLIT window drops its cached "
         "furniture layout so the chrome redraws at the new width\n");

  {
    static test_task_t tc_fl;
    wuss_task_t       *delegate_fl;
    box_t              box_fl, after_fl;
    wuss_window_t     *win_fl;
    int                i, old_titlebar_x1, found;

    tc_fl.redraw_count = 0;
    tc_fl.mouse_count  = 0;
    delegate_fl = mk_task(wuss, test_handle, &tc_fl);
    if (delegate_fl == NULL) goto Failure;

    box_fl.x0 = 0; box_fl.y0 = 0;
    box_fl.x1 = 60; box_fl.y1 = 60;
    rc = wuss_window_create(delegate_fl, &box_fl, "FL",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_NO_BACKDROP,
                            box_size(&box_fl),
                            SIZE2D(0, 0),
                            &win_fl);
    if (rc != result_OK)
      goto Failure;

    /* build and cache the furniture layout at the creation width */
    wuss__furniture_layout_build(win_fl);
    if (!(win_fl->furniture_layout.flags & wuss_FURNITURE_LAYOUT__VALID))
      goto Failure;
    old_titlebar_x1 = win_fl->furniture_layout.titlebar.x1;

    rc = wuss_window_resize(win_fl, SIZE2D(140, 60)); /* grow the width */
    if (rc != result_OK)
      goto Failure;

    /* the resize must have invalidated the cache: nothing else here rebuilds
     * it, so a stale valid==1 means furniture would paint at the old width */
    if (win_fl->furniture_layout.flags & wuss_FURNITURE_LAYOUT__VALID)
      goto Failure;

    /* rebuilding now must track the new, wider window */
    wuss__furniture_layout_build(win_fl);
    wuss_window_get_visible_bounds(win_fl, &after_fl);
    if (win_fl->furniture_layout.titlebar.x1 <= old_titlebar_x1)
      goto Failure; /* titlebar still at the old width */

    /* the full-width divider rule must span to the new right edge too */
    found = 0;
    for (i = 0; i < win_fl->furniture_layout.npieces; i++)
    {
      const wuss__furniture_piece_t *p = &win_fl->furniture_layout.pieces[i];

      if (p->paint == wuss__FURNITURE_PAINT_OUTLINE &&
          p->rect.y1 == win_fl->furniture_layout.titlebar.y1 &&
          p->rect.x1 == win_fl->furniture_layout.titlebar.x1)
        found = 1;
    }
    if (!found)
      goto Failure;

    wuss_window_close(win_fl);
  }

  printf("test: dragging a clear window onto an occluder leaves the occluder untouched\n");

  {
    static test_task_t tc_n, tc_o;
    wuss_task_t       *delegate_n, *delegate_o;
    box_t              box_n, box_o, visible_o, exposed, occluded, region;
    wuss_window_t     *win_n, *win_o;
    int                i, exposed_dirty, occluded_dirty;

    tc_n.redraw_count = 0;
    tc_n.mouse_count  = 0;
    delegate_n = mk_task(wuss, test_handle, &tc_n);
    if (delegate_n == NULL) goto Failure;

    box_n.x0 = 0; box_n.y0 = 140; /* clear of any occluder to start */
    box_n.x1 = 60; box_n.y1 = 170;
    rc = wuss_window_create(delegate_n, &box_n, "N",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            box_size(&box_n),
                            SIZE2D(0, 0),
                            &win_n);
    if (rc != result_OK)
      goto Failure;

    tc_o.redraw_count = 0;
    tc_o.mouse_count  = 0;
    delegate_o = mk_task(wuss, test_handle, &tc_o);
    if (delegate_o == NULL) goto Failure;

    box_o.x0 = 90; box_o.y0 = 140; /* N will be dragged partly on top of O */
    box_o.x1 = 130; box_o.y1 = 180;
    rc = wuss_window_create(delegate_o, &box_o, "O",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            box_size(&box_o),
                            SIZE2D(0, 0),
                            &win_o);
    if (rc != result_OK)
      goto Failure;

    /* O is created after N, so O is topmost -- N's destination footprint
     * will overlap an occluder above it in z-order. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_n, &visible);

    /* Move N far enough right that its new footprint lands partly under O
     * (which stays wholly untouched), while its old footprint started
     * entirely clear of O. */
    wuss_window_move(win_n, POINT(visible.x0 + 80, visible.y0 + 10));

    wuss_window_get_visible_bounds(win_n, &visible);
    wuss_window_get_visible_bounds(win_o, &visible_o);
    exposed.x0 = visible.x0;   exposed.y0 = visible.y0;
    exposed.x1 = visible_o.x0; exposed.y1 = visible.y1; /* N's part left of O */
    box_intersection(&visible, &visible_o, &occluded); /* N's part under O */

    /* The part of N's new footprint that lands under O must NOT be queued
     * dirty -- O hasn't moved, so its pixels are already correct there, and
     * the move blit must have skipped blitting into that area rather than
     * pasting N's stale pixels over it and forcing a repair. */
    occluded_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &occluded))
        occluded_dirty = 1;
    }
    if (occluded_dirty)
      goto Failure;

    /* The exposed part of N's new footprint, not under any occluder, must
     * NOT be queued dirty -- it was already moved there correctly by the
     * blit, so redrawing it too would be exactly the "repaint what could
     * have been left in place" waste this fast path exists to avoid. */
    exposed_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &exposed))
        exposed_dirty = 1;
    }
    if (exposed_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_n);
    wuss_window_close(win_o);
  }

  printf("test: moving a partly-occluded window blits its clean part and only repaints the occluded part\n");

  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *delegate_a, *delegate_b;
    box_t              box_a, box_b, visible_a, visible_b_before;
    box_t              clean_new, hidden_new, region;
    wuss_window_t     *win_a, *win_b;
    int                i, dx, clean_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 20; box_b.y0 = 10; /* left half will sit under A */
    box_b.x1 = 80; box_b.y1 = 50;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    box_a.x0 = 0; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 40; box_a.y1 = 100;
    rc = wuss_window_create(delegate_a, &box_a, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_a),
                            SIZE2D(0, 0),
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (x:20-80,y:10-50) is split by A (x:0-40) into a
     * hidden strip (x:20-40, under A) and a clean strip (x:40-80, exposed). */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    /* Move B far enough right that its whole new footprint clears A. */
    dx = 60;
    wuss_window_move(win_b, POINT(visible_b_before.x0 + dx,
                                        visible_b_before.y0));

    /* The clean strip (previously exposed, genuinely B's own pixels) lands
     * at its translated destination and must have been blitted there, not
     * repainted. */
    clean_new.x0 = 40 + dx; clean_new.y0 = 10;
    clean_new.x1 = 80 + dx; clean_new.y1 = 50;

    /* The hidden strip (previously under A, never B's valid rendering) has
     * no valid source pixels, so its translated destination must be a real
     * repaint. */
    hidden_new.x0 = 20 + dx; hidden_new.y0 = 10;
    hidden_new.x1 = 40 + dx; hidden_new.y1 = 50;

    hidden_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &hidden_new))
        hidden_dirty = 1;
    }
    if (!hidden_dirty)
      goto Failure;

    clean_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &clean_new))
        clean_dirty = 1;
    }
    if (clean_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: moving a window whose occluded piece was never blitted doesn't redraw the occluder\n");

  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *delegate_a, *delegate_b;
    box_t              box_a, box_b, visible_a, visible_b_before;
    box_t              region;
    wuss_window_t     *win_a, *win_b;
    int                i, occluder_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 0; box_b.y0 = 0; /* right part sits under A throughout */
    box_b.x1 = 60; box_b.y1 = 40;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    box_a.x0 = 40; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 100; box_a.y1 = 40;
    rc = wuss_window_create(delegate_a, &box_a, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_a),
                            SIZE2D(0, 0),
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    /* Move B straight down: its clean piece (x:0-40) and hidden piece
     * (x:40-60, under A) both stay clear of / under A exactly as before --
     * nothing about A's own pixels is ever touched by the blit, so A must
     * not be forced to redraw. */
    wuss_window_move(win_b, POINT(visible_b_before.x0,
                                        visible_b_before.y0 + 5));

    occluder_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &visible_a))
        occluder_dirty = 1;
    }
    if (occluder_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: moving a window split by a mid-band occluder past the gap between bands blits both bands in a safe order\n");

  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *delegate_a, *delegate_b;
    box_t              box_a, box_b, visible_b_before;
    box_t              occluded_overlap, hidden_new, region;
    wuss_window_t     *win_a, *win_b;
    int                i, occluded_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 0; box_b.y0 = 0; /* middle band sits under A */
    box_b.x1 = 60; box_b.y1 = 60;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    box_a.x0 = 0; box_a.y0 = 20; /* created after B, so A is topmost */
    box_a.x1 = 60; box_a.y1 = 40;
    rc = wuss_window_create(delegate_a, &box_a, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_a),
                            SIZE2D(0, 0),
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (y:0-60) is split by A (y:20-40) into a top clean
     * band (y:0-20), a hidden middle band (y:20-40) and a bottom clean band
     * (y:40-60), each spanning the full width. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);

    /* Move B down by 25px, past the 20px gap between the two clean bands:
     * the top band's destination (y:25-45) lands on the bottom band's
     * still-unread old source (y:40-60). Blitted in the other order --
     * bottom band first (its destination y:65-85 doesn't touch the top
     * band's source), then the top band -- both blits are safe, so this
     * is not a genuine clobber cycle (translating disjoint pieces by the
     * same offset never produces one: any conflict is consistently
     * oriented by the direction of the move). The top band's destination
     * overlap with A (y:25-40) is skipped by the blit entirely (A hasn't
     * moved, its pixels there are already correct), so it must stay clean;
     * the translated hidden band (y:45-65, never had valid pixels) still
     * needs forcing dirty for a real repaint. */
    wuss_window_move(win_b, POINT(visible_b_before.x0,
                                        visible_b_before.y0 + 25));

    occluded_overlap.x0 = 0;  occluded_overlap.y0 = 25;
    occluded_overlap.x1 = 60; occluded_overlap.y1 = 40;
    hidden_new.x0       = 0;  hidden_new.y0       = 45;
    hidden_new.x1       = 60; hidden_new.y1       = 65;

    occluded_dirty = hidden_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &occluded_overlap))
        occluded_dirty = 1;
      if (box_contains_box(&hidden_new, &region))
        hidden_dirty = 1;
    }
    if (occluded_dirty || !hidden_dirty)
      goto Failure;

    /* The blit must have actually happened, not fallen back: the part of
     * B's new footprint that's clear of A and not the hidden band (e.g.
     * the bottom band's new position, y:65-85) must not be dirtied. */
    {
      box_t clean_after, dirty_check;

      clean_after.x0 = 0;  clean_after.y0 = 65;
      clean_after.x1 = 60; clean_after.y1 = 85;
      for (i = 0; i < wuss_get_dirty_count(wuss); i++)
      {
        wuss_get_dirty(wuss, i, &region);
        if (!box_intersection(&region, &clean_after, &dirty_check))
          goto Failure;
      }
    }

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: dragging a window deeper under a corner occluder blits both L-shaped pieces in a safe order\n");

  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *delegate_a, *delegate_b;
    box_t              box_a, box_b, visible_b_before, visible_a, region;
    wuss_window_t     *win_a, *win_b;
    int                i;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 80; box_b.y0 = 80; /* corner already under A */
    box_b.x1 = 140; box_b.y1 = 140;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    box_a.x0 = 0; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 100; box_a.y1 = 100;
    rc = wuss_window_create(delegate_a, &box_a, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_a),
                            SIZE2D(0, 0),
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (80,80)-(140,140) overlaps A (0,0)-(100,100) in its
     * corner (80,80)-(100,100); the rest of B is split into an L-shaped
     * clean region of two pieces, one of whose destination lands on the
     * other's still-unread source -- but blitting the other piece first
     * avoids that entirely, so this must NOT fall back to a full clipped
     * redraw (that was the "Adjust drag behind a corner fully redraws the
     * window" regression). Dragging B up-left by (-15,-15) also grows the
     * overlap with A without ever fully hiding or fully clearing it -- the
     * blit skips the part of each piece's destination that now lands under
     * A, so A's own rendering there is never touched and needs no repair. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    wuss_window_move(win_b, POINT(visible_b_before.x0 - 15,
                                        visible_b_before.y0 - 15));

    /* The blit must have actually happened, not fallen back: B's own
     * footprint (outside A) must not be dirtied wholesale. */
    {
      box_t visible_b_after, whole_footprint, dirty_area_box;
      int   dirty_area, footprint_area;

      wuss_window_get_visible_bounds(win_b, &visible_b_after);
      box_union(&visible_b_before, &visible_b_after, &whole_footprint);

      dirty_area = 0;
      for (i = 0; i < wuss_get_dirty_count(wuss); i++)
      {
        wuss_get_dirty(wuss, i, &region);
        if (!box_intersection(&region, &whole_footprint, &dirty_area_box))
          dirty_area += (dirty_area_box.x1 - dirty_area_box.x0) *
                        (dirty_area_box.y1 - dirty_area_box.y0);
      }
      footprint_area = (whole_footprint.x1 - whole_footprint.x0) *
                       (whole_footprint.y1 - whole_footprint.y0);
      if (dirty_area >= footprint_area)
        goto Failure; /* fell back to a full redraw instead of blitting */
    }

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: destroy mid-drag then move doesn't crash\n");

  tc_c.redraw_count = 0;
  tc_c.mouse_count  = 0;
  delegate_c = mk_task(wuss, test_handle, &tc_c);
  if (delegate_c == NULL) goto Failure;

  box_c.x0 = 0;
  box_c.y0 = 0;
  box_c.x1 = 40;
  box_c.y1 = 40;
  rc = wuss_window_create(delegate_c,
                          &box_c,
                          "C",
                          wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                          ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                          ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                          wuss_NO_BACKDROP,
                          box_size(&box_c),
                          SIZE2D(0, 0),
                          &win_c);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* C's titlebar, above its content, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_c)
    goto Failure;

  wuss_window_close(win_c);

  rc = wuss_mouse_move(wuss, POINT(20, 20), &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: mouse and scroll events arrive in virtual content space, with the scroll offset applied exactly once\n");

  {
    static test_task_t tc_s = { 0 };
    wuss_task_t       *delegate_s;
    box_t              box_s, content_s;
    wuss_window_t     *win_s;
    point_t            scroll;

    delegate_s = mk_task(wuss, test_handle, &tc_s);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 60; box_s.y1 = 60; /* 50x50 content onto a 200x200 doc: room to scroll */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    scroll.x = 30;
    scroll.y = 40;
    wuss_window_set_scroll(win_s, scroll);
    wuss_window_get_scroll(win_s, &scroll); /* read back in case it clamped */

    wuss_window_get_content_bounds(win_s, &content_s);

    rc = wuss_mouse_click(wuss,
                          POINT(content_s.x0 + 5, content_s.y0 + 7),
                          wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_s)
      goto Failure;
    if (tc_s.last_x != 5 + scroll.x || tc_s.last_y != 7 + scroll.y)
      goto Failure; /* a task adding the scroll offset itself would double-count it */

    rc = wuss_mouse_click(wuss,
                          POINT(content_s.x0 + 5, content_s.y0 + 7),
                          wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_move(wuss,
                         POINT(content_s.x0 + 11, content_s.y0 + 13),
                         &hit);
    if (rc != result_OK)
      goto Failure;
    if (tc_s.last_x != 11 + scroll.x || tc_s.last_y != 13 + scroll.y)
      goto Failure;

    tc_s.last_scroll_x = -1;
    tc_s.last_scroll_y = -1;
    rc = wuss_scroll(wuss,
                     POINT(content_s.x0 + 3, content_s.y0 + 4),
                     1, &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_s)
      goto Failure;
    if (tc_s.last_scroll_x != 3 + scroll.x || tc_s.last_scroll_y != 4 + scroll.y)
      goto Failure;

    wuss_window_close(win_s);
  }

  printf("test: content bounds survive furniture, including the interior rules\n");

  {
    /* Furniture -- outline, titlebar, scrollbars and the rules dividing the
     * content from them -- is added outside the requested content box, never
     * carved out of it, so what the caller asks for is what it gets, both at
     * creation and after a resize. */
    static test_task_t tc_r;
    wuss_task_t       *delegate_r;
    box_t              box_r, content_r;
    wuss_window_t     *win_r;

    memset(&tc_r, 0, sizeof(tc_r));
    delegate_r = mk_task(wuss, test_handle, &tc_r);
    if (delegate_r == NULL) goto Failure;

    box_r.x0 = 20;
    box_r.y0 = 30;
    box_r.x1 = 120;
    box_r.y1 = 110;
    rc = wuss_window_create(delegate_r,
                            &box_r,
                            "rules",
                            wuss_WINDOW_DEFAULT, /* all furniture present */
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400),
                            SIZE2D(0, 0),
                            &win_r);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_r, &content_r);
    if (content_r.x1 - content_r.x0 != box_r.x1 - box_r.x0 ||
        content_r.y1 - content_r.y0 != box_r.y1 - box_r.y0)
      goto Failure;

    rc = wuss_window_resize(win_r, SIZE2D(61, 47));
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_r, &content_r);
    if (content_r.x1 - content_r.x0 != 61 || content_r.y1 - content_r.y0 != 47)
      goto Failure;

    wuss_window_close(win_r);
  }

  printf("test: wuss_window_create_placed tiles windows and reclaims a closed slot\n");

  {
    /* Windows created without a position are packed towards the top-left and
     * must not overlap; closing one frees its slot for the next create. */
    static test_task_t tc_p[4];
    wuss_task_t       *delegate_p;
    wuss_window_t     *win_p[4];
    box_t              vis[4], probe;
    int                k, m;

    memset(tc_p, 0, sizeof(tc_p));
    delegate_p = mk_task(wuss, test_handle, &tc_p[0]);
    if (delegate_p == NULL) goto Failure;

    for (k = 0; k < 4; k++)
    {
      rc = wuss_window_create_placed(delegate_p,
                                     SIZE2D(40, 30),
                                     "P",
                                     wuss_WINDOW_DEFAULT,
                                     wuss_NO_BACKDROP,
                                     SIZE2D(40, 30),
                                     SIZE2D(0, 0),
                                     &win_p[k]);
      if (rc != result_OK)
        goto Failure;
      wuss_window_get_visible_bounds(win_p[k], &vis[k]);
    }

    for (k = 0; k < 4; k++)
      for (m = k + 1; m < 4; m++)
        if (box_intersects(&vis[k], &vis[m]))
          goto Failure; /* auto-placed windows overlapped */

    /* free the second window's slot, then a new placed window should land
     * back in it rather than being pushed past the others */
    probe = vis[1];
    wuss_window_close(win_p[1]);

    rc = wuss_window_create_placed(delegate_p,
                                   SIZE2D(40, 30),
                                   "P",
                                   wuss_WINDOW_DEFAULT,
                                   wuss_NO_BACKDROP,
                                   SIZE2D(40, 30),
                                   SIZE2D(0, 0),
                                   &win_p[1]);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_p[1], &vis[1]);
    if (vis[1].x0 != probe.x0 || vis[1].y0 != probe.y0 ||
        vis[1].x1 != probe.x1 || vis[1].y1 != probe.y1)
      goto Failure; /* freed slot not reused */

    /* a manual move releases the slot: closing afterwards must not
     * double-release (would corrupt the packer's free list) */
    wuss_window_move(win_p[0], POINT(200, 200));

    for (k = 0; k < 4; k++)
      wuss_window_close(win_p[k]);
  }

  printf("test: wuss_set_palette and wuss_idle broadcast once per registered task\n");
  {
    /* the wuss under test was made with a 2-entry palette (see top of this
     * function); a fresh 2-entry palette must broadcast to every task, a
     * wrong-length one must be refused without broadcasting */
    static const colour_t swapped[2] = { { 0xFF202020 }, { 0xFFE0E0E0 } };
    static const colour_t too_long[3] =
      { { 0xFF000000 }, { 0xFF808080 }, { 0xFFFFFFFF } };

    static test_task_t tc_pa, tc_pb;
    wuss_task_t       *delegate_pa, *delegate_pb;

    /* every earlier block-local test_task_t is out of scope now; drop the
     * tasks that still point at it so the broadcast counts are exactly the
     * two made here */
    reap_test_tasks();

    memset(&tc_pa, 0, sizeof(tc_pa));
    memset(&tc_pb, 0, sizeof(tc_pb));
    delegate_pa = mk_task(wuss, test_handle, &tc_pa);
    delegate_pb = mk_task(wuss, test_handle, &tc_pb);
    if (delegate_pa == NULL || delegate_pb == NULL)
      goto Failure;

    rc = wuss_set_palette(wuss, swapped, NELEMS(swapped));
    if (rc != result_OK)
      goto Failure;
    if (tc_pa.palette_count != 1 || tc_pb.palette_count != 1)
      goto Failure; /* one PALETTE per registered task */

    rc = wuss_set_palette(wuss, too_long, NELEMS(too_long));
    if (rc != result_BAD_ARG)
      goto Failure;
    if (tc_pa.palette_count != 1 || tc_pb.palette_count != 1)
      goto Failure; /* rejected call must not have broadcast */

    /* wuss_idle broadcasts a single wuss_EVENT_IDLE to each task too; a
     * task with no window still gets it (delivery is per registered task,
     * not per window) */
    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_pa.idle_count != 1 || tc_pb.idle_count != 1)
      goto Failure;

    reap_test_tasks(); /* tc_pa/tc_pb die with this block too */
    rc = result_OK;
  }

  printf("test: wuss_window_set_hidden fires PRE_SHOW; not opting in still shows the window\n");
  {
    static test_task_t tc_ps;
    wuss_task_t       *delegate_ps;
    box_t              box_ps;
    wuss_window_t     *win_ps;

    memset(&tc_ps, 0, sizeof(tc_ps));
    delegate_ps = mk_task(wuss, test_handle, &tc_ps);
    if (delegate_ps == NULL)
      goto Failure;

    box_ps.x0 = 0;  box_ps.y0 = 0;
    box_ps.x1 = 60; box_ps.y1 = 60;
    rc = wuss_window_create(delegate_ps, &box_ps, "PS", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_ps), SIZE2D(0, 0), &win_ps);
    if (rc != result_OK)
      goto Failure;

    /* hide it: no event, and it must not catch the pointer any more */
    rc = wuss_window_set_hidden(win_ps, 1);
    if (rc != result_OK || tc_ps.pre_show_count != 0 || tc_ps.show_count != 0)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                          wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit == win_ps)
      goto Failure; /* hidden window is not hit-tested */
    (void) wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                            wuss_MOUSE_UP, &hit);

    /* handler does not call wuss_window_reveal_now: PRE_SHOW still fires,
     * but the window shows anyway (default proceed) */
    tc_ps.veto_pre_show = 1;
    rc = wuss_window_set_hidden(win_ps, 0);
    if (rc != result_OK)
      goto Failure;
    if (tc_ps.pre_show_count != 1 || tc_ps.show_count != 1)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                          wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_ps)
      goto Failure;
    (void) wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                            wuss_MOUSE_UP, &hit);
    rc = wuss_window_set_hidden(win_ps, 1);
    if (rc != result_OK)
      goto Failure;

    /* handler calls wuss_window_reveal_now explicitly: same result */
    tc_ps.veto_pre_show = 0;
    rc = wuss_window_set_hidden(win_ps, 0);
    if (rc != result_OK)
      goto Failure;
    if (tc_ps.pre_show_count != 2 || tc_ps.show_count != 2)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                          wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_ps)
      goto Failure;
    (void) wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                            wuss_MOUSE_UP, &hit);

    wuss_task_destroy(delegate_ps);
    mk_task_count = 0; /* delegate_ps is gone; drop the stale registry entry */
    rc = result_OK;
  }

  printf("test: a plain window's PRE_SHOW carries a NULL handle -- calling "
         "wuss_menu_open_window_now only when handle is non-NULL does not "
         "crash (regression: saturn.c/tasks.c called it unconditionally)\n");
  {
    static test_task_t tc_psh;
    wuss_task_t       *delegate_psh;
    box_t              box_psh;
    wuss_window_t     *win_psh;

    memset(&tc_psh, 0, sizeof(tc_psh));
    tc_psh.open_window_via_handle = 1;
    delegate_psh = mk_task(wuss, test_handle, &tc_psh);
    if (delegate_psh == NULL)
      goto Failure;

    box_psh.x0 = 0;  box_psh.y0 = 0;
    box_psh.x1 = 60; box_psh.y1 = 60;
    rc = wuss_window_create(delegate_psh, &box_psh, "PSH", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_psh), SIZE2D(0, 0), &win_psh);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_window_set_hidden(win_psh, 1);
    if (rc != result_OK)
      goto Failure;

    /* handle == NULL for a plain window; the handler's NULL guard must
     * take the early return and the window still shows (already
     * defaulted to proceed before the handler ran) */
    rc = wuss_window_set_hidden(win_psh, 0);
    if (rc != result_OK || tc_psh.pre_show_count != 1)
      goto Failure;
    if (win_psh->flags & wuss_WINDOW_HIDDEN)
      goto Failure;

    wuss_task_destroy(delegate_psh);
    mk_task_count = 0;
    rc = result_OK;
  }

  printf("test: wuss_task_destroy sends one wuss_EVENT_QUIT and closes the task's windows\n");
  {
    static test_task_t tc_q;
    wuss_task_t       *delegate_q;
    box_t              box_q;
    wuss_window_t     *win_q1, *win_q2;

    memset(&tc_q, 0, sizeof(tc_q));
    delegate_q = mk_task(wuss, test_handle, &tc_q);
    if (delegate_q == NULL) goto Failure;

    box_q.x0 = 0;  box_q.y0 = 0;
    box_q.x1 = 40; box_q.y1 = 40;
    rc = wuss_window_create(delegate_q, &box_q, "Q1", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_q), SIZE2D(0, 0), &win_q1);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_window_create(delegate_q, &box_q, "Q2", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_q), SIZE2D(0, 0), &win_q2);
    if (rc != result_OK)
      goto Failure;
    NOT_USED(win_q1);
    NOT_USED(win_q2);

    /* one QUIT for the task, regardless of how many windows it owns */
    wuss_task_destroy(delegate_q);
    if (tc_q.stop_count != 1)
      goto Failure;

    mk_task_count = 0; /* delegate_q is gone; drop the stale registry entry */
  }

  printf("test: an autoclose task self-destructs when its last window closes and stops receiving broadcasts\n");
  {
    static test_task_t tc_ac;
    wuss_task_t       *delegate_ac;
    box_t              box_ac;
    wuss_window_t     *win_ac1, *win_ac2;

    memset(&tc_ac, 0, sizeof(tc_ac));
    delegate_ac = mk_task(wuss, test_handle, &tc_ac);
    if (delegate_ac == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_ac, 1);

    box_ac.x0 = 0;  box_ac.y0 = 0;
    box_ac.x1 = 40; box_ac.y1 = 40;
    rc = wuss_window_create(delegate_ac, &box_ac, "AC1", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_ac), SIZE2D(0, 0), &win_ac1);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_window_create(delegate_ac, &box_ac, "AC2", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_ac), SIZE2D(0, 0), &win_ac2);
    if (rc != result_OK)
      goto Failure;

    /* first close: task still owns a window, so no reap and no QUIT */
    wuss_window_close(win_ac1);
    if (tc_ac.stop_count != 0)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_ac.idle_count != 1) /* still registered, still broadcast to */
      goto Failure;

    /* last close: reap fires one QUIT and unregisters the task */
    wuss_window_close(win_ac2);
    if (tc_ac.stop_count != 1)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_ac.idle_count != 1) /* gone: no further broadcast */
      goto Failure;

    mk_task_count = 0; /* delegate_ac reaped itself; drop the stale entry */
  }

  printf("test: a task reaping itself from inside wuss_idle does not derail the task walk\n");
  {
    static test_task_t tc_w1, tc_w2, tc_w3;
    wuss_task_t       *delegate_w1, *delegate_w2, *delegate_w3;
    box_t              box_w;
    wuss_window_t     *win_w1, *win_w3;

    memset(&tc_w1, 0, sizeof(tc_w1));
    memset(&tc_w2, 0, sizeof(tc_w2));
    memset(&tc_w3, 0, sizeof(tc_w3));

    box_w.x0 = 0;  box_w.y0 = 0;
    box_w.x1 = 40; box_w.y1 = 40;

    /* w1 first, w2 (self-reaping, autoclose) in the middle, w3 last: the
     * walk must deliver to w1, survive w2 freeing its own node, and still
     * reach w3. */
    delegate_w1 = mk_task(wuss, test_handle, &tc_w1);
    if (delegate_w1 == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_w1, 1);
    rc = wuss_window_create(delegate_w1, &box_w, "W1", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_w), SIZE2D(0, 0), &win_w1);
    if (rc != result_OK)
      goto Failure;

    delegate_w2 = mk_task(wuss, close_on_idle_handle, &tc_w2);
    if (delegate_w2 == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_w2, 1);
    rc = wuss_window_create(delegate_w2, &box_w, "W2", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_w), SIZE2D(0, 0),
                            &g_close_on_idle_win);
    if (rc != result_OK)
      goto Failure;

    delegate_w3 = mk_task(wuss, test_handle, &tc_w3);
    if (delegate_w3 == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_w3, 1);
    rc = wuss_window_create(delegate_w3, &box_w, "W3", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_w), SIZE2D(0, 0), &win_w3);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_w1.idle_count != 1) /* reached before the reap */
      goto Failure;
    if (tc_w2.stop_count != 1) /* self-reaped mid-walk */
      goto Failure;
    if (tc_w3.idle_count != 1) /* walk still reached the task behind w2 */
      goto Failure;

    /* w2 is gone; a second idle must skip it cleanly */
    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_w2.idle_count != 1 || tc_w3.idle_count != 2)
      goto Failure;

    wuss_window_close(win_w1); /* autoclose: reaps w1 */
    wuss_window_close(win_w3); /* autoclose: reaps w3 */
    mk_task_count = 0; /* all three tasks reaped themselves; drop stale entries */
  }

#ifdef WUSS_MENUS
  printf("test: wuss_menu_create_from_desc parses a descriptor tree\n");
  {
    static const wuss_menu_item_t borrowed_items[] =
    {
      { "Info",  wuss_MENU_ITEM_NONE, NULL },
      { "About", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t borrowed =
    {
      "Help", borrowed_items, NELEMS(borrowed_items)
    };

    wuss_menu_t       *m;
    const wuss_menu_t *sub;

    /* "Root" is the root caption (first token), then items App,
     * File{ %s title discarded, Grid(!), Export(~), |Quit }, Help(> borrowed),
     * with "File" substituted for both %s. '!'/'~' now pull an int vararg
     * (nonzero applies the flag) rather than always applying, so Grid's tick
     * and Export's shade are passed explicitly. '|Quit' sets DASHED on Quit
     * itself, so the submenu holds 3 rows. */
    rc = wuss_menu_create_from_desc(&m,
           "Root, App, %s { %s, !Grid, ~Export, |Quit }, >Help",
           "File", "File", 1, 1, &borrowed);
    if (rc != result_OK)
      goto Failure;

    if (m->nitems != 3)                                   goto MenuFail;
    if (m->title == NULL || strcmp(m->title, "Root") != 0) goto MenuFail;
    if (strcmp(m->items[0].text, "App") != 0)             goto MenuFail;
    if (m->items[0].submenu != NULL)                      goto MenuFail;

    /* items[1] "File" carries the { } submenu; its first token was the title
     * and is not emitted; '|Quit' sets DASHED on the Quit row itself, which
     * keeps its label */
    if (strcmp(m->items[1].text, "File") != 0)            goto MenuFail;
    sub = m->items[1].submenu;
    if (sub == NULL || sub->nitems != 3)                  goto MenuFail;
    if (strcmp(sub->items[0].text, "Grid") != 0)          goto MenuFail;
    if (!(sub->items[0].flags & wuss_MENU_ITEM_TICKED))   goto MenuFail;
    if (!(sub->items[1].flags & wuss_MENU_ITEM_DISABLED)) goto MenuFail;
    if (strcmp(sub->items[2].text, "Quit") != 0)          goto MenuFail;
    if (!(sub->items[2].flags & wuss_MENU_ITEM_DASHED))   goto MenuFail;

    /* items[2] "Help" got a deep copy of the borrowed menu */
    if (strcmp(m->items[2].text, "Help") != 0)            goto MenuFail;
    sub = m->items[2].submenu;
    if (sub == NULL || sub == &borrowed)                  goto MenuFail;
    if (sub->nitems != 2)                                 goto MenuFail;
    if (strcmp(sub->items[1].text, "About") != 0)         goto MenuFail;

    wuss_menu_destroy(m);

    /* same desc, both bools false: '!'/'~' apply nothing */
    rc = wuss_menu_create_from_desc(&m,
           "Root, App, %s { %s, !Grid, ~Export, |Quit }, >Help",
           "File", "File", 0, 0, &borrowed);
    if (rc != result_OK)
      goto Failure;
    sub = m->items[1].submenu;
    if (sub == NULL)                                       goto MenuFail;
    if (sub->items[0].flags & wuss_MENU_ITEM_TICKED)       goto MenuFail;
    if (sub->items[1].flags & wuss_MENU_ITEM_DISABLED)     goto MenuFail;
    wuss_menu_destroy(m);

    /* malformed: unbalanced brace */
    rc = wuss_menu_create_from_desc(&m, "T, A { B, C");
    if (rc != result_BAD_ARG)
      goto MenuFail;

    /* malformed: '!' vararg not 0/1 */
    rc = wuss_menu_create_from_desc(&m, "T, !Grid", 2);
    if (rc != result_BAD_ARG)
      goto MenuFail;

    rc = result_OK;
    goto MenuOK;

MenuFail:
    printf("wuss_test: menu-desc check failed\n");
    return result_TEST_FAILED;
MenuOK: ;
  }

#ifdef WUSS_COMPONENTS
  printf("test: wuss_fontmenu lists the bmfonts dir and resolves a pick\n");
  {
    const char        *dir;
    wuss_fontmenu_t   *fm;
    const wuss_menu_t *fmm;
    wuss_event_t       ev;
    const char        *name;
    int                i;

    dir = pathf("%s/resources/bmfonts", resources);

    rc = wuss_fontmenu_create(&fm, dir, "Font", NULL, NULL);
    if (rc != result_OK)
      goto FontMenuFail;

    fmm = wuss_fontmenu_menu(fm);
    if (fmm == NULL)                                     goto FontMenuFail;
    if (fmm->title == NULL || strcmp(fmm->title, "Font") != 0)
      goto FontMenuFail;
    if (fmm->nitems < 2)                                 goto FontMenuFail;

    /* items sorted ascending, every row a plain leaf */
    for (i = 0; i < fmm->nitems; i++)
    {
      if (fmm->items[i].text == NULL)                    goto FontMenuFail;
      if (fmm->items[i].submenu != NULL)                 goto FontMenuFail;
      if (i > 0 && strcmp(fmm->items[i - 1].text, fmm->items[i].text) >= 0)
        goto FontMenuFail;
    }

    /* a MENU_SELECT for this menu resolves to the row's label */
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = fmm;
    ev.data.menu_select.index  = 1;
    ev.data.menu_select.button = wuss_BUTTON_SELECT;
    name = wuss_fontmenu_selected(fm, &ev);
    if (name == NULL || strcmp(name, fmm->items[1].text) != 0)
      goto FontMenuFail;

    /* wrong event kind, foreign menu and out-of-range index all decline */
    ev.kind = wuss_EVENT_IDLE;
    if (wuss_fontmenu_selected(fm, &ev) != NULL)         goto FontMenuFail;
    ev.kind                   = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu  = NULL;
    if (wuss_fontmenu_selected(fm, &ev) != NULL)         goto FontMenuFail;
    ev.data.menu_select.menu  = fmm;
    ev.data.menu_select.index = fmm->nitems;
    if (wuss_fontmenu_selected(fm, &ev) != NULL)         goto FontMenuFail;

    wuss_fontmenu_destroy(fm);

    /* a SYSTEM-class wuss font is dropped from the menu; NONE-class and
     * unnamed slots are not */
    {
      const char      *sysfontfile;
      bmfont_t        *sysfont;
      wuss_font_desc_t sysdescs[2];
      wuss_t          *syswuss;

      sysfontfile = pathf("%s/resources/bmfonts/Symbols.png", resources);
      rc = bmfont_create(sysfontfile, &sysfont);
      if (rc != result_OK)                               goto FontMenuFail;

      sysdescs[0].font       = sysfont;
      sysdescs[0].font_class = wuss_FONT_CLASS_SYSTEM;
      sysdescs[0].name       = "Symbols";
      sysdescs[1].font       = sysfont;
      sysdescs[1].font_class = wuss_FONT_CLASS_NONE;
      sysdescs[1].name       = "Tiny"; /* NONE class: not skipped though named */

      rc = wuss_create(&scr, sysdescs, 2, NULL, 0, NULL, NULL, NULL, &syswuss);
      if (rc != result_OK)
      {
        bmfont_destroy(sysfont);
        goto FontMenuFail;
      }

      /* dir points into pathf's shared scratch buffer, which sysfontfile's
       * pathf call above just overwrote -- re-derive it rather than reuse
       * the now-stale pointer */
      dir = pathf("%s/resources/bmfonts", resources);

      rc = wuss_fontmenu_create(&fm, dir, "Font", syswuss, NULL);
      wuss_destroy(syswuss);
      bmfont_destroy(sysfont);
      if (rc != result_OK)                               goto FontMenuFail;

      fmm = wuss_fontmenu_menu(fm);
      if (fmm == NULL)                                    goto FontMenuFail;
      for (i = 0; i < fmm->nitems; i++)
        if (strcmp(fmm->items[i].text, "Symbols") == 0)   goto FontMenuFail;
      for (i = 0; i < fmm->nitems; i++)
        if (strcmp(fmm->items[i].text, "Tiny") == 0)
          break;
      if (i == fmm->nitems)                               goto FontMenuFail;

      wuss_fontmenu_destroy(fm);
    }

    /* missing directory is surfaced, not swallowed */
    rc = wuss_fontmenu_create(&fm, "no/such/dir/here", NULL, NULL, NULL);
    if (rc != result_FILE_NOT_FOUND)                     goto FontMenuFail;

    rc = result_OK;
    goto FontMenuOK;

FontMenuFail:
    printf("wuss_test: fontmenu check failed\n");
    return result_TEST_FAILED;
FontMenuOK: ;
  }

  printf("test: wuss_colourmenu covers the palette and resolves a pick\n");
  {
    wuss_colourmenu_t *cm;
    const wuss_menu_t *cmm;
    wuss_event_t       ev;
    wuss_colour_t      picked;
    int                ok;
    int                i;

    rc = wuss_colourmenu_create(&cm, wuss, "Colour", 0);
    if (rc != result_OK)
      goto ColourMenuFail;

    cmm = wuss_colourmenu_menu(cm);
    if (cmm == NULL)                                     goto ColourMenuFail;
    if (cmm->title == NULL || strcmp(cmm->title, "Colour") != 0)
      goto ColourMenuFail;
    if (cmm->nitems < 2)                                 goto ColourMenuFail;

    /* one swatch row per palette index, in order */
    for (i = 0; i < cmm->nitems; i++)
    {
      if (cmm->items[i].text == NULL)                    goto ColourMenuFail;
      if ((cmm->items[i].flags & wuss_MENU_ITEM_SWATCH) == 0)
        goto ColourMenuFail;
      if (cmm->items[i].swatch != (wuss_colour_t) i)     goto ColourMenuFail;
    }

    /* a MENU_SELECT for this menu resolves to the row's palette index */
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = cmm;
    ev.data.menu_select.index  = 1;
    ev.data.menu_select.button = wuss_BUTTON_SELECT;
    ok = -1;
    picked = wuss_colourmenu_selected(cm, &ev, &ok);
    if (!ok || picked != (wuss_colour_t) 1)             goto ColourMenuFail;

    /* wrong event kind, foreign menu and out-of-range index all decline */
    ev.kind = wuss_EVENT_IDLE;
    if (wuss_colourmenu_selected(cm, &ev, &ok) != 0 || ok)
      goto ColourMenuFail;
    ev.kind                   = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu  = NULL;
    if (wuss_colourmenu_selected(cm, &ev, &ok) != 0 || ok)
      goto ColourMenuFail;
    ev.data.menu_select.menu  = cmm;
    ev.data.menu_select.index = cmm->nitems;
    if (wuss_colourmenu_selected(cm, &ev, &ok) != 0 || ok)
      goto ColourMenuFail;

    wuss_colourmenu_destroy(cm);

    if (wuss_colourmenu_create(&cm, NULL, "Colour", 0) != result_NULL_ARG)
      goto ColourMenuFail;

    /* with_none appends a dashed-off "None" row resolving to
     * wuss_NO_BACKGROUND */
    rc = wuss_colourmenu_create(&cm, wuss, "Colour", 1);
    if (rc != result_OK)
      goto ColourMenuFail;

    cmm = wuss_colourmenu_menu(cm);
    if (cmm == NULL)                                     goto ColourMenuFail;
    if (cmm->items[0].swatch != (wuss_colour_t) 0)       goto ColourMenuFail;
    if ((cmm->items[cmm->nitems - 1].flags &
         (wuss_MENU_ITEM_SWATCH | wuss_MENU_ITEM_DASHED)) !=
        (wuss_MENU_ITEM_SWATCH | wuss_MENU_ITEM_DASHED))
      goto ColourMenuFail;
    if (cmm->items[cmm->nitems - 1].swatch != wuss_NO_BACKGROUND)
      goto ColourMenuFail;

    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = cmm;
    ev.data.menu_select.index  = cmm->nitems - 1;
    ev.data.menu_select.button = wuss_BUTTON_SELECT;
    ok = -1;
    picked = wuss_colourmenu_selected(cm, &ev, &ok);
    if (!ok || picked != wuss_NO_BACKGROUND)             goto ColourMenuFail;

    /* actually open and redraw it, so the "None" row's hatched chip gets
     * rasterised for real, not just checked as data -- needs its own
     * font-equipped wuss, unlike the shared fontless "wuss" above */
    {
      const char        *cmfontfile;
      bmfont_t          *cmfont;
      screen_t           cmscr;
      bitmap_t           cmbm;
      void              *cmpixels;
      wuss_t            *cmwuss;
      wuss_font_desc_t   cmfdesc;
      wuss_task_desc_t   cmdesc;
      static test_task_t cmtc;
      wuss_task_t       *cmowner;
      wuss_colourmenu_t *cmreal;
      const wuss_menu_t *cmrealm;
      int                cmrowbytes;

      cmfontfile = pathf("%s/resources/bmfonts/Tiny.png", resources);
      rc = bmfont_create(cmfontfile, &cmfont);
      if (rc != result_OK) goto ColourMenuFail;

      cmrowbytes = 200 * 4;
      cmpixels = malloc((size_t) cmrowbytes * 200);
      if (cmpixels == NULL) { rc = result_OOM; goto ColourMenuFail; }
      rc = bitmap_init(&cmbm, SIZE2D(200, 200), pixelfmt_bgrx8888,
                       cmrowbytes, NULL, cmpixels);
      if (rc != result_OK) { free(cmpixels); goto ColourMenuFail; }
      screen_for_bitmap(&cmscr, &cmbm);

      cmfdesc.font       = cmfont;
      cmfdesc.font_class = wuss_FONT_CLASS_NONE;
      cmfdesc.name       = NULL;
      rc = wuss_create(&cmscr, &cmfdesc, 1, NULL, 0, NULL, NULL, NULL,
                       &cmwuss);
      if (rc != result_OK) { free(cmpixels); goto ColourMenuFail; }

      memset(&cmtc, 0, sizeof(cmtc));
      cmdesc.handle    = test_handle;
      cmdesc.task_data = &cmtc;
      cmdesc.name      = "wuss-test";
      rc = wuss_task_create(cmwuss, &cmdesc, &cmowner);
      if (rc != result_OK) { free(cmpixels); goto ColourMenuFail; }

      rc = wuss_colourmenu_create(&cmreal, cmwuss, "Colour", 1);
      if (rc != result_OK) goto ColourMenuFail;
      cmrealm = wuss_colourmenu_menu(cmreal);

      rc = wuss_menu_open(cmowner, cmrealm, POINT(40, 40), NULL);
      if (rc != result_OK) goto ColourMenuFail;

      rc = wuss_redraw_dirty(cmwuss);
      if (rc != result_OK) goto ColourMenuFail;

      wuss_menu_close(cmwuss->menu_chain);
      wuss_colourmenu_destroy(cmreal);
      wuss_destroy(cmwuss);
      free(cmpixels);
      bmfont_destroy(cmfont);
    }

    wuss_colourmenu_destroy(cm);

    rc = result_OK;
    goto ColourMenuOK;

ColourMenuFail:
    printf("wuss_test: colourmenu check failed\n");
    return result_TEST_FAILED;
ColourMenuOK: ;
  }
#endif /* WUSS_COMPONENTS */

#ifdef WUSS_ICONS
  printf("test: menu pick flashes then delivers MENU_SELECT; fast ADJUST "
         "re-picks do not lose a click or leave a row inverted\n");
  {
    static const wuss_menu_item_t flash_items[] =
    {
      { "One",   wuss_MENU_ITEM_NONE, NULL },
      { "Two",   wuss_MENU_ITEM_NONE, NULL },
      { "Three", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t flash_menu =
    {
      "Pick", flash_items, NELEMS(flash_items)
    };

    const char      *fontfile;
    bmfont_t        *font = NULL;
    wuss_font_desc_t fdesc;
    screen_t         fscr;
    bitmap_t         fbm;
    void            *fpixels;
    wuss_t          *fwuss;
    test_task_t      ftc;
    wuss_task_t     *fowner;
    struct wuss__menu *chain;
    int                i;

    /* a menu needs a font for its row metrics; the core wuss above was made
     * without one */
    fontfile = pathf("%s/resources/bmfonts/Tiny.png", resources);
    rc = bmfont_create(fontfile, &font);
    if (rc != result_OK)
    {
      printf("wuss_test: flash test could not load %s\n", fontfile);
      goto Failure;
    }

    fpixels = malloc((size_t) rowbytes * 200);
    if (fpixels == NULL) { rc = result_OOM; goto FlashFail; }
    rc = bitmap_init(&fbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, fpixels);
    if (rc != result_OK) goto FlashFailFree;
    screen_for_bitmap(&fscr, &fbm);

    fdesc.font       = font;
    fdesc.font_class = wuss_FONT_CLASS_NONE;
    fdesc.name       = NULL;
    rc = wuss_create(&fscr, &fdesc, 1, NULL, 0, NULL, NULL, NULL, &fwuss);
    if (rc != result_OK) goto FlashFailFree;

    memset(&ftc, 0, sizeof(ftc));
    fowner = mk_task(fwuss, test_handle, &ftc);
    if (fowner == NULL) { rc = result_OOM; goto FlashDestroy; }

    /* --- a plain SELECT pick: flash runs, then one MENU_SELECT --- */
    rc = wuss_menu_open(fowner, &flash_menu, POINT(40, 40), NULL);
    if (rc != result_OK) goto FlashDestroy;

    chain = fwuss->menu_chain;
    if (chain == NULL || chain->menu != &flash_menu) goto FlashCheckFail;

    flash_pick_row(fwuss, chain, 1, wuss_BUTTON_SELECT);

    if (ftc.menu_select_count != 0) goto FlashCheckFail; /* deferred, not yet */
    if (chain->flash.frames <= 0)   goto FlashCheckFail; /* flash was armed */

    for (i = 0; i < 64; i++) /* longer than any plausible flash */
      wuss_idle(fwuss);

    if (ftc.menu_select_count != 1)  goto FlashCheckFail;
    if (ftc.last_menu_index != 1)    goto FlashCheckFail;
    if (fwuss->menu_chain != NULL)   goto FlashCheckFail; /* SELECT tore it down */

    /* --- fast ADJUST re-picks: row 0, then row 2 mid-flash --- */
    rc = wuss_menu_open(fowner, &flash_menu, POINT(40, 40), NULL);
    if (rc != result_OK) goto FlashDestroy;
    chain = fwuss->menu_chain;
    ftc.menu_select_count = 0;

    flash_pick_row(fwuss, chain, 0, wuss_BUTTON_ADJUST);
    wuss_idle(fwuss);
    wuss_idle(fwuss); /* a couple of flash frames, nowhere near done */
    flash_pick_row(fwuss, chain, 2, wuss_BUTTON_ADJUST);

    /* the pre-empted row 0 pick must have been delivered, not dropped */
    if (ftc.menu_select_count != 1) goto FlashCheckFail;
    if (ftc.last_menu_index != 0)   goto FlashCheckFail;
    /* and row 0 must not be left highlit */
    if (wuss__icon_hovered(chain->icons[0])) goto FlashCheckFail;

    for (i = 0; i < 64; i++) /* longer than any plausible flash */
      wuss_idle(fwuss);

    if (ftc.menu_select_count != 2) goto FlashCheckFail;
    if (ftc.last_menu_index != 2)   goto FlashCheckFail;
    /* ADJUST keeps the chain open. The flash ends un-highlit, but the pointer
     * is still parked on row 2 (flash_pick_row left it there) so that row --
     * and only that row -- comes back highlit. */
    if (fwuss->menu_chain == NULL)  goto FlashCheckFail;
    for (i = 0; i < flash_menu.nitems; i++)
      if (wuss__icon_hovered(fwuss->menu_chain->icons[i]) != (i == 2))
        goto FlashCheckFail;

    rc = result_OK;
    goto FlashDestroy;

FlashCheckFail:
    printf("wuss_test: menu flash check failed "
           "(select_count=%d last_index=%d chain=%p)\n",
           ftc.menu_select_count, ftc.last_menu_index,
           (void *) fwuss->menu_chain);
    rc = result_TEST_FAILED;

FlashDestroy:
    reap_test_tasks();
    wuss_destroy(fwuss);
FlashFailFree:
    free(fpixels);
FlashFail:
    bmfont_destroy(font);
    if (rc != result_OK)
      return result_TEST_FAILED;
  }

  printf("test: a parent menu row keeps its highlight while its submenu is "
         "open, and loses it once the submenu closes\n");
  {
    static const wuss_menu_item_t sm_sub_items[] =
    {
      { "Sub-A", wuss_MENU_ITEM_NONE, NULL },
      { "Sub-B", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t sm_sub =
    {
      "Sub", sm_sub_items, NELEMS(sm_sub_items)
    };
    static const wuss_menu_item_t sm_items[] =
    {
      { "Plain",  wuss_MENU_ITEM_NONE, NULL },
      { "More",   wuss_MENU_ITEM_NONE, &sm_sub }, /* row 1 owns the submenu */
      { "Bottom", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t sm_menu =
    {
      "Root", sm_items, NELEMS(sm_items)
    };

    const char      *fontfile;
    bmfont_t        *font = NULL;
    wuss_font_desc_t fdesc;
    screen_t         sscr;
    bitmap_t         sbm;
    void            *spixels;
    wuss_t          *swuss;
    test_task_t      stc;
    wuss_task_t     *sowner;
    struct wuss__menu *root;
    struct wuss__menu *sub;

    fontfile = pathf("%s/resources/bmfonts/Tiny.png", resources);
    rc = bmfont_create(fontfile, &font);
    if (rc != result_OK)
    {
      printf("wuss_test: submenu-highlight test could not load %s\n", fontfile);
      goto Failure;
    }

    spixels = malloc((size_t) rowbytes * 200);
    if (spixels == NULL) { rc = result_OOM; goto SubHiFail; }
    rc = bitmap_init(&sbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, spixels);
    if (rc != result_OK) goto SubHiFailFree;
    screen_for_bitmap(&sscr, &sbm);

    fdesc.font       = font;
    fdesc.font_class = wuss_FONT_CLASS_NONE;
    fdesc.name       = NULL;
    rc = wuss_create(&sscr, &fdesc, 1, NULL, 0, NULL, NULL, NULL, &swuss);
    if (rc != result_OK) goto SubHiFailFree;

    memset(&stc, 0, sizeof(stc));
    stc.submenu_to_open = &sm_sub;
    sowner = mk_task(swuss, test_handle, &stc);
    if (sowner == NULL) { rc = result_OOM; goto SubHiDestroy; }

    rc = wuss_menu_open(sowner, &sm_menu, POINT(40, 40), NULL);
    if (rc != result_OK) goto SubHiDestroy;

    root = swuss->menu_chain;
    if (root == NULL || root->menu != &sm_menu) goto SubHiCheckFail;

    /* pointer onto row 1's arrow gutter: its submenu opens */
    menu_move_over_row(swuss, root, 1, 1);
    if (root->child == NULL)             goto SubHiCheckFail;
    if (root->open_index != 1)           goto SubHiCheckFail;
    sub = root->child;
    if (sub->menu != &sm_sub)            goto SubHiCheckFail;
    if (!wuss__icon_hovered(root->icons[1])) goto SubHiCheckFail; /* parent lit */

    /* pointer travels into the submenu, onto its row 0. The parent row that
     * spawned it must keep its highlight; the submenu row gets one too. */
    menu_move_over_row(swuss, sub, 0, 0);
    if (root->child != sub)                  goto SubHiCheckFail; /* still open */
    if (!wuss__icon_hovered(root->icons[1])) goto SubHiCheckFail; /* retained */
    if (!wuss__icon_hovered(sub->icons[0]))  goto SubHiCheckFail;

    /* and while parked over the submenu's own title strip (no row under the
     * pointer, so no ICON event) the parent highlight still holds */
    {
      box_t   cb;
      point_t p;

      wuss_window_get_content_bounds(sub->window, &cb);
      p.x = (cb.x0 + cb.x1) / 2;
      p.y = cb.y0 - 3; /* just above the content: the titlebar */
      wuss_mouse_move(swuss, p, NULL);
      if (!wuss__icon_hovered(root->icons[1])) goto SubHiCheckFail;
    }

    /* pointer re-enters the parent on a different row (row 2, off the arrow):
     * the submenu closes and row 1 must go dark, row 2 lit */
    menu_move_over_row(swuss, root, 2, 0);
    if (root->child != NULL)                 goto SubHiCheckFail; /* closed */
    if (root->open_index != -1)              goto SubHiCheckFail;
    if (wuss__icon_hovered(root->icons[1]))  goto SubHiCheckFail; /* dropped */
    if (!wuss__icon_hovered(root->icons[2])) goto SubHiCheckFail;

    /* re-open, then re-enter the parent on the *same* row's text (off the
     * arrow): submenu closes, and row 1 stays lit because the pointer is on it */
    menu_move_over_row(swuss, root, 1, 1);
    if (root->child == NULL)                 goto SubHiCheckFail;
    menu_move_over_row(swuss, root, 1, 0);
    if (root->child != NULL)                 goto SubHiCheckFail;
    if (!wuss__icon_hovered(root->icons[1])) goto SubHiCheckFail;

    wuss_menu_close(root);
    rc = result_OK;
    goto SubHiDestroy;

SubHiCheckFail:
    printf("wuss_test: submenu-highlight check failed\n");
    rc = result_TEST_FAILED;

SubHiDestroy:
    reap_test_tasks();
    wuss_destroy(swuss);
SubHiFailFree:
    free(spixels);
SubHiFail:
    bmfont_destroy(font);
    if (rc != result_OK)
      return result_TEST_FAILED;
  }

  printf("test: wuss_MENU_ITEM_PRE_OPEN gates wuss_EVENT_PRE_SUBMENU_OPEN -- "
         "unflagged opens the static submenu with no event, flagged requires "
         "opting in\n");
  {
    static const wuss_menu_item_t po_sub_a_items[] =
    {
      { "A-one", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t po_sub_a =
    {
      "A", po_sub_a_items, NELEMS(po_sub_a_items)
    };
    static const wuss_menu_item_t po_sub_b_items[] =
    {
      { "B-one", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t po_sub_b =
    {
      "B", po_sub_b_items, NELEMS(po_sub_b_items)
    };
    static const wuss_menu_item_t po_items[] =
    {
      { "Plain", wuss_MENU_ITEM_NONE, &po_sub_a },       /* unflagged: opens
                                                            * po_sub_a directly,
                                                            * no event */
      { "Flagged", wuss_MENU_ITEM_PRE_OPEN, &po_sub_a }  /* flagged: fires
                                                            * wuss_EVENT_PRE_SUBMENU_OPEN,
                                                            * handler must opt
                                                            * in */
    };
    static const wuss_menu_t po_menu =
    {
      "Root", po_items, NELEMS(po_items)
    };

    const char      *fontfile;
    bmfont_t        *font = NULL;
    wuss_font_desc_t fdesc;
    screen_t         pscr;
    bitmap_t         pbm;
    void            *ppixels;
    wuss_t          *pwuss;
    test_task_t      ptc;
    wuss_task_t     *powner;
    struct wuss__menu *proot;

    fontfile = pathf("%s/resources/bmfonts/Tiny.png", resources);
    rc = bmfont_create(fontfile, &font);
    if (rc != result_OK)
    {
      printf("wuss_test: PRE_SUBMENU_OPEN test could not load %s\n", fontfile);
      goto Failure;
    }

    ppixels = malloc((size_t) rowbytes * 200);
    if (ppixels == NULL) { rc = result_OOM; goto PreOpenFail; }
    rc = bitmap_init(&pbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, ppixels);
    if (rc != result_OK) goto PreOpenFailFree;
    screen_for_bitmap(&pscr, &pbm);

    fdesc.font       = font;
    fdesc.font_class = wuss_FONT_CLASS_NONE;
    fdesc.name       = NULL;
    rc = wuss_create(&pscr, &fdesc, 1, NULL, 0, NULL, NULL, NULL, &pwuss);
    if (rc != result_OK) goto PreOpenFailFree;

    memset(&ptc, 0, sizeof(ptc));
    powner = mk_task(pwuss, test_handle, &ptc);
    if (powner == NULL) { rc = result_OOM; goto PreOpenDestroy; }

    rc = wuss_menu_open(powner, &po_menu, POINT(40, 40), NULL);
    if (rc != result_OK) goto PreOpenDestroy;

    proot = pwuss->menu_chain;
    if (proot == NULL || proot->menu != &po_menu) goto PreOpenCheckFail;

    /* row 0 unflagged: opens po_sub_a directly, no PRE_SUBMENU_OPEN fired */
    menu_move_over_row(pwuss, proot, 0, 1);
    if (ptc.pre_submenu_open_count != 0)     goto PreOpenCheckFail;
    if (proot->child == NULL)                goto PreOpenCheckFail;
    if (proot->child->menu != &po_sub_a)     goto PreOpenCheckFail;

    menu_move_over_row(pwuss, proot, 0, 0); /* off the arrow: closes the child */
    if (proot->child != NULL)                goto PreOpenCheckFail;

    /* row 1 flagged, not opting in: PRE_SUBMENU_OPEN fires but nobody calls
     * wuss_menu_open_submenu_now, so the row stays inert */
    menu_move_over_row(pwuss, proot, 1, 1);
    if (ptc.pre_submenu_open_count != 1)     goto PreOpenCheckFail;
    if (proot->child != NULL)                goto PreOpenCheckFail;

    /* move off and back on, this time opting in with po_sub_b -- a
     * different menu from the row's own static .submenu (po_sub_a) --
     * proves the callback's menu argument, not the static leaf, decides
     * what opens */
    menu_move_over_row(pwuss, proot, 1, 0);
    ptc.submenu_to_open = &po_sub_b;
    menu_move_over_row(pwuss, proot, 1, 1);
    if (ptc.pre_submenu_open_count != 2)     goto PreOpenCheckFail;
    if (proot->child == NULL)                goto PreOpenCheckFail;
    if (proot->child->menu != &po_sub_b)     goto PreOpenCheckFail;

    wuss_menu_close(proot);
    rc = result_OK;
    goto PreOpenDestroy;

PreOpenCheckFail:
    printf("wuss_test: PRE_SUBMENU_OPEN check failed "
           "(count=%d child=%p)\n",
           ptc.pre_submenu_open_count, (void *) proot->child);
    rc = result_TEST_FAILED;

PreOpenDestroy:
    reap_test_tasks();
    wuss_destroy(pwuss);
PreOpenFailFree:
    free(ppixels);
PreOpenFail:
    bmfont_destroy(font);
    if (rc != result_OK)
      return result_TEST_FAILED;
  }

  printf("test: wuss_MENU_ITEM_PRE_OPEN gates wuss_EVENT_PRE_SHOW for a "
         "borrowed-window leaf -- unflagged opens it with no event, flagged "
         "requires opting in\n");
  {
    static wuss_menu_item_t pw_items[2];
    static const wuss_menu_t pw_menu =
    {
      "Root", pw_items, NELEMS(pw_items)
    };

    const char      *fontfile;
    bmfont_t        *font = NULL;
    wuss_font_desc_t fdesc;
    screen_t         pscr;
    bitmap_t         pbm;
    void            *ppixels;
    wuss_t          *pwuss;
    test_task_t      ptc;
    wuss_task_t     *powner;
    box_t            winbox;
    wuss_window_t   *win_plain, *win_flagged;
    struct wuss__menu  *proot;

    fontfile = pathf("%s/resources/bmfonts/Tiny.png", resources);
    rc = bmfont_create(fontfile, &font);
    if (rc != result_OK)
    {
      printf("wuss_test: PRE_SHOW window-leaf test could not load %s\n",
             fontfile);
      goto Failure;
    }

    ppixels = malloc((size_t) rowbytes * 200);
    if (ppixels == NULL) { rc = result_OOM; goto PwFail; }
    rc = bitmap_init(&pbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, ppixels);
    if (rc != result_OK) goto PwFailFree;
    screen_for_bitmap(&pscr, &pbm);

    fdesc.font       = font;
    fdesc.font_class = wuss_FONT_CLASS_NONE;
    fdesc.name       = NULL;
    rc = wuss_create(&pscr, &fdesc, 1, NULL, 0, NULL, NULL, NULL, &pwuss);
    if (rc != result_OK) goto PwFailFree;

    memset(&ptc, 0, sizeof(ptc));
    powner = mk_task(pwuss, test_handle, &ptc);
    if (powner == NULL) { rc = result_OOM; goto PwDestroy; }

    winbox.x0 = 0;  winbox.y0 = 0;
    winbox.x1 = 40; winbox.y1 = 20;
    rc = wuss_window_create(powner, &winbox, "Plain",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_HIDDEN,
                            wuss_NO_BACKDROP, box_size(&winbox),
                            SIZE2D(0, 0), &win_plain);
    if (rc != result_OK) goto PwDestroy;
    rc = wuss_window_create(powner, &winbox, "Flagged",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_HIDDEN,
                            wuss_NO_BACKDROP, box_size(&winbox),
                            SIZE2D(0, 0), &win_flagged);
    if (rc != result_OK) goto PwDestroy;

    pw_items[0].text   = "Plain";
    pw_items[0].flags  = wuss_MENU_ITEM_NONE;
    pw_items[0].submenu = NULL;
    pw_items[0].window  = win_plain;
    pw_items[1].text   = "Flagged";
    pw_items[1].flags  = wuss_MENU_ITEM_PRE_OPEN;
    pw_items[1].submenu = NULL;
    pw_items[1].window  = win_flagged;

    rc = wuss_menu_open(powner, &pw_menu, POINT(40, 40), NULL);
    if (rc != result_OK) goto PwDestroy;

    proot = pwuss->menu_chain;
    if (proot == NULL || proot->menu != &pw_menu) goto PwCheckFail;

    /* row 0 unflagged: still goes through the plain wuss_window_set_hidden
     * path, so PRE_SHOW fires with handle == NULL (default-proceed, no
     * opt-in required) -- unlike PRE_SUBMENU_OPEN, this is the one
     * pre-existing production call site the flag does not gate */
    menu_move_over_row(pwuss, proot, 0, 1);
    if (ptc.pre_show_count != 1)                    goto PwCheckFail;
    if (win_plain->flags & wuss_WINDOW_HIDDEN)      goto PwCheckFail;

    menu_move_over_row(pwuss, proot, 0, 0); /* off the arrow: closes it */
    if (!(win_plain->flags & wuss_WINDOW_HIDDEN))   goto PwCheckFail;

    /* row 1 flagged, handler does not opt in: PRE_SHOW fires (handle
     * non-NULL) but the row stays inert */
    ptc.veto_pre_show = 1;
    menu_move_over_row(pwuss, proot, 1, 1);
    if (ptc.pre_show_count != 2)                    goto PwCheckFail;
    if (!(win_flagged->flags & wuss_WINDOW_HIDDEN)) goto PwCheckFail;

    /* opting in via wuss_menu_open_window_now shows it */
    menu_move_over_row(pwuss, proot, 1, 0);
    ptc.veto_pre_show          = 0;
    ptc.open_window_via_handle = 1;
    menu_move_over_row(pwuss, proot, 1, 1);
    if (ptc.pre_show_count != 3)                    goto PwCheckFail;
    if (win_flagged->flags & wuss_WINDOW_HIDDEN)    goto PwCheckFail;

    wuss_menu_close(proot);
    rc = result_OK;
    goto PwDestroy;

PwCheckFail:
    printf("wuss_test: PRE_SHOW window-leaf check failed "
           "(count=%d plain_hidden=%d flagged_hidden=%d)\n",
           ptc.pre_show_count,
           (win_plain->flags & wuss_WINDOW_HIDDEN) != 0,
           (win_flagged->flags & wuss_WINDOW_HIDDEN) != 0);
    rc = result_TEST_FAILED;

PwDestroy:
    reap_test_tasks();
    wuss_destroy(pwuss);
PwFailFree:
    free(ppixels);
PwFail:
    bmfont_destroy(font);
    if (rc != result_OK)
      return result_TEST_FAILED;
  }

  printf("test: a MENU press over another window closes the open menu and "
         "reaches that window's task so it opens its own menu\n");
  {
    static const wuss_menu_item_t menu_a_items[] =
    {
      { "A-one", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_item_t menu_b_items[] =
    {
      { "B-one", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t menu_a = { "A", menu_a_items, NELEMS(menu_a_items) };
    static const wuss_menu_t menu_b = { "B", menu_b_items, NELEMS(menu_b_items) };

    const char      *fontfile;
    bmfont_t        *font = NULL;
    wuss_font_desc_t fdesc;
    screen_t         mscr;
    bitmap_t         mbm;
    void            *mpixels;
    wuss_t          *mwuss;
    menu_task_t      mta, mtb;
    wuss_task_t     *task_a, *task_b;
    wuss_window_t   *wa, *wb;
    box_t            ba, bb;

    fontfile = pathf("%s/resources/bmfonts/Tiny.png", resources);
    rc = bmfont_create(fontfile, &font);
    if (rc != result_OK)
    {
      printf("wuss_test: menu-move test could not load %s\n", fontfile);
      goto Failure;
    }

    mpixels = malloc((size_t) rowbytes * 200);
    if (mpixels == NULL) { rc = result_OOM; goto MoveFail; }
    rc = bitmap_init(&mbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, mpixels);
    if (rc != result_OK) goto MoveFailFree;
    screen_for_bitmap(&mscr, &mbm);

    fdesc.font       = font;
    fdesc.font_class = wuss_FONT_CLASS_NONE;
    fdesc.name       = NULL;
    rc = wuss_create(&mscr, &fdesc, 1, NULL, 0, NULL, NULL, NULL, &mwuss);
    if (rc != result_OK) goto MoveFailFree;

    memset(&mta, 0, sizeof(mta));
    memset(&mtb, 0, sizeof(mtb));
    mta.menu = &menu_a;
    mtb.menu = &menu_b;

    task_a = mk_task(mwuss, menu_open_handle, &mta);
    task_b = mk_task(mwuss, menu_open_handle, &mtb);
    if (task_a == NULL || task_b == NULL) { rc = result_OOM; goto MoveDestroy; }
    mta.self = task_a;
    mtb.self = task_b;

    /* two well-separated chromeless client windows, so the whole visible box
     * is content and menu A (which opens at the press point inside A) cannot
     * reach into B's box */
    ba.x0 = 6;   ba.y0 = 6;   ba.x1 = 60;  ba.y1 = 60;
    bb.x0 = 130; bb.y0 = 130; bb.x1 = 190; bb.y1 = 190;

    rc = wuss_window_create(task_a, &ba, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&ba), SIZE2D(0, 0), &wa);
    if (rc != result_OK) goto MoveDestroy;

    rc = wuss_window_create(task_b, &bb, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&bb), SIZE2D(0, 0), &wb);
    if (rc != result_OK) goto MoveDestroy;

    /* MENU over A: task A opens menu A */
    wuss_mouse_click(mwuss, POINT(20, 20), wuss_BUTTON_MENU,
                     wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(mwuss, POINT(20, 20), wuss_BUTTON_MENU,
                     wuss_MOUSE_UP, NULL);

    if (mta.menu_press_count != 1)            goto MoveCheckFail;
    if (mwuss->menu_chain == NULL ||
        mwuss->menu_chain->menu != &menu_a)   goto MoveCheckFail;

    /* MENU over B while menu A is still open: the old chain must close AND
     * task B must get the press so menu B opens in its place. Regression:
     * before the mouse-click fix the press was spent closing menu A and B
     * never heard it. */
    wuss_mouse_click(mwuss, POINT(160, 160), wuss_BUTTON_MENU,
                     wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(mwuss, POINT(160, 160), wuss_BUTTON_MENU,
                     wuss_MOUSE_UP, NULL);

    if (mtb.menu_press_count != 1)            goto MoveCheckFail;
    if (mwuss->menu_chain == NULL ||
        mwuss->menu_chain->menu != &menu_b)   goto MoveCheckFail;

    /* replacing menu A's chain told task A its handle was gone */
    if (mta.menu_closed_count != 1)           goto MoveCheckFail;
    if (mta.menu_handle != NULL)              goto MoveCheckFail;

    /* a plain SELECT press on bare backdrop still just dismisses */
    wuss_mouse_click(mwuss, POINT(4, 196), wuss_BUTTON_SELECT,
                     wuss_MOUSE_DOWN, NULL);
    if (mwuss->menu_chain != NULL)            goto MoveCheckFail;

    /* the click-outside dismissal told task B too. Regression: before the
     * MENU_CLOSED fix wuss freed the chain here without telling task B, so
     * mtb.menu_handle dangled and this wuss_menu_close was a use-after-free
     * (ASan: heap-use-after-free in wuss_menu_close). */
    if (mtb.menu_closed_count != 1)           goto MoveCheckFail;
    if (mtb.menu_handle != NULL)              goto MoveCheckFail;
    wuss_menu_close(mtb.menu_handle); /* NULL now: safe no-op */
    if (mwuss->menu_chain != NULL)            goto MoveCheckFail;

    rc = result_OK;
    goto MoveDestroy;

MoveCheckFail:
    printf("wuss_test: menu-move check failed "
           "(a_press=%d b_press=%d chain=%p)\n",
           mta.menu_press_count, mtb.menu_press_count,
           (void *) mwuss->menu_chain);
    rc = result_TEST_FAILED;

MoveDestroy:
    reap_test_tasks();
    wuss_destroy(mwuss);
MoveFailFree:
    free(mpixels);
MoveFail:
    bmfont_destroy(font);
    if (rc != result_OK)
      return result_TEST_FAILED;
  }

  printf("test: a task teardown that leaves a menu chain open is safe -- "
         "wuss_task_destroy abandons it before QUIT, and wuss_destroy frees "
         "the internal menu task after every client QUIT\n");
  {
    static const wuss_menu_item_t q_items[] =
    {
      { "Q-one", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t q_menu = { "Q", q_items, NELEMS(q_items) };

    const char      *fontfile;
    bmfont_t        *font = NULL;
    wuss_font_desc_t fdesc;
    screen_t         qscr;
    bitmap_t         qbm;
    void            *qpixels;
    wuss_t          *qwuss;
    menu_task_t      qmt;
    wuss_task_t     *task_q;
    wuss_window_t   *wq;
    box_t            bq;

    fontfile = pathf("%s/resources/bmfonts/Tiny.png", resources);
    rc = bmfont_create(fontfile, &font);
    if (rc != result_OK)
    {
      printf("wuss_test: menu-quit test could not load %s\n", fontfile);
      goto Failure;
    }

    qwuss   = NULL;
    qpixels = malloc((size_t) rowbytes * 200);
    if (qpixels == NULL) { rc = result_OOM; goto QuitFail; }
    rc = bitmap_init(&qbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, qpixels);
    if (rc != result_OK) goto QuitFreeOnly;
    screen_for_bitmap(&qscr, &qbm);

    fdesc.font       = font;
    fdesc.font_class = wuss_FONT_CLASS_NONE;
    fdesc.name       = NULL;
    rc = wuss_create(&qscr, &fdesc, 1, NULL, 0, NULL, NULL, NULL, &qwuss);
    if (rc != result_OK) goto QuitFreeOnly;

    memset(&qmt, 0, sizeof(qmt));
    qmt.menu                = &q_menu;
    qmt.close_chain_on_quit = 1;

    /* the client task is registered first; the internal menu task is created
     * only on the wuss_menu_open below, so it lands *after* the client in
     * wuss::tasks. wuss_destroy's task sweep therefore reaches the client's
     * QUIT while the menu task node is still, by list order, pending. */
    task_q = mk_task(qwuss, menu_open_handle, &qmt);
    if (task_q == NULL) { rc = result_OOM; goto QuitDestroy; }
    qmt.self = task_q;

    bq.x0 = 6; bq.y0 = 6; bq.x1 = 60; bq.y1 = 60;
    rc = wuss_window_create(task_q, &bq, "Q",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&bq), SIZE2D(0, 0), &wq);
    if (rc != result_OK) goto QuitDestroy;
    NOT_USED(wq);

    wuss_mouse_click(qwuss, POINT(20, 20), wuss_BUTTON_MENU,
                     wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(qwuss, POINT(20, 20), wuss_BUTTON_MENU,
                     wuss_MOUSE_UP, NULL);
    if (qwuss->menu_chain == NULL)           goto QuitCheckFail;
    if (qmt.menu_handle == NULL)             goto QuitCheckFail;

    /* Phase 1: wuss_task_destroy on the chain's owner. It abandons the chain
     * (wuss_EVENT_MENU_CLOSED), which nulls qmt.menu_handle, *before* the
     * QUIT it then delivers -- so the close_chain_on_quit handler's
     * wuss_menu_close is a NULL no-op, not a double-free of the just-freed
     * chain (ASan: heap-use-after-free in wuss_menu_close). */
    reap_test_tasks();
    if (qmt.menu_handle != NULL)             goto QuitCheckFail;
    if (qwuss->menu_chain != NULL)           goto QuitCheckFail;

    /* Phase 2: same again but torn down by wuss_destroy's own task sweep,
     * with task_q left registered. The internal menu task used to be freed by
     * that sweep before task_q's QUIT, so the QUIT handler's wuss_menu_close
     * walked a freed task's window list (ASan: heap-use-after-free in
     * list_remove via wuss_window_close). */
    memset(&qmt, 0, sizeof(qmt));
    qmt.menu                = &q_menu;
    qmt.close_chain_on_quit = 1;
    task_q = mk_task(qwuss, menu_open_handle, &qmt);
    if (task_q == NULL) { rc = result_OOM; goto QuitDestroy; }
    qmt.self = task_q;
    rc = wuss_window_create(task_q, &bq, "Q2",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&bq), SIZE2D(0, 0), &wq);
    if (rc != result_OK) goto QuitDestroy;
    wuss_mouse_click(qwuss, POINT(20, 20), wuss_BUTTON_MENU,
                     wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(qwuss, POINT(20, 20), wuss_BUTTON_MENU,
                     wuss_MOUSE_UP, NULL);
    if (qwuss->menu_chain == NULL)           goto QuitCheckFail;

    forget_test_tasks(); /* drop task_q from the registry; wuss_destroy frees it */
    wuss_destroy(qwuss); /* QUIT -> task_q closes the chain; must not fault */
    qwuss = NULL;

    rc = result_OK;
    goto QuitFreeOnly;

QuitCheckFail:
    printf("wuss_test: menu-quit check failed (chain=%p handle=%p)\n",
           (void *) qwuss->menu_chain, (void *) qmt.menu_handle);
    rc = result_TEST_FAILED;
    goto QuitDestroy;

QuitDestroy:
    reap_test_tasks();
    if (qwuss != NULL)
      wuss_destroy(qwuss);
QuitFreeOnly:
    free(qpixels);
QuitFail:
    bmfont_destroy(font);
    if (rc != result_OK)
      return result_TEST_FAILED;
  }
#endif /* WUSS_ICONS */
#endif /* WUSS_MENUS */

  printf("test: invalidating a box already fully covered by an existing "
        "dirty rect keeps the larger rect\n");

  {
    box_t outer, inner, dirty;

    rc = wuss_redraw_dirty(wuss); /* flush anything still pending first */
    if (rc != result_OK)
      goto Failure;

    outer.x0 = 10; outer.y0 = 10;
    outer.x1 = 90; outer.y1 = 90;
    rc = wuss_invalidate(wuss, &outer);
    if (rc != result_OK)
      goto Failure;

    inner.x0 = 30; inner.y0 = 30;
    inner.x1 = 50; inner.y1 = 50; /* fully inside outer: must not shrink it */
    rc = wuss_invalidate(wuss, &inner);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    wuss_get_dirty(wuss, 0, &dirty);
    if (dirty.x0 != outer.x0 || dirty.y0 != outer.y0 ||
        dirty.x1 != outer.x1 || dirty.y1 != outer.y1)
      goto Failure; /* must still cover the whole outer box, not just inner */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
  }

  printf("test: furniture hit boxes tile the visible box with no leaks\n");

  {
    static test_task_t tc_hs;
    wuss_task_t       *delegate_hs;
    box_t              box_hs, visible, content;
    box_t              back, close, toggle, resize;
    box_t              vup, vdown, vwell, hleft, hright, hwell;
    wuss_window_t     *win_hs;
    int                midx, midy;

    tc_hs.redraw_count = 0;
    tc_hs.mouse_count  = 0;
    delegate_hs = mk_task(wuss, test_handle, &tc_hs);
    if (delegate_hs == NULL) goto Failure;

    /* 120x80 content: wide enough that BACK, CLOSE and TOGGLE_SIZE do not
     * overlap in the titlebar. */
    box_hs.x0 = 5; box_hs.y0 = 5;
    box_hs.x1 = 125; box_hs.y1 = 85;
    rc = wuss_window_create(delegate_hs, &box_hs, "HS", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_hs);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_hs, &visible);
    wuss_window_get_content_bounds(win_hs, &content);
    wuss__back_box(win_hs, &back);
    wuss__close_box(win_hs, &close);
    wuss__toggle_box(win_hs, &toggle);
    wuss__resize_box(win_hs, &resize);
    wuss__vscroll_up_box(win_hs, &vup);
    wuss__vscroll_down_box(win_hs, &vdown);
    wuss__vscroll_well_box(win_hs, &vwell);
    wuss__hscroll_left_box(win_hs, &hleft);
    wuss__hscroll_right_box(win_hs, &hright);
    wuss__hscroll_well_box(win_hs, &hwell);

    midx = (content.x0 + content.x1) / 2;
    midy = (content.y0 + content.y1) / 2;

    /* outline band, each edge */
    if (wuss__furniture_hit_test(win_hs, POINT(midx, visible.y0)) != wuss_FURNITURE_TITLE)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x0, midy)) != wuss_FURNITURE_HSCROLL_WELL)
      goto Failure; /* left outline column, level with the content: nearest chrome is the hscroll strip */
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x1 - 1, midy)) != wuss_FURNITURE_VSCROLL_WELL)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(midx, visible.y1 - 1)) != wuss_FURNITURE_HSCROLL_WELL)
      goto Failure;

    /* the four window corners belong to the adjacent furniture */
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x0, visible.y0)) != wuss_FURNITURE_BACK)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x1 - 1, visible.y0)) != wuss_FURNITURE_TOGGLE_SIZE)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x1 - 1, visible.y1 - 1)) != wuss_FURNITURE_RESIZE)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x0, visible.y1 - 1)) != wuss_FURNITURE_HSCROLL_LEFT)
      goto Failure;

    /* icon centres */
    if (wuss__furniture_hit_test(win_hs, POINT((back.x0 + back.x1) / 2, (back.y0 + back.y1) / 2)) != wuss_FURNITURE_BACK)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((close.x0 + close.x1) / 2, (close.y0 + close.y1) / 2)) != wuss_FURNITURE_CLOSE)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((toggle.x0 + toggle.x1) / 2, (toggle.y0 + toggle.y1) / 2)) != wuss_FURNITURE_TOGGLE_SIZE)
      goto Failure;

    /* scroll-part centres */
    if (wuss__furniture_hit_test(win_hs, POINT((vup.x0 + vup.x1) / 2, (vup.y0 + vup.y1) / 2)) != wuss_FURNITURE_VSCROLL_UP)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((vdown.x0 + vdown.x1) / 2, (vdown.y0 + vdown.y1) / 2)) != wuss_FURNITURE_VSCROLL_DOWN)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((vwell.x0 + vwell.x1) / 2, (vwell.y0 + vwell.y1) / 2)) != wuss_FURNITURE_VSCROLL_WELL)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((hleft.x0 + hleft.x1) / 2, (hleft.y0 + hleft.y1) / 2)) != wuss_FURNITURE_HSCROLL_LEFT)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((hright.x0 + hright.x1) / 2, (hright.y0 + hright.y1) / 2)) != wuss_FURNITURE_HSCROLL_RIGHT)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((hwell.x0 + hwell.x1) / 2, (hwell.y0 + hwell.y1) / 2)) != wuss_FURNITURE_HSCROLL_WELL)
      goto Failure;

    /* divider seam between the content and the scroll strips */
    if (wuss__furniture_hit_test(win_hs, POINT(content.x1, midy)) != wuss_FURNITURE_VSCROLL_WELL)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(midx, content.y1)) != wuss_FURNITURE_HSCROLL_WELL)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(content.x1, content.y1 - 1)) == wuss_FURNITURE_CONTENT)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(content.x1 - 1, content.y1)) == wuss_FURNITURE_CONTENT)
      goto Failure;

    /* genuine content */
    if (wuss__furniture_hit_test(win_hs, POINT(content.x0 + 5, content.y0 + 5)) != wuss_FURNITURE_CONTENT)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(content.x1 - 1, content.y1 - 1)) != wuss_FURNITURE_CONTENT)
      goto Failure;

    if (!furniture_hit_sweep(win_hs, 0))
      goto Failure;

    wuss_window_close(win_hs);
  }

  printf("test: wuss_window_invalidate clamps a client box to the content "
        "area, never dirtying the furniture around it\n");

  {
    static test_task_t tc_iv;
    wuss_task_t       *delegate_iv;
    wuss_window_t     *win_iv;
    box_t              box_iv, content, local, dirty;
    int                i, w, h;

    tc_iv.redraw_count = 0;
    tc_iv.mouse_count  = 0;
    delegate_iv = mk_task(wuss, test_handle, &tc_iv);
    if (delegate_iv == NULL) goto Failure;

    box_iv.x0 = 5; box_iv.y0 = 5;
    box_iv.x1 = 125; box_iv.y1 = 85;
    rc = wuss_window_create(delegate_iv, &box_iv, "IV", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_iv);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_iv, &content);

    rc = wuss_redraw_dirty(wuss); /* flush the create's own dirty region first */
    if (rc != result_OK)
      goto Failure;

    /* a box like a ball's swept range at the bottom-right corner, straddling
     * both the content and the furniture (titlebar/outline/scrollbars)
     * around it -- content-local, so translate content's screen origin
     * back off before building it. */
    w = content.x1 - content.x0;
    h = content.y1 - content.y0;
    local.x0 = w - 10;
    local.y0 = h - 10;
    local.x1 = w + 10; /* runs past content.x1 into the furniture */
    local.y1 = h + 10; /* runs past content.y1 into the furniture */
    wuss_window_invalidate(win_iv, &local);

    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;
    wuss_get_dirty(wuss, 0, &dirty);
    if (dirty.x0 < content.x0 || dirty.y0 < content.y0 ||
        dirty.x1 > content.x1 || dirty.y1 > content.y1)
      goto Failure; /* must be clamped to content, not spill into furniture */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* a box entirely outside the content (pure furniture, e.g. a client bug
     * that never clamped at all) must not dirty anything. */
    before_a = tc_iv.redraw_count;
    local.x0 = -50; local.y0 = -50;
    local.x1 = -10; local.y1 = -10;
    wuss_window_invalidate(win_iv, &local);
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_iv.redraw_count != before_a)
      goto Failure;

    /* sweep every furniture-adjacent edge: a content-local box straddling
     * that edge must clamp to it, never crossing into visible's outline. */
    for (i = 0; i < 4; i++)
    {
      switch (i)
      {
      case 0: /* left edge */
        local.x0 = -10; local.y0 = 0;
        local.x1 = 10;  local.y1 = 10;
        break;
      case 1: /* top edge */
        local.x0 = 0;   local.y0 = -10;
        local.x1 = 10;  local.y1 = 10;
        break;
      case 2: /* right edge */
        local.x0 = w - 10; local.y0 = 0;
        local.x1 = w + 10; local.y1 = 10;
        break;
      default: /* bottom edge */
        local.x0 = 0;       local.y0 = h - 10;
        local.x1 = 10;      local.y1 = h + 10;
        break;
      }

      wuss_window_invalidate(win_iv, &local);
      if (wuss_get_dirty_count(wuss) > 0)
      {
        wuss_get_dirty(wuss, 0, &dirty);
        if (dirty.x0 < content.x0 || dirty.y0 < content.y0 ||
            dirty.x1 > content.x1 || dirty.y1 > content.y1)
          goto Failure;
      }
      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;
    }

    wuss_window_close(win_iv);
  }

  printf("test: furniture hit tiling holds for every furniture-flag combo\n");

  {
    static const wuss_window_flags_t combos[] =
    {
      wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_OUTLINE,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_RESIZE,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_VSCROLL & ~wuss_WINDOW_HSCROLL,
      /* one scrollbar + resize: carve is 0 on the stripless axis, so the
       * icon's near edge is the only thing sizing its hit box */
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_VSCROLL,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_HSCROLL,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_TOGGLE_SIZE,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_CLOSE,
      wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_TITLEBAR
    };

    static test_task_t tc_cb;
    wuss_task_t       *delegate_cb;
    box_t              box_cb, visible;
    wuss_window_t     *win_cb;
    unsigned int       i;

    for (i = 0; i < sizeof combos / sizeof combos[0]; i++)
    {
      int leak;

      tc_cb.redraw_count = 0;
      tc_cb.mouse_count  = 0;
      delegate_cb = mk_task(wuss, test_handle, &tc_cb);
      if (delegate_cb == NULL) goto Failure;

      box_cb.x0 = 5; box_cb.y0 = 5;
      box_cb.x1 = 125; box_cb.y1 = 85;
      rc = wuss_window_create(delegate_cb, &box_cb, "CB", combos[i],
                              wuss_NO_BACKDROP,
                              SIZE2D(400, 400), SIZE2D(0, 0), &win_cb);
      if (rc != result_OK)
        goto Failure;

      /* No titlebar still keeps a 1px top frame with nothing behind it: a
       * click there is the documented CONTENT exception. */
      leak = (combos[i] & wuss_WINDOW_NO_TITLEBAR) &&
             !(combos[i] & wuss_WINDOW_NO_OUTLINE);

      if (!furniture_hit_sweep(win_cb, leak))
      {
        printf("wuss_test: combo index %u (flags 0x%X) failed the hit sweep\n",
               i, (unsigned int) combos[i]);
        goto Failure;
      }

      /* Where the window keeps its resize icon, the icon's own drawn centre
       * must hit RESIZE. The sweep alone misses a collapsed hit box: those
       * pixels just fall to a neighbouring region, still leak-free. */
      if (combos[i] & wuss_WINDOW_RESIZE)
      {
        box_t resize;

        wuss__resize_box(win_cb, &resize);
        if (wuss__furniture_hit_test(win_cb,
              POINT((resize.x0 + resize.x1) / 2,
                    (resize.y0 + resize.y1) / 2)) != wuss_FURNITURE_RESIZE)
        {
          printf("wuss_test: combo index %u (flags 0x%X): resize icon centre "
                 "does not hit RESIZE\n", i, (unsigned int) combos[i]);
          goto Failure;
        }
      }

      if (!(combos[i] & wuss_WINDOW_BACK) &&
          wuss__furniture_hit_test(win_cb, POINT(box_cb.x0, box_cb.y0)) != wuss_FURNITURE_CLOSE)
      {
        wuss_window_get_visible_bounds(win_cb, &visible);
        if (wuss__furniture_hit_test(win_cb, POINT(visible.x0, visible.y0)) != wuss_FURNITURE_CLOSE)
          goto Failure;
      }

      wuss_window_close(win_cb);
    }
  }

  printf("test: a titlebar carries a full-width divider rule at its foot\n");

  {
    static test_task_t tc_dv;
    wuss_task_t       *delegate_dv;
    box_t              box_dv;
    wuss_window_t     *win_dv;
    int                i, found;

    tc_dv.redraw_count = 0;
    tc_dv.mouse_count  = 0;
    delegate_dv = mk_task(wuss, test_handle, &tc_dv);
    if (delegate_dv == NULL) goto Failure;

    box_dv.x0 = 5; box_dv.y0 = 5;
    box_dv.x1 = 125; box_dv.y1 = 85;
    rc = wuss_window_create(delegate_dv, &box_dv, "DV", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_dv);
    if (rc != result_OK)
      goto Failure;

    wuss__furniture_layout_build(win_dv);

    /* an OUTLINE piece exactly one pixel tall, sitting on the titlebar's
     * bottom edge and spanning its full width */
    found = 0;
    for (i = 0; i < win_dv->furniture_layout.npieces; i++)
    {
      const wuss__furniture_piece_t *p = &win_dv->furniture_layout.pieces[i];

      if (p->paint == wuss__FURNITURE_PAINT_OUTLINE &&
          p->rect.y1 == win_dv->furniture_layout.titlebar.y1 &&
          p->rect.y1 - p->rect.y0 == WUSS_DIVIDER_PX &&
          p->rect.x0 == win_dv->furniture_layout.titlebar.x0 &&
          p->rect.x1 == win_dv->furniture_layout.titlebar.x1)
        found = 1;
    }
    if (!found)
      goto Failure;

    wuss_window_close(win_dv);
  }

  printf("test: wuss__slider_value_to_px / wuss__slider_px_to_value convert "
        "and round-trip\n");

  {
    box_t   groove;
    point_t pt;

    groove.x0 = 0; groove.y0 = 0;
    groove.x1 = 100; groove.y1 = 20;

    /* endpoints and midpoint of a [0,100] range over a 100px groove: exact,
     * no rounding needed */
    if (wuss__slider_value_to_px(&groove, wuss_SLIDER_HORIZONTAL,
                                 0, 0, 100) != 0)
      goto Failure;
    if (wuss__slider_value_to_px(&groove, wuss_SLIDER_HORIZONTAL,
                                 100, 0, 100) != 100)
      goto Failure;
    if (wuss__slider_value_to_px(&groove, wuss_SLIDER_HORIZONTAL,
                                 50, 0, 100) != 50)
      goto Failure;

    /* a degenerate [lo,hi] span always reads back as pixel 0 */
    if (wuss__slider_value_to_px(&groove, wuss_SLIDER_HORIZONTAL,
                                 7, 7, 7) != 0)
      goto Failure;

    /* px_to_value rounds to the nearest value rather than truncating: a 10px
     * groove over [0,3] places value 1 at 3.33px, so pixel 3 (0.9 of the way
     * to value 1) must round up to 1, not truncate down to 0 */
    groove.x0 = 0; groove.y0 = 0;
    groove.x1 = 10; groove.y1 = 20;

    pt.x = 3; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 3) != 1)
      goto Failure;

    /* pixel 1 (0.3 of the way to value 1) is closer to 0, and must round
     * down rather than up */
    pt.x = 1; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 3) != 0)
      goto Failure;

    /* an exact half-way pixel rounds up to the higher value: a 4px groove
     * over [0,1] puts the 0/1 boundary at pixel 2 */
    groove.x0 = 0; groove.y0 = 0;
    groove.x1 = 4; groove.y1 = 20;

    pt.x = 2; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 1) != 1)
      goto Failure;

    /* a point left of the groove clamps to lo, one past its right edge
     * clamps to hi */
    pt.x = -50; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 1) != 0)
      goto Failure;
    pt.x = 50; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 1) != 1)
      goto Failure;

    /* VERTICAL: pixel 0 is the groove's bottom (y1), value grows upward */
    groove.x0 = 0; groove.y0 = 0;
    groove.x1 = 20; groove.y1 = 10;

    pt.x = 10; pt.y = groove.y1;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_VERTICAL,
                                 pt, 0, 100) != 0)
      goto Failure;
    pt.x = 10; pt.y = groove.y0;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_VERTICAL,
                                 pt, 0, 100) != 100)
      goto Failure;
  }

  printf("test: wuss__slider_value_for_point maps a screen click through a "
        "real slider icon, honouring the groove gap and a reversed "
        "min > max\n");

  {
    static test_task_t tc_sl;
    wuss_task_t       *delegate_sl;
    wuss_window_t     *win_sl;
    wuss_icon_t       *icon_sl;
    wuss_icon_spec_t   spec_sl;
    box_t              box_sl, content_sl, screen_box_sl, groove_sl;
    point_t            scroll_sl, pt_sl;

    delegate_sl = mk_task(wuss, test_handle, &tc_sl);
    if (delegate_sl == NULL) goto Failure;

    box_sl.x0 = 5; box_sl.y0 = 5; box_sl.x1 = 125; box_sl.y1 = 105;
    rc = wuss_window_create(delegate_sl, &box_sl, "SL", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_sl);
    if (rc != result_OK)
      goto Failure;

    memset(&spec_sl, 0, sizeof(spec_sl));
    spec_sl.bbox                  = (box_t) BOX_POS_SIZE(0, 0, 108, 20);
    spec_sl.type                  = wuss_ICON_TYPE_SLIDER;
    spec_sl.u.slider.orientation  = wuss_SLIDER_HORIZONTAL;
    spec_sl.u.slider.min          = 0;
    spec_sl.u.slider.max          = 100;
    spec_sl.u.slider.default_value = 0;
    rc = wuss_icon_create(win_sl, &spec_sl, &icon_sl);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_sl, &content_sl);
    wuss_window_get_scroll(win_sl, &scroll_sl);
    wuss__icon_box_to_screen(&content_sl, scroll_sl, &spec_sl.bbox,
                             &screen_box_sl);
    wuss__slider_groove_box(&screen_box_sl, &groove_sl);

    /* a click at the groove's on-screen left/right edges reads back as the
     * slider's min/max, confirming wuss__slider_value_for_point applies the
     * WUSS_SLIDER_GAP inset rather than reading off the full icon bbox */
    pt_sl.x = groove_sl.x0; pt_sl.y = (groove_sl.y0 + groove_sl.y1) / 2;
    if (wuss__slider_value_for_point(win_sl, icon_sl, pt_sl) != 0)
      goto Failure;

    pt_sl.x = groove_sl.x1; pt_sl.y = (groove_sl.y0 + groove_sl.y1) / 2;
    if (wuss__slider_value_for_point(win_sl, icon_sl, pt_sl) != 100)
      goto Failure;

    /* a reversed min > max mirrors the reading: the groove's left pixel
     * (nominally "pixel 0") now yields the high end of [lo,hi] */
    wuss_icon_set_value(win_sl, icon_sl, 100);
    icon_sl->spec.u.slider.min = 100;
    icon_sl->spec.u.slider.max = 0;

    pt_sl.x = groove_sl.x0; pt_sl.y = (groove_sl.y0 + groove_sl.y1) / 2;
    if (wuss__slider_value_for_point(win_sl, icon_sl, pt_sl) != 100)
      goto Failure;

    pt_sl.x = groove_sl.x1; pt_sl.y = (groove_sl.y0 + groove_sl.y1) / 2;
    if (wuss__slider_value_for_point(win_sl, icon_sl, pt_sl) != 0)
      goto Failure;

    wuss_window_close(win_sl);
  }

  printf("test: wuss_icons_load scans resources/wuss/icons and compresses\n");

  {
    char            icons_dir[256];
    const bitmap_t *icon_bm;
    int             idx, opton;

    /* copy: pathf hands back one shared static buffer, and wuss_icons_load's
     * own per-file joins would clobber it mid-call */
    strncpy(icons_dir,
            pathf("%s/resources/wuss/icons", resources),
            sizeof(icons_dir) - 1);
    icons_dir[sizeof(icons_dir) - 1] = '\0';

    rc = wuss_icons_load(wuss, icons_dir);
    if (rc != result_OK)
      goto Failure;

    /* the four fixtures: optoff/opton/radoff/radon */
    if (wuss_icons_count(wuss) != 4)
      goto Failure;

    opton = wuss_icons_lookup(wuss, "opton");
    if (opton < 0 || wuss_icons_lookup(wuss, "radoff") < 0)
      goto Failure;
    if (wuss_icons_lookup(wuss, "nonesuch") != -1)
      goto Failure;

    icon_bm = wuss_icons_bitmap(wuss, opton);
    if (icon_bm == NULL || !bitmap_is_compressed(icon_bm))
      goto Failure;
    if (wuss_icons_bitmap(wuss, 4) != NULL)
      goto Failure;

    /* an icon spec picks it up by index via wuss_ICON_SET */
    {
      static test_task_t tc_ic;
      wuss_task_t       *delegate_ic;
      wuss_window_t     *win_ic;
      wuss_icon_t       *icon;
      wuss_icon_spec_t   spec;
      box_t              box_ic;

      memset(&tc_ic, 0, sizeof(tc_ic));
      delegate_ic = mk_task(wuss, test_handle, &tc_ic);
      if (delegate_ic == NULL) goto Failure;

      box_ic.x0 = 5; box_ic.y0 = 5; box_ic.x1 = 125; box_ic.y1 = 105;
      rc = wuss_window_create(delegate_ic, &box_ic, "IC", wuss_WINDOW_DEFAULT,
                              wuss_NO_BACKDROP,
                              SIZE2D(400, 400), SIZE2D(0, 0), &win_ic);
      if (rc != result_OK)
        goto Failure;

      memset(&spec, 0, sizeof(spec));
      spec.bbox     = (box_t) BOX_POS_SIZE(0, 0, 16, 16);
      spec.type     = wuss_ICON_TYPE_BITMAP;
      spec.u.bitmap.set = wuss_ICON_SET(opton);
      rc = wuss_icon_create(win_ic, &spec, &icon);
      if (rc != result_OK)
        goto Failure;

      /* a bogus index is rejected */
      spec.u.bitmap.set = wuss_ICON_SET(99);
      if (wuss_icon_create(win_ic, &spec, &icon) != result_WUSS_BAD_INDEX)
        goto Failure;
      rc = result_OK;

      wuss_window_close(win_ic);
    }

    /* a reload replaces the set cleanly (no leak, ASan would catch it) */
    rc = wuss_icons_load(wuss, icons_dir);
    if (rc != result_OK || wuss_icons_count(wuss) != 4)
      goto Failure;

    idx = wuss_icons_lookup(wuss, "optoff");
    if (idx < 0)
      goto Failure;
  }

  wuss_destroy(wuss);

  free(pixels);

  return result_TEST_PASSED;


Failure:

  printf("wuss_test: failed (rc=0x%X)\n", rc);

  return result_TEST_FAILED;
}

#else /* !(WUSS_FURNITURE && WUSS_ICONS) */

/* Compact core test for builds with furniture and/or icons compiled out.
 * Exercises the chromeless window path: create, move, z-order, invalidate,
 * redraw and programmatic scroll. No titlebar, no scrollbars, no icons. */
result_t wuss_test(const char *resources)
{
  result_t       rc;
  int            rowbytes;
  void          *pixels;
  bitmap_t       bm;
  screen_t       scr;
  wuss_t        *wuss;
  test_task_t    tc_a, tc_b;
  wuss_task_t   *delegate_a, *delegate_b;
  box_t          box_a, box_b, content;
  wuss_window_t *win_a, *win_b, *hit;
  point_t        scroll;

  /* Force a chromeless window even in a build that still has furniture, so the
   * assertions below (content box == visible box) hold in every config. */
  const wuss_window_flags_t chromeless = wuss_WINDOW_NO_TITLEBAR |
                                         wuss_WINDOW_NO_OUTLINE;

  NOT_USED(resources);

  rowbytes = 200 * 4;
  pixels = malloc((size_t) rowbytes * 200);
  if (pixels == NULL)
    goto Failure;

  rc = bitmap_init(&bm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes, NULL,
                   pixels);
  if (rc != result_OK)
    goto Failure;

  screen_for_bitmap(&scr, &bm);

  printf("test: wuss_create (core)\n");

  rc = wuss_create(&scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create too small\n");

  box_a.x0 = 0; box_a.y0 = 0; box_a.x1 = 100; box_a.y1 = 0;
  rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_a, "toosmall", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP,
                          box_size(&box_a),
                          SIZE2D(0, 0), &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  memset(&tc_a, 0, sizeof(tc_a));
  memset(&tc_b, 0, sizeof(tc_b));
  delegate_a = mk_task(wuss, test_handle, &tc_a);
  if (delegate_a == NULL) goto Failure;
  delegate_b = mk_task(wuss, test_handle, &tc_b);
  if (delegate_b == NULL) goto Failure;

  box_a.x0 = 0; box_a.y0 = 0; box_a.x1 = 100; box_a.y1 = 100;
  rc = wuss_window_create(delegate_a, &box_a, "A", chromeless,
                          wuss_NO_BACKDROP,
                          SIZE2D(400, 400),
                          SIZE2D(0, 0), &win_a);
  if (rc != result_OK)
    goto Failure;

  box_b.x0 = 50; box_b.y0 = 50; box_b.x1 = 150; box_b.y1 = 150;
  rc = wuss_window_create(delegate_b, &box_b, "B", chromeless,
                          wuss_NO_BACKDROP,
                          SIZE2D(400, 400),
                          SIZE2D(0, 0), &win_b);
  if (rc != result_OK)
    goto Failure;

  /* With furniture off the content box is the visible box verbatim. */
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x0 != 0 || content.y0 != 0 ||
      content.x1 != 100 || content.y1 != 100)
    goto Failure;

  printf("test: window_resize honours the requested size even past the "
         "screen edge\n");

  /* screen is 200x200; win_a sits at (0,0), chromeless. asking for a
   * 500x500 content area is honoured verbatim. */
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 500 || content.y1 - content.y0 != 500)
    goto Failure;

  /* a smaller request is likewise honoured verbatim */
  rc = wuss_window_resize(win_a, SIZE2D(120, 90));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 120 || content.y1 - content.y0 != 90)
    goto Failure;

  /* an offset top-left does not clip the size either */
  wuss_window_move(win_a, POINT(60, 40));
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 500 || content.y1 - content.y0 != 500)
    goto Failure;
  wuss_window_move(win_a, POINT(0, 0));
  rc = wuss_window_resize(win_a, SIZE2D(100, 100));
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create can never make a window bigger than the "
         "screen\n");

  {
    box_t          box_big;
    wuss_window_t *win_big;

    /* chromeless and the on-screen nudge drags the top-left back to (0,0),
     * so the clamp caps content at the full 200x200 screen */
    box_big.x0 = 10; box_big.y0 = 10; box_big.x1 = 400; box_big.y1 = 400;
    rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_big, "BIG", chromeless,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400),
                            SIZE2D(0, 0), &win_big);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_content_bounds(win_big, &content);
    if (content.x1 - content.x0 != 200 || content.y1 - content.y0 != 200)
      goto Failure;
    wuss_window_close(win_big);
  }

  printf("test: redraw delivers REDRAW events\n");

  wuss_redraw(wuss);
  if (tc_a.redraw_count == 0 || tc_b.redraw_count == 0)
    goto Failure;

  printf("test: z-order - click routes to topmost window\n");

  /* B was created last so it is on top over the overlap at (75,75). */
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;

  wuss_window_restack(win_a, wuss_ZORDER_FRONT);
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1)
    goto Failure;

  printf("test: mouse point is in document coordinates\n");

  if (tc_a.last_x != 75 || tc_a.last_y != 75)
    goto Failure;

  printf("test: window_move\n");

  wuss_window_move(win_a, POINT(20, 20));
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x0 != 20 || content.y0 != 20)
    goto Failure;

  printf("test: programmatic scroll offsets document coordinates\n");

  wuss_window_set_scroll(win_a, POINT(10, 5));
  wuss_window_get_scroll(win_a, &scroll);
  if (scroll.x != 10 || scroll.y != 5)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(20, 20), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != win_a)
    goto Failure;
  if (tc_a.last_x != 10 || tc_a.last_y != 5)
    goto Failure;

  printf("test: wheel scroll routes to the window but a NO_VSCROLL window "
         "does not move\n");

  /* chromeless carries NO_VSCROLL, so wuss__scroll_step suppresses the wheel
   * step: the window is still the hit target but its offset is unchanged. */
  wuss_scroll(wuss, POINT(20, 20), 8, &hit);
  if (hit != win_a)
    goto Failure;
  wuss_window_get_scroll(win_a, &scroll);
  if (scroll.y != 5)
    goto Failure;

  printf("test: invalidate marks dirty region\n");

  wuss_window_invalidate(win_a, NULL);
  if (wuss_get_dirty_count(wuss) == 0)
    goto Failure;
  wuss_redraw(wuss);
  if (wuss_get_dirty_count(wuss) != 0)
    goto Failure;

  printf("test: window_close delivers CLOSE and drops the window\n");

  wuss_window_close(win_b);
  rc = wuss_mouse_click(wuss, POINT(140, 140), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != NULL)
    goto Failure;

  wuss_destroy(wuss);
  free(pixels);

  return result_TEST_PASSED;

Failure:

  printf("wuss_test: failed\n");

  return result_TEST_FAILED;
}

#endif /* WUSS_FURNITURE && WUSS_ICONS */
