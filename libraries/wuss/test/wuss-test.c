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
#include "io/filetype.h"
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/message.h"
#ifdef WUSS_MENUS
#include "wuss/menu.h"
#include "wuss/menu-desc.h"
#endif
#ifdef WUSS_COMPONENTS
#include "wuss/component/dialogue.h"
#include "wuss/component/fontmenu.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/saveas.h"
#include "wuss/gadget/colourset.h"
#endif
#ifdef WUSS_GADGETS
#include "wuss/gadget/stringset.h"
#include "wuss/gadget/gridview.h"
#endif

/* white-box: the furniture hit-test sweep calls wuss__furniture_hit_test and
 * the box helpers from impl.h / furniture.h directly */
#if defined(WUSS_ICONS)
#include "../core/impl.h"
#include "wuss/icon-spec.h"
#include "../icon.h"
#endif
#if defined(WUSS_ICONBAR)
#include "../iconbar.h"
#endif

#include "test/all-tests.h"
#include "test/tmpdir.h"
#include "harness.h"

/* ----------------------------------------------------------------------- */

/* The SDL interactive driver that used to live here (spawn_* callbacks, the
 * launcher table and the run_wuss loop) is now the standalone `wuss` app
 * in apps/wuss/main.c. This file is the Wuss unit test only. */

/* ----------------------------------------------------------------------- */

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

/* Non-zero if want[0..n-1] appear in that front-to-back order across all
 * three stacks. Other windows may be interleaved: earlier tests leave some. */
