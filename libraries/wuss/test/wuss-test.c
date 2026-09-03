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
#endif
#ifdef WUSS_COMPONENTS
#include "wuss/component/fontmenu.h"
#include "wuss/component/colourmenu.h"
#endif

/* white-box: the menu-flash test drives picks through the icon layer and
 * reads back struct wuss__menu / struct wuss_icon state directly */
#if defined(WUSS_MENUS) && defined(WUSS_ICONS)
#include "../impl.h"
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
  int                 menu_select_count;
  int                 last_menu_index;
}
test_task_t;

static result_t test_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  test_task_t *tc;

  NOT_USED(window);

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
      return result_BAD_ARG; /* any non-OK return vetoes the reveal */
    break;

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
#endif

/* ----------------------------------------------------------------------- */

/* Total area covered by the dirty list, counting overlapped pixels once.
 * Summing each region's area instead would double-count wherever two
 * invalidations overlap, which they legitimately do. */
static int dirty_union_area(wuss_t *wuss, const box_t *bounds)
{
  static unsigned char covered[512 * 512];

  box_t region;
  int   w, h, i, x, y, area;

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
  bad_config.furniture.back            = 0;
  bad_config.furniture.close           = 0;
  bad_config.furniture.toggle          = 0;
  bad_config.furniture.resize          = 0;
  bad_config.furniture.scroll.arrows   = 0;
  bad_config.furniture.scroll.wells    = 0;
  bad_config.furniture.scroll.sausages = 0;
  rc = wuss_create(&scr, NULL, NULL, 0, &bad_config, NULL, &bad_wuss);
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  printf("test: wuss_create with custom palette, no config\n");

  {
    wuss_t *custom_wuss;

    rc = wuss_create(&scr, NULL, custom_palette, 2, NULL, NULL, &custom_wuss);
    if (rc != result_OK)
      goto Failure;
    wuss_destroy(custom_wuss);
  }

  printf("test: wuss_create with default palette\n");

  rc = wuss_create(&scr, NULL, NULL, 0, NULL, NULL, &wuss);
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
    wuss_config_t   symcfg;
    wuss_t         *symw;
    wuss_window_t  *symwin;
    wuss_task_t    *symdel;
    box_t           symbox;

    memset(&symcfg, 0, sizeof(symcfg));
    symcfg.furniture.title.bg = wuss_COLOUR_BLUE;   /* -> index 2 */
    symcfg.furniture.title.fg = wuss_COLOUR_WHITE;  /* -> index 3 */
    symcfg.backdrop = (wuss_backdrop_t) wuss_BACKDROP_COLOUR(wuss_COLOUR_GREEN); /* -> 1 */

    rc = wuss_create(&scr, NULL, sympal, 5, &symcfg, NULL, &symw);
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
        wuss__resolve_colour(symw, wuss_COLOUR_BACKDROP) != 1)
    {
      wuss_destroy(symw);
      goto Failure;
    }

    /* a symbolic colour is accepted (and stored resolved) where a client
     * passes a wuss_colour_t. */
    symdel = mk_task(symw, NULL, NULL);
    if (symdel == NULL) { wuss_destroy(symw); goto Failure; }
    symbox.x0 = 0; symbox.y0 = 0; symbox.x1 = 80; symbox.y1 = 80;
    rc = wuss_window_create(symdel, &symbox, "sym", wuss_WINDOW_NONE,
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
                          wuss_WINDOW_NONE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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

  printf("test: window_resize can never grow a window past the screen\n");

  /* screen is 200x200; win_a's visible box sits at (0,25) with a 1px
   * outline all round and a 20px titlebar. asking for a 500x500 content
   * area must clamp so the visible box still fits: width 200-0-2,
   * height 200-25-2-20. */
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 198 || content.y1 - content.y0 != 153)
    goto Failure;
  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x1 != 200 || visible.y1 != 200)
    goto Failure;

  /* a request that already fits is honoured verbatim */
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
    rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_big, "BIG", wuss_WINDOW_NO_VSCROLL |
                            wuss_WINDOW_NO_HSCROLL | wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                          wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_e, tc_f;
    wuss_task_t   *delegate_e, *delegate_f;
    box_t          box_e, box_f;
    wuss_window_t *win_e, *win_f;

    tc_e.redraw_count = 0;
    tc_e.mouse_count  = 0;
    delegate_e = mk_task(wuss, test_handle, &tc_e);
    if (delegate_e == NULL) goto Failure;

    box_e.x0 = 100; box_e.y0 = 0;
    box_e.x1 = 150; box_e.y1 = 50;
    rc = wuss_window_create(delegate_e,
                            &box_e,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_TITLEBAR |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_h, tc_g;
    wuss_task_t   *delegate_h, *delegate_g;
    box_t          box_h, box_g;
    wuss_window_t *win_h, *win_g;
    int            before_h, before_g;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h = mk_task(wuss, test_handle, &tc_h);
    if (delegate_h == NULL) goto Failure;

    box_h.x0 = 10; box_h.y0 = 10;
    box_h.x1 = 30; box_h.y1 = 30;
    rc = wuss_window_create(delegate_h,
                            &box_h,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_i, tc_j;
    wuss_task_t   *delegate_i, *delegate_j;
    box_t          box_i, box_j, dirty;
    wuss_window_t *win_i, *win_j;

    tc_i.redraw_count = 0;
    tc_i.mouse_count  = 0;
    delegate_i = mk_task(wuss, test_handle, &tc_i);
    if (delegate_i == NULL) goto Failure;

    box_i.x0 = 0; box_i.y0 = 0;
    box_i.x1 = 100; box_i.y1 = 100;
    rc = wuss_window_create(delegate_i,
                            &box_i,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_m;
    wuss_task_t   *delegate_m;
    box_t          box_m;
    wuss_window_t *win_m;
    int            before_m;

    tc_m.redraw_count = 0;
    tc_m.mouse_count  = 0;
    delegate_m = mk_task(wuss, test_handle, &tc_m);
    if (delegate_m == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 60; box_m.y1 = 60; /* 50x50, fully on-screen, topmost (created last) */
    rc = wuss_window_create(delegate_m,
                            &box_m,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_g, tc_h;
    wuss_task_t   *delegate_g, *delegate_h;
    box_t          box_g, box_h;
    wuss_window_t *win_g, *win_h;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h = mk_task(wuss, test_handle, &tc_h);
    if (delegate_h == NULL) goto Failure;

    box_h.x0 = 130; box_h.y0 = 50;
    box_h.x1 = 190; box_h.y1 = 100;
    rc = wuss_window_create(delegate_h,
                            &box_h,
                            "H",
                            wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_m;
    wuss_task_t   *delegate_m;
    box_t          box_m, content, visible;
    wuss_window_t *win_m;

    tc_m.redraw_count    = 0;
    tc_m.mouse_count     = 0;
    delegate_m = mk_task(wuss, test_handle, &tc_m);
    if (delegate_m == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 210; box_m.y1 = 210; /* 200x200 content, floored at 80x60 */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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

  printf("test: toggle-size blits rather than redrawing the whole window\n");

  {
    static test_task_t    tc_t;
    wuss_task_t   *delegate_t;
    box_t          box_t_win, before, after, titlebar, toggle;
    wuss_window_t *win_t;
    int            outline_px, titlebar_height, inset, icon;
    int            i, dirty_area, full_area, cx, cy, old_icon_x, old_icon_y, found;
    int            interior_x, interior_y, interior_dirty;
    int            old_vscroll_x, old_vscroll_y, old_vscroll_found;

    tc_t.redraw_count = 0;
    tc_t.mouse_count  = 0;
    delegate_t = mk_task(wuss, test_handle, &tc_t);
    if (delegate_t == NULL) goto Failure;

    box_t_win.x0 = 10; box_t_win.y0 = 10;
    box_t_win.x1 = 50; box_t_win.y1 = 50; /* 40x40 content, room to grow to a 200x200 doc */
    rc = wuss_window_create(delegate_t, &box_t_win, "T", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_t);
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
      goto Failure; /* maximizing must stay on-screen: bounded by what's left
                      * of the screen from T's own x0/y0 (10,10), not by the
                      * screen's full width/height as if T were at the origin */

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
    static test_task_t    tc_r;
    wuss_task_t   *delegate_r;
    box_t          box_r, before, titlebar, toggle;
    wuss_window_t *win_r;
    int            outline_px, titlebar_height, inset, icon;
    int            i, interior_x, interior_y, interior_dirty, cx, cy;

    tc_r.redraw_count = 0;
    tc_r.mouse_count  = 0;
    delegate_r = mk_task(wuss, test_handle, &tc_r);
    if (delegate_r == NULL) goto Failure;

    box_r.x0 = 10; box_r.y0 = 10;
    box_r.x1 = 50; box_r.y1 = 50; /* 40x40 content; doc bigger than that, so it starts scrollable */
    rc = wuss_window_create(delegate_r, &box_r, "R", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_d;
    wuss_task_t   *delegate_d;
    box_t          box_d, content_before;
    wuss_window_t *win_d;
    point_t        scroll_d;
    int            i;
    int            top_x, top_y, mid_x, mid_y, bottom_x, bottom_y;
    int            top_dirty, mid_dirty, bottom_dirty;

    tc_d.redraw_count = 0;
    tc_d.mouse_count  = 0;
    delegate_d = mk_task(wuss, test_handle, &tc_d);
    if (delegate_d == NULL) goto Failure;

    box_d.x0 = 10; box_d.y0 = 10;
    box_d.x1 = 90; box_d.y1 = 90; /* 80x80 content; doc taller, so it starts scrollable */
    rc = wuss_window_create(delegate_d, &box_d, "D", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_d, tc_o;
    wuss_task_t   *delegate_d, *delegate_o;
    box_t          box_d, box_o, content_before;
    wuss_window_t *win_d, *win_o;
    point_t        scroll_d;
    int            i, split_x;
    int            occluded_x, occluded_y, kept_x, kept_y, strip_x, strip_y;
    int            occluded_dirty, kept_dirty, strip_dirty;

    tc_d.redraw_count = 0; tc_d.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_d = mk_task(wuss, test_handle, &tc_d);
    if (delegate_d == NULL) goto Failure;
    delegate_o = mk_task(wuss, test_handle, &tc_o);
    if (delegate_o == NULL) goto Failure;

    box_d.x0 = 10; box_d.y0 = 10;
    box_d.x1 = 110; box_d.y1 = 90; /* 100x80 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_d, &box_d, "D", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            SIZE2D(100, 140), SIZE2D(0, 0), &win_d);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_d, &content_before);
    split_x = (content_before.x0 + content_before.x1) / 2;

    /* occluder covering the right half of D's content box and beyond */
    box_o.x0 = split_x; box_o.y0 = content_before.y0 - 5;
    box_o.x1 = content_before.x1 + 40; box_o.y1 = content_before.y1 + 40;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_s, tc_o;
    wuss_task_t   *delegate_s, *delegate_o;
    box_t          box_s, box_o, content;
    wuss_window_t *win_s, *win_o;
    int            i, split_x;
    int            occluded_x, occluded_y, kept_x, kept_y, strip_x, strip_y;
    int            occluded_dirty, kept_dirty, strip_dirty;

    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_s = mk_task(wuss, test_handle, &tc_s);
    if (delegate_s == NULL) goto Failure;
    delegate_o = mk_task(wuss, test_handle, &tc_o);
    if (delegate_o == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 110; box_s.y1 = 90; /* 100x80 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            SIZE2D(100, 200), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_s, &content);
    split_x = (content.x0 + content.x1) / 2;

    box_o.x0 = split_x; box_o.y0 = content.y0 - 5;
    box_o.x1 = content.x1 + 40; box_o.y1 = content.y1 + 40;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    colour_t       cm;
    uint32_t       fb_m, fb_o;
    static test_task_t    tc_m, tc_o;
    wuss_task_t   *delegate_m, *delegate_o;
    box_t          box_m, box_o, content;
    wuss_window_t *win_m, *win_o;
    int            occ_x, occ_y, mid_x, mid_y, delta;

    cm = colour_rgb(0x11, 0x22, 0x33);
    tc_m.redraw_count = 0; tc_m.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_m = mk_task(wuss, paint_handle, &cm);
    if (delegate_m == NULL) goto Failure;
    delegate_o = mk_task(wuss, paint_handle, NULL);
    if (delegate_o == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 110; box_m.y1 = 110; /* 100x100 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    colour_t       cm;
    static test_task_t    tc_m, tc_o;
    wuss_task_t   *delegate_m, *delegate_o;
    box_t          box_m, box_o, content, ovis;
    wuss_window_t *win_m, *win_o;
    int            gx, gy, delta, bad;

    cm = colour_rgb(0x11, 0x22, 0x33);
    tc_m.redraw_count = 0; tc_m.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_m = mk_task(wuss, paint_handle, &cm);
    if (delegate_m == NULL) goto Failure;
    delegate_o = mk_task(wuss, paint_handle, NULL);
    if (delegate_o == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 110; box_m.y1 = 110; /* 100x100 content; doc taller, scrollable */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            SIZE2D(100, 300), SIZE2D(0, 0), &win_m);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_m, &content);

    /* O strictly inside M's content, gap on every side */
    box_o.x0 = content.x0 + 25; box_o.y0 = content.y0 + 25;
    box_o.x1 = content.x0 + 75; box_o.y1 = content.y0 + 65;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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

  printf("test: wuss_WINDOW_NO_RESIZE_BLIT redraws the whole window instead of blitting\n");

  {
    static test_task_t    tc_nb;
    wuss_task_t   *delegate_nb;
    box_t          box_nb, before, titlebar, toggle;
    wuss_window_t *win_nb;
    int            outline_px, titlebar_height, inset, icon;
    int            i, cx, cy, interior_x, interior_y, interior_dirty;

    tc_nb.redraw_count = 0;
    tc_nb.mouse_count  = 0;
    delegate_nb = mk_task(wuss, test_handle, &tc_nb);
    if (delegate_nb == NULL) goto Failure;

    box_nb.x0 = 10; box_nb.y0 = 10;
    box_nb.x1 = 50; box_nb.y1 = 50; /* 40x40 content, room to grow to a 200x200 doc */
    rc = wuss_window_create(delegate_nb, &box_nb, "NB", wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_u;
    wuss_task_t   *delegate_u;
    box_t          box_u, ub, titlebar, toggle;
    wuss_window_t *win_u;
    int            outline_px, titlebar_height, inset, icon;
    int            cx, cy;

    tc_u.redraw_count = 0;
    tc_u.mouse_count  = 0;
    delegate_u = mk_task(wuss, test_handle, &tc_u);
    if (delegate_u == NULL) goto Failure;

    box_u.x0 = 80; box_u.y0 = 80;
    box_u.x1 = 120; box_u.y1 = 120; /* 40x40 content */
    rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_u, "U", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_v;
    wuss_task_t   *delegate_v;
    box_t          box_v, vb, titlebar, toggle;
    wuss_window_t *win_v;
    int            outline_px, titlebar_height, inset, icon;
    int            cx, cy;

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
    rc = wuss_window_create(delegate_v, &box_v, "V", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
      goto Failure; /* screen-limited width/height went negative (x0 is
                      * further right than the screen edge plus furniture
                      * can make room for), producing an inverted box --
                      * un-hit-testable forever after, since box_contains_point
                      * can never match x0>x1: the window is stuck, unclickable,
                      * unclosable. Must floor at WUSS_MIN_CONTENT like
                      * drag-resize.c does, even if that leaves the maximized
                      * window hanging off the visible screen. */

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

  printf("test: dragging a back-most window with nothing above it still blits\n");

  {
    static test_task_t    tc_k, tc_l;
    wuss_task_t   *delegate_k, *delegate_l;
    box_t          box_k, box_l;
    wuss_window_t *win_k, *win_l;
    int            before_k, before_l;

    tc_k.redraw_count = 0;
    tc_k.mouse_count  = 0;
    delegate_k = mk_task(wuss, test_handle, &tc_k);
    if (delegate_k == NULL) goto Failure;

    box_k.x0 = 0; box_k.y0 = 140; /* clear of the still-open A/B windows above */
    box_k.x1 = 50; box_k.y1 = 175;
    rc = wuss_window_create(delegate_k,
                            &box_k,
                            "K",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_m2;
    wuss_task_t   *delegate_m2;
    box_t          box_m2, before2, after2, region;
    wuss_window_t *win_m2;
    int            i, dirty_area, full_area, interior_x, interior_y, interior_dirty;

    tc_m2.redraw_count = 0;
    tc_m2.mouse_count  = 0;
    delegate_m2 = mk_task(wuss, test_handle, &tc_m2);
    if (delegate_m2 == NULL) goto Failure;

    box_m2.x0 = 0; box_m2.y0 = 0;
    box_m2.x1 = 40; box_m2.y1 = 40;
    rc = wuss_window_create(delegate_m2,
                            &box_m2,
                            "M2",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_nb2;
    wuss_task_t   *delegate_nb2;
    box_t          box_nb2, before3, after3, region;
    wuss_window_t *win_nb2;
    int            i, dirty_area, full_area;

    tc_nb2.redraw_count = 0;
    tc_nb2.mouse_count  = 0;
    delegate_nb2 = mk_task(wuss, test_handle, &tc_nb2);
    if (delegate_nb2 == NULL) goto Failure;

    box_nb2.x0 = 0; box_nb2.y0 = 0;
    box_nb2.x1 = 40; box_nb2.y1 = 40;
    rc = wuss_window_create(delegate_nb2, &box_nb2, "NB2", wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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

  printf("test: dragging a clear window onto an occluder leaves the occluder untouched\n");

  {
    static test_task_t    tc_n, tc_o;
    wuss_task_t   *delegate_n, *delegate_o;
    box_t          box_n, box_o, visible_o, exposed, occluded, region;
    wuss_window_t *win_n, *win_o;
    int            i, exposed_dirty, occluded_dirty;

    tc_n.redraw_count = 0;
    tc_n.mouse_count  = 0;
    delegate_n = mk_task(wuss, test_handle, &tc_n);
    if (delegate_n == NULL) goto Failure;

    box_n.x0 = 0; box_n.y0 = 140; /* clear of any occluder to start */
    box_n.x1 = 60; box_n.y1 = 170;
    rc = wuss_window_create(delegate_n, &box_n, "N",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_a, tc_b;
    wuss_task_t   *delegate_a, *delegate_b;
    box_t          box_a, box_b, visible_a, visible_b_before;
    box_t          clean_new, hidden_new, region;
    wuss_window_t *win_a, *win_b;
    int            i, dx, clean_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 20; box_b.y0 = 10; /* left half will sit under A */
    box_b.x1 = 80; box_b.y1 = 50;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_a, tc_b;
    wuss_task_t   *delegate_a, *delegate_b;
    box_t          box_a, box_b, visible_a, visible_b_before;
    box_t          region;
    wuss_window_t *win_a, *win_b;
    int            i, occluder_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 0; box_b.y0 = 0; /* right part sits under A throughout */
    box_b.x1 = 60; box_b.y1 = 40;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_a, tc_b;
    wuss_task_t   *delegate_a, *delegate_b;
    box_t          box_a, box_b, visible_b_before;
    box_t          occluded_overlap, hidden_new, region;
    wuss_window_t *win_a, *win_b;
    int            i, occluded_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 0; box_b.y0 = 0; /* middle band sits under A */
    box_b.x1 = 60; box_b.y1 = 60;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_a, tc_b;
    wuss_task_t   *delegate_a, *delegate_b;
    box_t          box_a, box_b, visible_b_before, visible_a, region;
    wuss_window_t *win_a, *win_b;
    int            i;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 80; box_b.y0 = 80; /* corner already under A */
    box_b.x1 = 140; box_b.y1 = 140;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
      box_t  visible_b_after, whole_footprint, dirty_area_box;
      int    dirty_area, footprint_area;

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
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_s = { 0 };
    wuss_task_t   *delegate_s;
    box_t          box_s, content_s;
    wuss_window_t *win_s;
    point_t        scroll;

    delegate_s = mk_task(wuss, test_handle, &tc_s);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 60; box_s.y1 = 60; /* 50x50 content onto a 200x200 doc: room to scroll */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_r;
    wuss_task_t   *delegate_r;
    box_t          box_r, content_r;
    wuss_window_t *win_r;

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
                            wuss_WINDOW_NONE, /* all furniture present */
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    static test_task_t    tc_p[4];
    wuss_task_t   *delegate_p;
    wuss_window_t *win_p[4];
    box_t          vis[4], probe;
    int            k, m;

    memset(tc_p, 0, sizeof(tc_p));
    delegate_p = mk_task(wuss, test_handle, &tc_p[0]);
    if (delegate_p == NULL) goto Failure;

    for (k = 0; k < 4; k++)
    {
      rc = wuss_window_create_placed(delegate_p,
                                     SIZE2D(40, 30),
                                     "P",
                                     wuss_WINDOW_NONE,
                                     wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
                                   wuss_WINDOW_NONE,
                                   wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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

    static test_task_t  tc_pa, tc_pb;
    wuss_task_t *delegate_pa, *delegate_pb;

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

  printf("test: wuss_window_set_hidden fires PRE_SHOW; a veto keeps the window hidden\n");
  {
    static test_task_t  tc_ps;
    wuss_task_t   *delegate_ps;
    box_t          box_ps;
    wuss_window_t *win_ps;

    memset(&tc_ps, 0, sizeof(tc_ps));
    delegate_ps = mk_task(wuss, test_handle, &tc_ps);
    if (delegate_ps == NULL)
      goto Failure;

    box_ps.x0 = 0;  box_ps.y0 = 0;
    box_ps.x1 = 60; box_ps.y1 = 60;
    rc = wuss_window_create(delegate_ps, &box_ps, "PS", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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

    /* vetoed reveal: PRE_SHOW fires, returns the veto rc, no SHOW, window
     * stays hidden */
    tc_ps.veto_pre_show = 1;
    rc = wuss_window_set_hidden(win_ps, 0);
    if (rc != result_BAD_ARG)
      goto Failure;
    if (tc_ps.pre_show_count != 1 || tc_ps.show_count != 0)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                          wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit == win_ps)
      goto Failure; /* still hidden */
    (void) wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                            wuss_MOUSE_UP, &hit);

    /* allow it: PRE_SHOW then SHOW, and now it catches the pointer */
    tc_ps.veto_pre_show = 0;
    rc = wuss_window_set_hidden(win_ps, 0);
    if (rc != result_OK)
      goto Failure;
    if (tc_ps.pre_show_count != 2 || tc_ps.show_count != 1)
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

  printf("test: wuss_task_destroy sends one wuss_EVENT_QUIT and closes the task's windows\n");
  {
    static test_task_t    tc_q;
    wuss_task_t   *delegate_q;
    box_t          box_q;
    wuss_window_t *win_q1, *win_q2;

    memset(&tc_q, 0, sizeof(tc_q));
    delegate_q = mk_task(wuss, test_handle, &tc_q);
    if (delegate_q == NULL) goto Failure;

    box_q.x0 = 0;  box_q.y0 = 0;
    box_q.x1 = 40; box_q.y1 = 40;
    rc = wuss_window_create(delegate_q, &box_q, "Q1", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            box_size(&box_q), SIZE2D(0, 0), &win_q1);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_window_create(delegate_q, &box_q, "Q2", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    wuss_task_t   *delegate_ac;
    box_t          box_ac;
    wuss_window_t *win_ac1, *win_ac2;

    memset(&tc_ac, 0, sizeof(tc_ac));
    delegate_ac = mk_task(wuss, test_handle, &tc_ac);
    if (delegate_ac == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_ac, 1);

    box_ac.x0 = 0;  box_ac.y0 = 0;
    box_ac.x1 = 40; box_ac.y1 = 40;
    rc = wuss_window_create(delegate_ac, &box_ac, "AC1", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            box_size(&box_ac), SIZE2D(0, 0), &win_ac1);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_window_create(delegate_ac, &box_ac, "AC2", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    wuss_task_t   *delegate_w1, *delegate_w2, *delegate_w3;
    box_t          box_w;
    wuss_window_t *win_w1, *win_w3;

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
    rc = wuss_window_create(delegate_w1, &box_w, "W1", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            box_size(&box_w), SIZE2D(0, 0), &win_w1);
    if (rc != result_OK)
      goto Failure;

    delegate_w2 = mk_task(wuss, close_on_idle_handle, &tc_w2);
    if (delegate_w2 == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_w2, 1);
    rc = wuss_window_create(delegate_w2, &box_w, "W2", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            box_size(&box_w), SIZE2D(0, 0),
                            &g_close_on_idle_win);
    if (rc != result_OK)
      goto Failure;

    delegate_w3 = mk_task(wuss, test_handle, &tc_w3);
    if (delegate_w3 == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_w3, 1);
    rc = wuss_window_create(delegate_w3, &box_w, "W3", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
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
    wuss_menu_t *m;
    const wuss_menu_t *sub;

    /* "Root" is the root caption (first token), then items App,
     * File{ %s title discarded, Grid(!), Export(~), |Quit }, Help(> borrowed),
     * with "File" substituted for both %s. '|Quit' sets DASHED on Quit itself,
     * so the submenu holds 3 rows. */
    rc = wuss_menu_create_from_desc(&m,
           "Root, App, %s { %s, !Grid, ~Export, |Quit }, >Help",
           "File", "File", &borrowed);
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

    /* malformed: unbalanced brace */
    rc = wuss_menu_create_from_desc(&m, "T, A { B, C");
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

    dir = path_join_filename(resources, 2, "resources", "bmfonts");

    rc = wuss_fontmenu_create(&fm, dir, "Font", NULL);
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

    /* missing directory is surfaced, not swallowed */
    rc = wuss_fontmenu_create(&fm, "no/such/dir/here", NULL, NULL);
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

    rc = wuss_colourmenu_create(&cm, wuss, "Colour");
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

    if (wuss_colourmenu_create(&cm, NULL, "Colour") != result_NULL_ARG)
      goto ColourMenuFail;

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

    const char        *fontfile;
    bmfont_t          *font;
    screen_t           fscr;
    bitmap_t           fbm;
    void              *fpixels;
    wuss_t            *fwuss;
    test_task_t        ftc;
    wuss_task_t       *fowner;
    struct wuss__menu *chain;
    int                i;

    /* a menu needs a font for its row metrics; the core wuss above was made
     * without one */
    fontfile = path_join_filename(resources, 3, "resources", "bmfonts",
                                  path_join_leafname("tiny", "png"));
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

    rc = wuss_create(&fscr, font, NULL, 0, NULL, NULL, &fwuss);
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
#endif /* WUSS_ICONS */
#endif /* WUSS_MENUS */

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
  const wuss_window_flags_t chromeless =
    wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE | wuss_WINDOW_NO_CLOSE |
    wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE | wuss_WINDOW_NO_VSCROLL |
    wuss_WINDOW_NO_HSCROLL | wuss_WINDOW_NO_RESIZE;

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

  rc = wuss_create(&scr, NULL, NULL, 0, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create too small\n");

  box_a.x0 = 0; box_a.y0 = 0; box_a.x1 = 100; box_a.y1 = 0;
  rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_a, "toosmall", wuss_WINDOW_NONE,
                          wuss_NO_BACKGROUND, box_size(&box_a),
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
                          wuss_NO_BACKGROUND, SIZE2D(400, 400),
                          SIZE2D(0, 0), &win_a);
  if (rc != result_OK)
    goto Failure;

  box_b.x0 = 50; box_b.y0 = 50; box_b.x1 = 150; box_b.y1 = 150;
  rc = wuss_window_create(delegate_b, &box_b, "B", chromeless,
                          wuss_NO_BACKGROUND, SIZE2D(400, 400),
                          SIZE2D(0, 0), &win_b);
  if (rc != result_OK)
    goto Failure;

  /* With furniture off the content box is the visible box verbatim. */
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x0 != 0 || content.y0 != 0 ||
      content.x1 != 100 || content.y1 != 100)
    goto Failure;

  printf("test: window_resize can never grow a window past the screen\n");

  /* screen is 200x200; win_a sits at (0,0), chromeless. asking for a
   * 500x500 content area must clamp to the 200x200 screen. */
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 200 || content.y1 - content.y0 != 200)
    goto Failure;

  /* a request that already fits is left exactly as asked */
  rc = wuss_window_resize(win_a, SIZE2D(120, 90));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 120 || content.y1 - content.y0 != 90)
    goto Failure;

  /* a window whose top-left is offset only gets the space that is left */
  wuss_window_move(win_a, POINT(60, 40));
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 140 || content.y1 - content.y0 != 160)
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
                            wuss_NO_BACKGROUND, SIZE2D(400, 400),
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

  printf("test: wheel scroll delivers SCROLL event and moves offset\n");

  wuss_scroll(wuss, POINT(20, 20), 8, &hit);
  if (hit != win_a)
    goto Failure;
  wuss_window_get_scroll(win_a, &scroll);
  if (scroll.y != 13)
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
