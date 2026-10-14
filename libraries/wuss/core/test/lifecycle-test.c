/* core/test/lifecycle-test.c -- wuss create, destroy, config and resize tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#ifdef WUSS_MENUS
#include "wuss/menu.h"
#endif
#include "wuss/icon-spec.h"
#include "../../icon.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

result_t wuss_lifecycle_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  int            rowbytes;
  wuss_config_t  bad_config;
  wuss_t        *bad_wuss;
  wuss_t        *wuss;
  box_t          content;
  const colour_t custom_palette[2] = { 0, 0 };

  NOT_USED(resources);

  rowbytes = 200 * 4;
  rc       = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;

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
  rc = wuss_create(&fixture.scr, NULL, 0, NULL, 0, &bad_config, NULL, NULL, &bad_wuss);
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  printf("test: wuss_create with custom palette, no config\n");

  {
    wuss_t *custom_wuss;

    rc = wuss_create(&fixture.scr, NULL, 0, custom_palette, 2, NULL, NULL, NULL, &custom_wuss);
    if (rc != result_OK)
      goto Failure;
    wuss_destroy(custom_wuss);
  }

  printf("test: wuss_create with default palette\n");

  rc = wuss_create(&fixture.scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;
  wuss_destroy(wuss);

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

    rc = wuss_create(&fixture.scr, NULL, 0, sympal, 5, &symcfg, NULL, NULL, &symw);
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

    rc = wuss_create(&fixture.scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &fw);
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

    rc = wuss_create(&fixture.scr, NULL, 0, cfgpal, 5, &cfg, NULL, NULL, &cfgw);
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
                     rowbytes, NULL, fixture.pixels);
    if (rc != result_OK)
      goto Failure;
    screen_for_bitmap(&small_scr, &small_bm);

    rc = wuss_create(&fixture.scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &rsw);
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


  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:

  wuss_fixture_close(&fixture);
  printf("wuss_lifecycle_test: failed\n");

  return result_TEST_FAILED;
}

#else

result_t wuss_lifecycle_test(const char *resources)
{
  NOT_USED(resources);

  return result_TEST_PASSED;
}

#endif /* WUSS_FURNITURE && WUSS_ICONS */