static int z_order_is(wuss_t *wuss, wuss_window_t *const *want, int n)
{
  wuss_window_t *w;
  int            i;

  i = 0;
  for (w = wuss__z_first(wuss); w != NULL && i < n; w = wuss__z_below(w))
    if (w == want[i])
      i++;

  return i == n;
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

#ifndef WUSS_GADGETS
  NOT_USED(resources); /* only the gadget and component tests read it */
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
  bad_config.furniture.pressed         = 0;
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

#ifdef WUSS_MENUS
  printf("test: wuss_destroy with a borrowed menu window open does not "
         "hand the focus back to a task it already freed\n");

  {
    /* task A's window has the focus when task B opens its own window as a
     * menu, so B's level saves A's window to restore the focus to. A is
     * reaped first; abandoning B's chain must not deliver GAIN_FOCUS to A's
     * window once A is gone. Caught by ASan. */
    static test_task_t tc_fa, tc_fb;
    wuss_t            *fw;
    wuss_task_t       *task_fa, *task_fb;
    wuss_window_t     *win_fa, *win_fb;
    box_t              fbox;
    wuss_icon_spec_t   fspec;

    rc = wuss_create(&scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &fw);
    if (rc != result_OK)
      goto Failure;

    memset(&tc_fa, 0, sizeof(tc_fa));
    memset(&tc_fb, 0, sizeof(tc_fb));
    task_fa = mk_task(fw, test_handle, &tc_fa);
    task_fb = mk_task(fw, test_handle, &tc_fb);
    forget_test_tasks(); /* wuss_destroy frees both */
    if (task_fa == NULL || task_fb == NULL) { wuss_destroy(fw); goto Failure; }

    fbox.x0 = 10; fbox.y0 = 30; fbox.x1 = 90; fbox.y1 = 90;
    rc = wuss_window_create(task_fa, &fbox, "A",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP, SIZE2D(80, 60), SIZE2D(0, 0),
                            &win_fa);
    if (rc == result_OK)
      rc = wuss_set_focus(fw, win_fa);
    if (rc == result_OK)
      rc = wuss_window_create(task_fb, &fbox, "B",
                              wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE |
                              wuss_WINDOW_HIDDEN,
                              wuss_NO_BACKDROP, SIZE2D(80, 60), SIZE2D(0, 0),
                              &win_fb);
    if (rc == result_OK)
    {
      /* a writable makes the level take the focus, saving A's window */
      wuss_icon_spec_writable(&fspec, (box_t) BOX_POS_SIZE(4, 4, 56, 16), "x",
                              8, 0);
      rc = wuss_icon_create(win_fb, &fspec, NULL);
    }
    if (rc == result_OK)
      rc = wuss_menu_open_window(task_fb, win_fb, POINT(20, 40), NULL);
    if (rc != result_OK) { wuss_destroy(fw); goto Failure; }

    wuss_destroy(fw);
  }
#endif

  printf("test: wuss_set_config swaps the config mid-session, refusing a "
         "bad colour without change\n");

  {
    static const colour_t cfgpal[5] =
    {
      { 0xFF0000FF }, { 0xFF00FF00 }, { 0xFFFF0000 },
      { 0xFFFFFFFF }, { 0xFF000000 }
    };

    static test_task_t tc_cfg;
    wuss_config_t      cfg;
    wuss_t            *cfgw;
    wuss_task_t       *cfgdel;

    memset(&cfg, 0, sizeof(cfg));
    cfg.furniture.title.bg = wuss_COLOUR_BLUE;  /* -> index 2 */
    cfg.furniture.title.fg = wuss_COLOUR_WHITE; /* -> index 3 */
    cfg.body.window        = wuss_COLOUR_RED;   /* -> index 0 */

    rc = wuss_create(&scr, NULL, 0, cfgpal, 5, &cfg, NULL, NULL, &cfgw);
    if (rc != result_OK)
      goto Failure;

    memset(&tc_cfg, 0, sizeof(tc_cfg));
    cfgdel = mk_task(cfgw, test_handle, &tc_cfg);
    if (cfgdel == NULL) { wuss_destroy(cfgw); goto Failure; }

    cfg.furniture.title.bg = wuss_COLOUR_RED;   /* -> index 0 */
    cfg.body.window        = wuss_COLOUR_WHITE; /* -> index 3 */
    rc = wuss_set_config(cfgw, &cfg);
    if (rc != result_OK ||
        wuss__resolve_colour(cfgw, wuss_COLOUR_TITLE_BG) != 0 ||
        wuss__resolve_colour(cfgw, wuss_COLOUR_WINDOW) != 3 ||
        tc_cfg.palette_count != 1)
    {
      wuss_destroy(cfgw);
      goto Failure;
    }

    /* index 7 is past the 5-entry palette: refused, nothing changes and
     * nothing is broadcast */
    cfg.furniture.title.bg = wuss_COLOUR_GREEN;
    cfg.furniture.title.fg = 7;
    cfg.body.window        = wuss_COLOUR_BLUE; /* stored before the check */
    rc = wuss_set_config(cfgw, &cfg);
    if (rc != result_WUSS_BAD_COLOUR ||
        wuss__resolve_colour(cfgw, wuss_COLOUR_TITLE_BG) != 0 ||
        wuss__resolve_colour(cfgw, wuss_COLOUR_TITLE_FG) != 3 ||
        cfgw->body.window != 3 ||
        tc_cfg.palette_count != 1)
    {
      wuss_destroy(cfgw);
      goto Failure;
    }

    wuss_destroy(cfgw); /* sweeps cfgdel too */
    mk_task_count = 0;
    rc = result_OK;
  }

  printf("test: wuss_resize moves windows back on-screen, shrinking only "
         "those bigger than the new screen\n");

  {
    static const wuss_window_flags_t bare = wuss_WINDOW_NO_TITLEBAR |
                                            wuss_WINDOW_NO_OUTLINE;

    bitmap_t       small_bm;
    screen_t       small_scr;
    wuss_t        *rsw;
    wuss_task_t   *rsdel;
    box_t          box_fit, box_over;
    wuss_window_t *win_fit, *win_over;

    /* a 100x100 view onto the same pixels (rowbytes stays 200 wide) */
    rc = bitmap_init(&small_bm, SIZE2D(100, 100), pixelfmt_bgrx8888,
                     rowbytes, NULL, pixels);
    if (rc != result_OK)
      goto Failure;
    screen_for_bitmap(&small_scr, &small_bm);

    rc = wuss_create(&scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &rsw);
    if (rc != result_OK)
      goto Failure;

    rsdel = mk_task(rsw, NULL, NULL);
    if (rsdel == NULL) { wuss_destroy(rsw); goto Failure; }

    /* furniture-less, so visible == content: nothing to drag or resize by
     * if the mode change were to shrink it */
    box_fit.x0 = 140; box_fit.y0 = 140; box_fit.x1 = 180; box_fit.y1 = 180;
    rc = wuss_window_create(rsdel, &box_fit, "fit", bare, wuss_NO_BACKDROP,
                            SIZE2D(40, 40), SIZE2D(0, 0), &win_fit);
    if (rc != result_OK) { wuss_destroy(rsw); goto Failure; }

    box_over.x0 = 20; box_over.y0 = 20; box_over.x1 = 170; box_over.y1 = 170;
    rc = wuss_window_create(rsdel, &box_over, "over", bare, wuss_NO_BACKDROP,
                            SIZE2D(150, 150), SIZE2D(0, 0), &win_over);
    if (rc != result_OK) { wuss_destroy(rsw); goto Failure; }

    rc = wuss_resize(rsw, &small_scr);
    if (rc != result_OK) { wuss_destroy(rsw); goto Failure; }

    /* fits the new screen: keeps its 40x40 size, pushed up-left to fit */
    wuss_window_get_content_bounds(win_fit, &content);
    if (content.x0 != 60 || content.y0 != 60 ||
        content.x1 != 100 || content.y1 != 100)
    {
      wuss_destroy(rsw);
      goto Failure;
    }

    /* bigger than the new screen: shrunk to it and pinned at the origin */
    wuss_window_get_content_bounds(win_over, &content);
    if (content.x0 != 0 || content.y0 != 0 ||
        content.x1 != 100 || content.y1 != 100)
    {
      wuss_destroy(rsw);
      goto Failure;
    }

    wuss_destroy(rsw); /* sweeps rsdel too */
    mk_task_count = 0; /* drop the now-stale registry entry */
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

  printf("test: ADJUST on the close icon sends CLOSE_ADJUST, with the held modifiers, and keeps the window\n");

  tc_a.pre_close_count = 0;
  wuss_set_modifiers(wuss, wuss_KEY_MOD_SHIFT);
  rc = wuss_mouse_click(wuss, POINT(6, 11), wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* A's close icon */
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 36), wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit); /* if the press had started a drag, this would move A */
  if (rc != result_OK)
    goto Failure;

  wuss_set_modifiers(wuss, wuss_KEY_MOD_NONE);

  if (tc_a.close_adjust_count != 1 || tc_a.pre_close_count != 0)
    goto Failure; /* told the task, did not try to close */
  if (tc_a.close_adjust_mods != wuss_KEY_MOD_SHIFT)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible); /* still alive */
  if (visible.x0 != 0 || visible.y0 != 0)
    goto Failure; /* unmoved: no drag was started */

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

  printf("test: two same-button, same-window DOWNs within the double-click "
        "window carry wuss_BUTTON_DOUBLE on the second; a third does not "
        "chain into another double\n");

  wuss_set_time(wuss, 1000);
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (tc_b.last_button & wuss_BUTTON_DOUBLE)
    goto Failure; /* first press of the pair: not a double */

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  wuss_set_time(wuss, 1100); /* 100ms later, well within the 400ms default */
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (!(tc_b.last_button & wuss_BUTTON_DOUBLE))
    goto Failure; /* same window, same button, within time and distance */

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  wuss_set_time(wuss, 1150);
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (tc_b.last_button & wuss_BUTTON_DOUBLE)
    goto Failure; /* a third press starts a fresh pair, does not chain */

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: a same-window DOWN outside double_click_ms does not carry "
        "wuss_BUTTON_DOUBLE\n");

  wuss_set_time(wuss, 2000);
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  wuss_set_time(wuss, 2500); /* 500ms later, past the 400ms default */
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (tc_b.last_button & wuss_BUTTON_DOUBLE)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: a DOWN on a different window resets the double-click "
        "pair even within the time window\n");

  wuss_set_time(wuss, 3000);
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* B */
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  wuss_set_time(wuss, 3100);
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A, different window */
  if (rc != result_OK)
    goto Failure;
  if (tc_a.last_button & wuss_BUTTON_DOUBLE)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
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

  printf("test: window_resize caps the size at the screen edge\n");

  /* screen is 200x200; win_a's visible box sits at (0,25) with a 1px
   * outline all round and a 20px titlebar. asking for a 500x500 content
   * area is capped so the visible box ends at the screen edge (198 wide,
   * 153 tall of content). */
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 198 || content.y1 - content.y0 != 153)
    goto Failure;
  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x1 != 200 || visible.y1 != 200)
    goto Failure;

  /* a smaller request is honoured verbatim */
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

  printf("test: drag-resize stops where the window's furniture still fits\n");

  {
    static test_task_t tc_f;
    wuss_task_t       *delegate_f;
    box_t              box_f, content, visible;
    box_t              close, toggle, vup, vdown, hleft, hright;
    wuss_window_t     *win_f;

    delegate_f = mk_task(wuss, test_handle, &tc_f);
    if (delegate_f == NULL) goto Failure;

    /* a doc smaller than the furniture: only the furniture floor holds it */
    box_f.x0 = 10; box_f.y0 = 10;
    box_f.x1 = 110; box_f.y1 = 110;
    rc = wuss_window_create(delegate_f, &box_f, "F", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1, 1), SIZE2D(0, 0), &win_f);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_f, &visible);
    wuss_window_get_content_bounds(win_f, &content);

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* F's resize icon */
    if (rc != result_OK || hit != win_f)
      goto Failure;
    rc = wuss_mouse_move(wuss, POINT(content.x0, content.y0), &hit); /* try to squash it flat */
    if (rc != result_OK)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(content.x0, content.y0), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss__close_box(win_f, &close);
    wuss__toggle_box(win_f, &toggle);
    wuss__vscroll_up_box(win_f, &vup);
    wuss__vscroll_down_box(win_f, &vdown);
    wuss__hscroll_left_box(win_f, &hleft);
    wuss__hscroll_right_box(win_f, &hright);

    if (toggle.x0 < close.x1 + WUSS_BUTTON_INSET)
      goto Failure; /* titlebar icons overlap */
    if (vdown.y0 < vup.y1 || hright.x0 < hleft.x1)
      goto Failure; /* scroll arrows overlap */

    wuss_window_close(win_f);
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

  printf("test: closing a hidden window never dirties the visible window sitting under its old footprint\n");

  {
    static test_task_t tc_c;
    wuss_task_t       *delegate_c;
    box_t              box_visible, box_hidden, region;
    wuss_window_t     *win_visible, *win_hidden_c;
    int                i, overlap_x, overlap_y, overlap_dirty;

    tc_c.redraw_count = 0;
    tc_c.mouse_count  = 0;
    delegate_c = mk_task(wuss, test_handle, &tc_c);
    if (delegate_c == NULL) goto Failure;

    /* a visible window occupying the screen's top-left, matching where a
     * hidden singleton dialogue (e.g. wuss_proginfo_window) is always
     * created (content box (0,0)-(w,h): see wuss_info_create) */
    box_visible.x0 = 0; box_visible.y0 = 0;
    box_visible.x1 = 80; box_visible.y1 = 80;
    rc = wuss_window_create(delegate_c, &box_visible, "V", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_visible);
    if (rc != result_OK)
      goto Failure;

    /* hidden window whose footprint overlaps win_visible's -- never drawn,
     * so closing it must not touch pixels that belong to win_visible */
    box_hidden.x0 = 0; box_hidden.y0 = 0;
    box_hidden.x1 = 40; box_hidden.y1 = 40;
    rc = wuss_window_create(delegate_c, &box_hidden, "Hidden",
                            wuss_WINDOW_HIDDEN, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_hidden_c);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates' own invalidates */
    if (rc != result_OK)
      goto Failure;

    overlap_x = (box_hidden.x0 + box_hidden.x1) / 2;
    overlap_y = (box_hidden.y0 + box_hidden.y1) / 2;

    wuss_window_close(win_hidden_c);

    overlap_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, overlap_x, overlap_y))
        overlap_dirty = 1;
    }
    if (overlap_dirty)
      goto Failure; /* closing a hidden window must not force a repaint of
                      * whatever visible window/backdrop actually occupies
                      * its old, never-drawn footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_visible);
  }

  printf("test: input focus: flag gate, click-to-focus, key routing, loss on hide/close\n");

  {
    static test_task_t tc_f, tc_n;
    wuss_task_t       *delegate_f, *delegate_n;
    box_t              box_f, box_g, box_n, content_f, content_n;
    wuss_window_t     *win_f, *win_g, *win_n;
    int                claimed;

    delegate_f = mk_task(wuss, test_handle, &tc_f);
    delegate_n = mk_task(wuss, test_handle, &tc_n);
    if (delegate_f == NULL || delegate_n == NULL) goto Failure;

    box_f.x0 = 10;  box_f.y0 = 30;  box_f.x1 = 60;  box_f.y1 = 80;
    box_g.x0 = 70;  box_g.y0 = 30;  box_g.x1 = 120; box_g.y1 = 80;
    box_n.x0 = 130; box_n.y0 = 30;  box_n.x1 = 180; box_n.y1 = 80;
    rc = wuss_window_create(delegate_f, &box_f, "F",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_f);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(delegate_f, &box_g, "G",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_g);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(delegate_n, &box_n, "N", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_n);
    if (rc != result_OK) goto Failure;

    wuss_window_get_content_bounds(win_f, &content_f);
    wuss_window_get_content_bounds(win_n, &content_n);

    rc = wuss_redraw_dirty(wuss); /* flush the creates' own invalidates */
    if (rc != result_OK) goto Failure;

    /* nothing focused: keys go unclaimed */
    if (wuss_get_focus(wuss) != NULL) goto Failure;
    rc = wuss_key(wuss, 'a', wuss_KEY_MOD_NONE, &claimed);
    if (rc != result_OK || claimed) goto Failure;

    /* the flag gates wuss_set_focus */
    if (wuss_set_focus(wuss, win_n) != result_BAD_ARG) goto Failure;

    /* a Menu press never takes focus; a Select press on content does, and
     * GAIN_FOCUS arrives */
    wuss_mouse_click(wuss, POINT(content_f.x0 + 5, content_f.y0 + 5),
                     wuss_BUTTON_MENU, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(content_f.x0 + 5, content_f.y0 + 5),
                     wuss_BUTTON_MENU, wuss_MOUSE_UP, NULL);
    if (wuss_get_focus(wuss) != NULL) goto Failure;
    wuss_mouse_click(wuss, POINT(content_f.x0 + 5, content_f.y0 + 5),
                     wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(content_f.x0 + 5, content_f.y0 + 5),
                     wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (wuss_get_focus(wuss) != win_f || tc_f.gain_focus_count != 1)
      goto Failure;

    /* the focused titlebar is repainted in the tint */
    if (wuss_get_dirty_count(wuss) == 0) goto Failure;

    /* keys reach the focused window, with modifiers */
    rc = wuss_key(wuss, wuss_KEY_LEFT, wuss_KEY_MOD_SHIFT, &claimed);
    if (rc != result_OK || !claimed || tc_f.key_count != 1 ||
        tc_f.last_key != wuss_KEY_LEFT ||
        tc_f.last_key_mods != wuss_KEY_MOD_SHIFT)
      goto Failure;

    /* a declined key is reported unclaimed */
    tc_f.unclaim_keys = 1;
    rc = wuss_key(wuss, 'x', wuss_KEY_MOD_CTRL, &claimed);
    if (rc != result_OK || claimed || tc_f.key_count != 2) goto Failure;
    tc_f.unclaim_keys = 0;

    /* clicking a non-focusable window leaves the focus alone */
    wuss_mouse_click(wuss, POINT(content_n.x0 + 5, content_n.y0 + 5),
                     wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(content_n.x0 + 5, content_n.y0 + 5),
                     wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (wuss_get_focus(wuss) != win_f || tc_n.key_count != 0) goto Failure;

    /* refocusing the holder fires nothing */
    if (wuss_set_focus(wuss, win_f) != result_OK ||
        tc_f.gain_focus_count != 1 || tc_f.lose_focus_count != 0)
      goto Failure;

    /* moving focus: LOSE then GAIN */
    if (wuss_set_focus(wuss, win_g) != result_OK ||
        tc_f.lose_focus_count != 1 || tc_f.gain_focus_count != 2)
      goto Failure;

    /* hiding the focused window drops focus with LOSE; a hidden window
     * can't be focused */
    rc = wuss_window_set_hidden(win_g, 1);
    if (rc != result_OK || wuss_get_focus(wuss) != NULL ||
        tc_f.lose_focus_count != 2)
      goto Failure;
    if (wuss_set_focus(wuss, win_g) != result_BAD_ARG) goto Failure;

    /* try_close fires LOSE; forced close clears silently */
    if (wuss_set_focus(wuss, win_f) != result_OK) goto Failure;
    rc = wuss_window_try_close(win_f);
    if (rc != result_OK || wuss_get_focus(wuss) != NULL ||
        tc_f.lose_focus_count != 3)
      goto Failure;

    rc = wuss_window_set_hidden(win_g, 0);
    if (rc != result_OK) goto Failure;
    if (wuss_set_focus(wuss, win_g) != result_OK) goto Failure;
    wuss_window_close(win_g);
    if (wuss_get_focus(wuss) != NULL || tc_f.lose_focus_count != 3)
      goto Failure;

    wuss_window_close(win_n);
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;
  }

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)
#endif

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

  printf("test: two wheel scrolls delivered before wuss_redraw_dirty runs never blit the first scroll's undrawn strip into the second scroll's content\n");

  {
    /* Regression: fast wheel spinning can call wuss_window_set_scroll twice
     * (or more) before wuss_redraw_dirty ever runs. The first call's blit
     * shifts old content and marks the newly-exposed strip dirty but does
     * NOT paint it -- that strip holds pre-scroll pixels until the redraw
     * runs. If the second call's blit source includes that still-dirty
     * strip, it smears those stale pixels across the content box instead of
     * being excluded per the "stale" collection in wuss_window_set_scroll.
     * paint_handle paints one-pixel rows keyed by doc_y in the blue channel,
     * so a stale/smeared row is detectable exactly after a single final
     * redraw. */
    colour_t           cw;
    static test_task_t tc_w;
    wuss_task_t       *delegate_w;
    box_t              box_w, content_w;
    wuss_window_t     *win_w;
    int                wx, wy, bad;

    cw = colour_rgb(0x55, 0x66, 0x77);
    tc_w.redraw_count = 0; tc_w.mouse_count = 0;
    delegate_w = mk_task(wuss, paint_handle, &cw);
    if (delegate_w == NULL) goto Failure;

    box_w.x0 = 10; box_w.y0 = 10;
    box_w.x1 = 90; box_w.y1 = 90; /* 80x80 content; doc taller, scrollable */
    rc = wuss_window_create(delegate_w, &box_w, "W", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 200), SIZE2D(0, 0), &win_w);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_w, &content_w);

    /* two wheel notches back-to-back, no redraw in between -- exactly what
     * fast spinning delivers */
    wuss_window_set_scroll(win_w, POINT(0, 10));
    wuss_window_set_scroll(win_w, POINT(0, 20));

    rc = wuss_redraw_dirty(wuss); /* the only flush, as fast scrolling leaves it */
    if (rc != result_OK)
      goto Failure;

    /* every row of the content box must show the doc_y the final scroll
     * offset (20) puts there -- no row may still carry pixels from the
     * intermediate offset (10) or from before either scroll */
    bad = 0;
    wx  = (content_w.x0 + content_w.x1) / 2;
    for (wy = content_w.y0; wy < content_w.y1; wy++)
    {
      uint32_t px, blue;

      px   = ((const uint32_t *) pixels)[wy * 200 + wx];
      blue = px & 0xff;
      if (blue != (uint32_t) ((wy - content_w.y0 + 20) & 0xff))
        bad = 1;
    }
    if (bad)
      goto Failure; /* a stacked scroll's blit reused a stale strip */

    wuss_window_close(win_w);
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
     * anything still sitting in wuss->dirty.box[] (queued by an earlier call this
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
#ifdef WUSS_ICONBAR
      /* clamp above the icon bar's fixed bottom strip too: it always draws
       * on top of every window, same reasoning as the occluder skip in the
       * next test below */
      content_e.y1 = MIN(content_e.y1, 200 - WUSS_ICONBAR_HEIGHT);
#endif
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
#ifdef WUSS_ICONBAR
      /* clamp above the icon bar's fixed bottom strip too: it always draws
       * on top of every window, same reasoning as the occluder skip below */
      content_f.y1 = MIN(content_f.y1, 200 - WUSS_ICONBAR_HEIGHT);
#endif
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

    /* placement avoids every shown window, so clear out windows left open
     * by earlier tests */
    reap_test_tasks();

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

    /* placement honours where windows actually are: free the second slot
     * again, drag the first window into it, and the next placed window must
     * avoid the moved window rather than land in the stale free slot */
    wuss_window_close(win_p[1]);
    wuss_window_move(win_p[0], POINT(probe.x0 + 5, probe.y0 + 20));
    wuss_window_get_visible_bounds(win_p[0], &vis[0]);

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
    if (box_intersects(&vis[0], &vis[1]))
      goto Failure; /* placed over a moved window */

    for (k = 0; k < 4; k++)
      wuss_window_close(win_p[k]);
  }

  printf("test: wuss_window_create_at anchors, places relative and avoids "
         "the icon bar\n");

  {
    static test_task_t tc_a;
    wuss_task_t       *delegate_a;
    wuss_place_t       place;
    wuss_window_t     *win_a, *win_b;
    box_t              vis_a, vis_b;
    int                bottom;
#ifdef WUSS_ICONBAR
    wuss_iconbar_icon_spec_t spec;
    wuss_iconbar_icon_t     *icon;
#endif

    reap_test_tasks();

    memset(&tc_a, 0, sizeof(tc_a));
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    /* top right: flush with the gutter on both edges it is against */
    memset(&place, 0, sizeof(place));
    place.align = wuss_ALIGN_TOP | wuss_ALIGN_RIGHT;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "A",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_a);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_a, &vis_a);
    if (vis_a.y0 != WUSS_PLACE_GUTTER || vis_a.x1 != 200 - WUSS_PLACE_GUTTER)
      goto Failure;

    /* relative: the footprint's top-left is offset from the other window's */
    memset(&place, 0, sizeof(place));
    place.relative_to = win_a;
    place.offset      = POINT(-32, 32);
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != vis_a.x0 - 32 || vis_b.y0 != vis_a.y0 + 32)
      goto Failure;
    wuss_window_close(win_b);

    /* relative past the bottom-right corner: clamped back onto the screen */
    place.offset = POINT(500, 500);
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x1 != 200 || vis_b.y1 != 200)
      goto Failure;
    wuss_window_close(win_b);

    /* centred (zero-initialised) */
    memset(&place, 0, sizeof(place));
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != (200 - (vis_b.x1 - vis_b.x0)) / 2 ||
        vis_b.y0 != (200 - (vis_b.y1 - vis_b.y0)) / 2)
      goto Failure;
    wuss_window_close(win_b);

    /* bottom left, avoiding the icon bar: sits a gutter above the bar */
    bottom = 200;
