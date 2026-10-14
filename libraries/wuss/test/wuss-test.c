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

/* ----------------------------------------------------------------------- */

/* ----------------------------------------------------------------------- */

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