#ifdef WUSS_ICONBAR
    memset(&spec, 0, sizeof(spec));
    spec.text = "T";
    rc = wuss_iconbar_icon_create(wuss, delegate_a, &spec, &icon);
    if (rc != result_OK)
      goto Failure;
    bottom = 200 - WUSS_ICONBAR_HEIGHT;
#endif
    memset(&place, 0, sizeof(place));
    place.flags = wuss_PLACE_AVOID_ICONBAR;
    place.align = wuss_ALIGN_BOTTOM | wuss_ALIGN_LEFT;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != WUSS_PLACE_GUTTER ||
        vis_b.y1 != bottom - WUSS_PLACE_GUTTER)
      goto Failure;
    wuss_window_close(win_b);

    /* spare space first, searched from the anchor's corner: lands in the
     * free bottom-left, clear of A */
    place.flags |= wuss_PLACE_SPARE;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != WUSS_PLACE_GUTTER ||
        vis_b.y1 != bottom - WUSS_PLACE_GUTTER ||
        box_intersects(&vis_a, &vis_b))
      goto Failure;
    wuss_window_close(win_b);

    /* no anchor: searched from the top-left, as before */
    place.align = wuss_ALIGN_CENTRE;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != WUSS_PLACE_GUTTER || vis_b.y0 != WUSS_PLACE_GUTTER ||
        box_intersects(&vis_a, &vis_b))
      goto Failure;
    wuss_window_close(win_b);

    /* bottom right: A leaves no room against the right edge, so nearest
     * that corner is the bottom row, clear of A, not the top-left */
    place.align = wuss_ALIGN_BOTTOM | wuss_ALIGN_RIGHT;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 <= WUSS_PLACE_GUTTER ||
        vis_b.y1 != bottom - WUSS_PLACE_GUTTER ||
        box_intersects(&vis_a, &vis_b))
      goto Failure;
    wuss_window_close(win_b);

#ifdef WUSS_ICONBAR
    wuss_iconbar_icon_destroy(wuss, icon);
#endif
    reap_test_tasks();
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

  printf("test: a dragged window snaps flush to a screen edge it comes "
         "within WUSS_SNAP_PX of\n");

  {
    static test_task_t tc_sn;
    wuss_task_t       *delegate_sn;
    box_t              box_sn, start, visible;
    wuss_window_t     *win_sn;
    point_t            grab;
    size2d_t           scr;

    delegate_sn = mk_task(wuss, test_handle, &tc_sn);
    if (delegate_sn == NULL) goto Failure;

    box_sn.x0 = 40; box_sn.y0 = 40;
    box_sn.x1 = 100; box_sn.y1 = 100;
    rc = wuss_window_create(delegate_sn, &box_sn, "SN", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_sn);
    if (rc != result_OK) goto Failure;

    scr = wuss->scr->size;
    wuss_window_get_visible_bounds(win_sn, &start);
    grab = POINT((start.x0 + start.x1) / 2, start.y0 + 3); /* titlebar */

    rc = wuss_mouse_click(wuss, grab, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_sn) goto Failure;

    /* within range of the top-left corner: snaps to both edges */
    wuss_mouse_move(wuss, POINT(grab.x - start.x0 + WUSS_SNAP_PX - 1, grab.y - start.y0 + WUSS_SNAP_PX - 1), NULL);
    wuss_window_get_visible_bounds(win_sn, &visible);
    if (visible.x0 != 0 || visible.y0 != 0) goto Failure;

    /* just out of range: follows the pointer */
    wuss_mouse_move(wuss, POINT(grab.x - start.x0 + WUSS_SNAP_PX, grab.y - start.y0 + WUSS_SNAP_PX), NULL);
    wuss_window_get_visible_bounds(win_sn, &visible);
    if (visible.x0 != WUSS_SNAP_PX || visible.y0 != WUSS_SNAP_PX) goto Failure;

    /* within range of the bottom-right corner: snaps to both edges */
    wuss_mouse_move(wuss, POINT(grab.x + scr.w - start.x1 - 2, grab.y + scr.h - start.y1 - 2), NULL);
    wuss_window_get_visible_bounds(win_sn, &visible);
    if (visible.x1 != scr.w || visible.y1 != scr.h) goto Failure;

    wuss_mouse_click(wuss, grab, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    wuss_window_close(win_sn);
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

  printf("test: window stacks: top over middle over back, restack stays within a stack, set_stack relinks\n");

  {
    wuss_task_t   *owner;
    wuss_window_t *back, *mid_a, *mid_b, *top, *bad;
    wuss_window_t *order[4];
    box_t          box;

    owner = mk_task(wuss, paint_handle, NULL);
    if (owner == NULL) goto Failure;

    /* four windows stacked on the same spot, created back stack first */
    box = (box_t) BOX_POS_SIZE(20, 40, 60, 60);
    rc  = wuss_window_create(owner, &box, "B", wuss_WINDOW_DEFAULT |
                             wuss_WINDOW_STACK_BACK, wuss_NO_BACKDROP,
                             SIZE2D(60, 60), SIZE2D(20, 20), &back);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(owner, &box, "A", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, SIZE2D(60, 60), SIZE2D(20, 20),
                            &mid_a);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(owner, &box, "M", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, SIZE2D(60, 60), SIZE2D(20, 20),
                            &mid_b);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(owner, &box, "T", wuss_WINDOW_DEFAULT |
                            wuss_WINDOW_STACK_TOP, wuss_NO_BACKDROP,
                            SIZE2D(60, 60), SIZE2D(20, 20), &top);
    if (rc != result_OK) goto Failure;

    /* a window is frontmost in its stack, and every stack is in front of
     * the next regardless of creation order */
    order[0] = top; order[1] = mid_b; order[2] = mid_a; order[3] = back;
    if (!z_order_is(wuss, order, 4)) goto Failure;
    if (wuss__window_at(wuss, POINT(50, 80)) != top) goto Failure;

    /* front and back only move a window within its own stack */
    wuss_window_restack(mid_a, wuss_ZORDER_FRONT);
    order[1] = mid_a; order[2] = mid_b;
    if (!z_order_is(wuss, order, 4)) goto Failure;

    wuss_window_restack(back, wuss_ZORDER_FRONT);
    wuss_window_restack(top, wuss_ZORDER_BACK);
    if (!z_order_is(wuss, order, 4)) goto Failure; /* each alone in its stack */

    wuss_window_restack(mid_a, wuss_ZORDER_BACK);
    order[1] = mid_b; order[2] = mid_a;
    if (!z_order_is(wuss, order, 4)) goto Failure;

    /* set_stack lands at the front of the destination stack */
    wuss_window_set_stack(mid_a, wuss_STACK_TOP);
    order[0] = mid_a; order[1] = top; order[2] = mid_b;
    if (!z_order_is(wuss, order, 4)) goto Failure;
    if (wuss__window_at(wuss, POINT(50, 80)) != mid_a) goto Failure;

    wuss_window_set_stack(mid_a, wuss_STACK_BACK);
    order[0] = top; order[1] = mid_b; order[2] = mid_a; order[3] = back;
    if (!z_order_is(wuss, order, 4)) goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;

    /* a window cannot be in two stacks at once */
    rc = wuss_window_create(owner, &box, "X", wuss_WINDOW_STACK_TOP |
                            wuss_WINDOW_STACK_BACK, wuss_NO_BACKDROP,
                            SIZE2D(60, 60), SIZE2D(20, 20), &bad);
    if (rc != result_BAD_ARG) goto Failure;

    wuss_window_close(top);
    wuss_window_close(mid_a);
    wuss_window_close(mid_b);
    wuss_window_close(back);

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;
  }

  printf("test: set_title truncates at WUSS_TITLE_MAX without splitting a "
         "multi-byte UTF-8 codepoint\n");

  {
    /* WUSS_TITLE_MAX - 2 ASCII bytes then a 3-byte UTF-8 codepoint (U+20AC,
     * EURO SIGN: 0xE2 0x82 0xAC) -- WUSS_TITLE_MAX lands on the euro sign's
     * middle continuation byte, so a raw byte truncation to
     * WUSS_TITLE_MAX chars (as strncpy alone would do) keeps 0xE2 0x82 and drops 0xAC,
     * leaving a malformed trailing sequence. The fix must back off over
     * both continuation bytes instead, dropping the whole codepoint. */
    static test_task_t tc_title;
    wuss_task_t       *delegate_title;
    wuss_window_t     *win_title;
    box_t              box_title;
    char               long_title[WUSS_TITLE_MAX + 8];
    const char        *got;
    size_t             len;

    tc_title.redraw_count = 0;
    tc_title.mouse_count  = 0;
    delegate_title = mk_task(wuss, test_handle, &tc_title);
    if (delegate_title == NULL) goto Failure;

    box_title.x0 = 10; box_title.y0 = 10;
    box_title.x1 = 50; box_title.y1 = 50;
    rc = wuss_window_create(delegate_title, &box_title, "T",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_title);
    if (rc != result_OK)
      goto Failure;

    memset(long_title, 'a', WUSS_TITLE_MAX - 2);
    long_title[WUSS_TITLE_MAX - 2] = (char) 0xE2;
    long_title[WUSS_TITLE_MAX - 1] = (char) 0x82;
    long_title[WUSS_TITLE_MAX]     = (char) 0xAC;
    long_title[WUSS_TITLE_MAX + 1] = '\0';

    wuss_window_set_title(win_title, long_title);
    got = wuss_window_get_title(win_title);
    len = strlen(got);

    if (len > WUSS_TITLE_MAX - 2)
      goto Failure; /* the euro sign's lead byte (0xE2) must have been
                      * dropped along with its continuations, not kept
                      * dangling on its own */
    if (len != WUSS_TITLE_MAX - 2)
      goto Failure; /* the leading ASCII bytes must all survive */
    if (((unsigned char) got[WUSS_TITLE_MAX - 3]) != 'a')
      goto Failure;

    wuss_window_close(win_title);
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

  printf("test: window_resize caps the size at the screen edge\n");

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
