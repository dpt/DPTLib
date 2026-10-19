/* icon/test/iconpool-test.c -- wuss icon pool and icon drawing tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "geom/box.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "wuss/icon-spec.h"
#include "wuss/icon.h"
#include "../../icon.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */

result_t wuss_iconpool_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: wuss_iconpool_load scans resources/wuss/iconpool and compresses\n");

  {
    const bitmap_t *icon_bm;
    int             idx, opton, deep, count;

    rc = wuss_iconpool_load_resource(wuss, resources);
    if (rc != result_OK)
      goto Failure;

    /* resources/wuss/iconpool/ may carry more than these four fixtures
     * (optoff/opton/radoff/radon), so just require they're all present. */
    count = wuss_iconpool_count(wuss);
    if (count < 4)
      goto Failure;

    opton = wuss_iconpool_lookup(wuss, "opton");
    if (opton < 0 || wuss_iconpool_lookup(wuss, "radoff") < 0)
      goto Failure;
    if (wuss_iconpool_lookup(wuss, "nonesuch") != -1)
      goto Failure;

    /* only 32bpp icons get RLE-compressed: opton is plain grey (stays as
     * loaded), radoff is grey+alpha (expanded to 32bpp) */
    if (wuss_iconpool_bitmap(wuss, opton) == NULL)
      goto Failure;
    deep    = wuss_iconpool_lookup(wuss, "radoff");
    icon_bm = wuss_iconpool_bitmap(wuss, deep);
    if (icon_bm == NULL || !bitmap_is_compressed(icon_bm))
      goto Failure;
    if (wuss_iconpool_bitmap(wuss, count) != NULL)
      goto Failure;

    /* entries are sorted by name, whatever order the directory scan gave */
    if (!(wuss_iconpool_lookup(wuss, "apps") <
          wuss_iconpool_lookup(wuss, "gright") &&
          wuss_iconpool_lookup(wuss, "gright") <
          wuss_iconpool_lookup(wuss, "optoff") &&
          wuss_iconpool_lookup(wuss, "optoff") < opton &&
          opton < wuss_iconpool_lookup(wuss, "radoff") &&
          wuss_iconpool_lookup(wuss, "radoff") <
          wuss_iconpool_lookup(wuss, "radon")))
      goto Failure;

    /* an icon spec picks it up by index via wuss_ICONPOOL_SET */
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
      spec.u.bitmap.set = wuss_ICONPOOL_SET(opton);
      rc = wuss_icon_create(win_ic, &spec, &icon);
      if (rc != result_OK)
        goto Failure;

      /* a pressed image resolves the same way */
      spec.u.bitmap.pressed_set =
        wuss_ICONPOOL_SET(wuss_iconpool_lookup(wuss, "radon"));
      rc = wuss_icon_create(win_ic, &spec, &icon);
      if (rc != result_OK)
        goto Failure;

      spec.u.bitmap.pressed_set = wuss_ICONPOOL_SET(99);
      if (wuss_icon_create(win_ic, &spec, &icon) != result_WUSS_BAD_INDEX)
        goto Failure;
      spec.u.bitmap.pressed_set = 0;

      /* a bogus index is rejected */
      spec.u.bitmap.set = wuss_ICONPOOL_SET(99);
      if (wuss_icon_create(win_ic, &spec, &icon) != result_WUSS_BAD_INDEX)
        goto Failure;

      /* or by name, resolved to the pool bitmaps at creation; the borrowed
       * names are not kept */
      memset(&spec, 0, sizeof(spec));
      spec.bbox                   = (box_t) BOX_POS_SIZE(0, 0, 16, 16);
      spec.type                   = wuss_ICON_TYPE_BITMAP;
      spec.u.bitmap.name          = "opton";
      spec.u.bitmap.pressed_name  = "radon";
      rc = wuss_icon_create(win_ic, &spec, &icon);
      if (rc != result_OK)
        goto Failure;
      if (icon->spec.u.bitmap.image != wuss_iconpool_bitmap(wuss, opton) ||
          icon->spec.u.bitmap.pressed_image !=
            wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss, "radon")) ||
          icon->spec.u.bitmap.name != NULL ||
          icon->spec.u.bitmap.pressed_name != NULL)
        goto Failure;

      /* an explicit index wins over a name */
      spec.u.bitmap.set = wuss_ICONPOOL_SET(wuss_iconpool_lookup(wuss, "radon"));
      rc = wuss_icon_create(win_ic, &spec, &icon);
      if (rc != result_OK ||
          icon->spec.u.bitmap.image !=
            wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss, "radon")))
        goto Failure;
      spec.u.bitmap.set = 0;

      /* an unknown name warns but still creates (the icon draws the
       * placeholder box); likewise an unknown pressed name */
      spec.u.bitmap.name = "nonesuch";
      rc = wuss_icon_create(win_ic, &spec, &icon);
      if (rc != result_OK || icon->spec.u.bitmap.image != NULL)
        goto Failure;
      spec.u.bitmap.name         = "opton";
      spec.u.bitmap.pressed_name = "nonesuch";
      rc = wuss_icon_create(win_ic, &spec, &icon);
      if (rc != result_OK || icon->spec.u.bitmap.pressed_image != NULL)
        goto Failure;
      rc = result_OK;

      wuss_window_close(win_ic);
    }

    /* entries know their names; out-of-range indices have none */
    if (wuss_iconpool_name(wuss, opton) == NULL ||
        strcmp(wuss_iconpool_name(wuss, opton), "opton") != 0 ||
        wuss_iconpool_name(wuss, -1) != NULL ||
        wuss_iconpool_name(wuss, count) != NULL)
      goto Failure;

    /* NULL and overlong directories are refused */
    {
      static char longdir[300];

      memset(longdir, 'x', sizeof(longdir) - 1);
      if (wuss_iconpool_load(wuss, NULL) != result_NULL_ARG ||
          wuss_iconpool_load_resource(wuss, NULL) != result_NULL_ARG ||
          wuss_iconpool_load(wuss, longdir) != result_BAD_ARG)
        goto Failure;
    }

    /* a reload replaces the set cleanly (no leak, ASan would catch it) */
    rc = wuss_iconpool_load_resource(wuss, resources);
    if (rc != result_OK || wuss_iconpool_count(wuss) != count)
      goto Failure;

    idx = wuss_iconpool_lookup(wuss, "optoff");
    if (idx < 0)
      goto Failure;

    /* a depth change via wuss_resize re-orders the set in place: rgb order
     * for a paletted screen, back to the screen's own bgr order for 32bpp,
     * with the bitmap_t addresses icons hold unchanged */
    {
      unsigned char p4pixels[100 * 200];
      bitmap_t      p4bm;
      screen_t      p4scr;
      pixelfmt_t    fmt32;

      icon_bm = wuss_iconpool_bitmap(wuss, deep);
      fmt32   = pixelfmt_base(icon_bm->format);
      if (fmt32 != pixelfmt_bgra8888 && fmt32 != pixelfmt_bgrx8888)
        goto Failure;

      rc = bitmap_init(&p4bm, SIZE2D(200, 200), pixelfmt_p4, 100, NULL,
                       p4pixels);
      if (rc != result_OK)
        goto Failure;
      screen_for_bitmap(&p4scr, &p4bm);

      rc = wuss_resize(wuss, &p4scr);
      if (rc != result_OK)
        goto Failure;
      if (wuss_iconpool_bitmap(wuss, deep) != icon_bm ||
          !bitmap_is_compressed(icon_bm) ||
          (pixelfmt_base(icon_bm->format) != pixelfmt_rgba8888 &&
           pixelfmt_base(icon_bm->format) != pixelfmt_rgbx8888))
        goto Failure;

      rc = wuss_resize(wuss, &fixture.scr);
      if (rc != result_OK)
        goto Failure;
      if (wuss_iconpool_bitmap(wuss, deep) != icon_bm ||
          pixelfmt_base(icon_bm->format) != fmt32)
        goto Failure;
    }
  }

  printf("test: wuss_iconpool_load keeps paletted icons uncompressed, compresses deep ones and fails the set on a bad PNG\n");

  {
    int idx;

    rc = wuss_iconpool_load(wuss, pathf("%s/resources/test/wuss/icons",
                                     resources));
    if (rc != result_OK)
      goto Failure;
    if (wuss_iconpool_count(wuss) != 4)
      goto Failure;

    /* paletted PNGs can't be RLE-compressed, and stay as loaded: they must
     * not abort the load or be dropped */
    idx = wuss_iconpool_lookup(wuss, "pal2");
    if (idx < 0 || wuss_iconpool_bitmap(wuss, idx)->format != pixelfmt_p2)
      goto Failure;
    idx = wuss_iconpool_lookup(wuss, "grey8");
    if (idx < 0 || wuss_iconpool_bitmap(wuss, idx)->format != pixelfmt_p8)
      goto Failure;

    /* a grey+alpha PNG is expanded to 32bpp and so compresses like RGBA */
    idx = wuss_iconpool_lookup(wuss, "greyalpha8");
    if (idx < 0 || !bitmap_is_compressed(wuss_iconpool_bitmap(wuss, idx)))
      goto Failure;
    idx = wuss_iconpool_lookup(wuss, "rgba");
    if (idx < 0 || !bitmap_is_compressed(wuss_iconpool_bitmap(wuss, idx)))
      goto Failure;

    /* a file that isn't a PNG fails the whole load */
    if (wuss_iconpool_load(wuss, pathf("%s/resources/test/wuss/badicons",
                                    resources)) == result_OK)
      goto Failure;

    /* put the standard set back for the tests that follow */
    rc = wuss_iconpool_load_resource(wuss, resources);
    if (rc != result_OK)
      goto Failure;
  }

  printf("test: a task's private icon pool is searched before the shared one, falls back to it, and is freed with the task\n");

  {
    static test_task_t tc_pp;
    wuss_task_t       *delegate_pp;
    wuss_window_t     *win_pp;
    wuss_icon_t       *icon;
    wuss_icon_spec_t   spec;
    box_t              box_pp;
    const char        *dir_pp;
    const bitmap_t    *priv;

    dir_pp = pathf("%s/resources/test/wuss/icons", resources);

    memset(&tc_pp, 0, sizeof(tc_pp));
    delegate_pp = mk_task(wuss, test_handle, &tc_pp);
    if (delegate_pp == NULL) goto Failure;

    /* no private pool yet: only the shared pool answers */
    if (wuss_task_iconpool_find(delegate_pp, "rgba") != NULL ||
        wuss_task_iconpool_find(delegate_pp, "opton") !=
          wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss, "opton")))
      goto Failure;

    rc = wuss_task_iconpool_load(delegate_pp, dir_pp);
    if (rc != result_OK)
      goto Failure;

    /* a private-only name is invisible to the shared pool */
    priv = wuss_task_iconpool_find(delegate_pp, "rgba");
    if (priv == NULL || wuss_iconpool_lookup(wuss, "rgba") != -1)
      goto Failure;

    /* a name the private pool lacks falls back to the shared one */
    if (wuss_task_iconpool_find(delegate_pp, "opton") !=
          wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss, "opton")) ||
        wuss_task_iconpool_find(delegate_pp, "nonesuch") != NULL)
      goto Failure;

    /* icons in the task's windows resolve names privately first */
    box_pp = (box_t) BOX_POS_SIZE(5, 5, 120, 100);
    rc = wuss_window_create(delegate_pp, &box_pp, "PP", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_pp);
    if (rc != result_OK)
      goto Failure;

    memset(&spec, 0, sizeof(spec));
    spec.bbox          = (box_t) BOX_POS_SIZE(0, 0, 16, 16);
    spec.type          = wuss_ICON_TYPE_BITMAP;
    spec.u.bitmap.name = "rgba";
    rc = wuss_icon_create(win_pp, &spec, &icon);
    if (rc != result_OK || icon->spec.u.bitmap.image != priv)
      goto Failure;

    spec.u.bitmap.name = "opton";
    rc = wuss_icon_create(win_pp, &spec, &icon);
    if (rc != result_OK ||
        icon->spec.u.bitmap.image != wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss, "opton")))
      goto Failure;

    /* another task's window can't see the private name: placeholder box */
    {
      static test_task_t tc_other;
      wuss_task_t       *delegate_other;
      wuss_window_t     *win_other;

      memset(&tc_other, 0, sizeof(tc_other));
      delegate_other = mk_task(wuss, test_handle, &tc_other);
      if (delegate_other == NULL) goto Failure;

      rc = wuss_window_create(delegate_other, &box_pp, "OT",
                              wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                              SIZE2D(0, 0), SIZE2D(0, 0), &win_other);
      if (rc != result_OK)
        goto Failure;

      spec.u.bitmap.name = "rgba";
      rc = wuss_icon_create(win_other, &spec, &icon);
      if (rc != result_OK || icon->spec.u.bitmap.image != NULL)
        goto Failure;
    }

    /* a private name shadows the same name in the shared pool */
    rc = wuss_iconpool_load(wuss, dir_pp);
    if (rc != result_OK)
      goto Failure;
    if (wuss_task_iconpool_find(delegate_pp, "rgba") != priv ||
        priv == wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss, "rgba")))
      goto Failure;

    /* a bad directory fails the load and leaves the pool empty, as for the
     * shared pool; the task falls back to the shared one */
    if (wuss_task_iconpool_load(delegate_pp,
                                pathf("%s/resources/test/wuss/badicons",
                                      resources)) == result_OK ||
        wuss_task_iconpool_find(delegate_pp, "rgba") !=
          wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss, "rgba")))
      goto Failure;

    /* a task's resource pool lives under wuss-tasks/<task name>/iconpool;
     * a task with no name or one too long for the path is refused */
    {
      static test_task_t tc_rs;
      static char        longname[300];
      wuss_task_desc_t   desc;
      wuss_task_t       *task_rs;

      if (wuss_task_iconpool_load(delegate_pp, NULL) != result_NULL_ARG ||
          wuss_task_iconpool_load_resource(delegate_pp, NULL) !=
            result_NULL_ARG)
        goto Failure;

      memset(&tc_rs, 0, sizeof(tc_rs));
      desc.handle    = test_handle;
      desc.task_data = &tc_rs;
      desc.name      = "minesweeping";
      rc = wuss_task_create(wuss, &desc, &task_rs);
      if (rc != result_OK)
        goto Failure;
      rc = wuss_task_iconpool_load_resource(task_rs, resources);
      if (rc != result_OK || task_rs->iconpool.nbitmaps == 0)
        goto Failure;

      task_rs->name = NULL;
      if (wuss_task_iconpool_load_resource(task_rs, resources) !=
          result_BAD_ARG)
        goto Failure;

      memset(longname, 'x', sizeof(longname) - 1);
      task_rs->name = longname;
      if (wuss_task_iconpool_load_resource(task_rs, resources) !=
          result_BAD_ARG)
        goto Failure;

      wuss_task_destroy(task_rs);
    }

    /* the standard shared set goes back for the tests that follow; the
     * private pool is freed with its task (ASan would catch a leak) */
    rc = wuss_iconpool_load_resource(wuss, resources);
    if (rc != result_OK)
      goto Failure;
    reap_test_tasks();
  }

  printf("test: a BITMAP or DRAGGABLE icon that can't be drawn leaves a red box, one that can doesn't\n");

  {
    static test_task_t         tc_rb;
    static pixelfmt_rgba8888_t rgba_px[10 * 10];
    static pixelfmt_bgrx8888_t bgrx_px[10 * 10];

    wuss_task_t               *delegate_rb;
    wuss_window_t             *win_rb;
    wuss_icon_t               *icon;
    wuss_icon_spec_t           spec;
    box_t                      box_rb;
    box_t                      content_rb;
    bitmap_t                   rle_bm, plain_bm;
    void                      *rle_blob;
    pixelfmt_any_t             red;
    int                        cx, cy, i;
    unsigned int               px[3];

    memset(&tc_rb, 0, sizeof(tc_rb));
    delegate_rb = mk_task(wuss, test_handle, &tc_rb);
    if (delegate_rb == NULL) goto Failure;

    box_rb = (box_t) BOX_POS_SIZE(20, 20, 100, 100);
    rc = wuss_window_create(delegate_rb, &box_rb, "RB", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_rb);
    if (rc != result_OK)
      goto Failure;

    /* an rgba8888 RLE bitmap can't be blitted to this bgrx8888 screen */
    for (i = 0; i < 10 * 10; i++)
      rgba_px[i] = colour_rgb(0, 0, 255).primary;
    rle_blob = malloc(sizeof(rgba_px));
    if (rle_blob == NULL) goto Failure;
    memcpy(rle_blob, rgba_px, sizeof(rgba_px));
    rc = bitmap_init(&rle_bm, SIZE2D(10, 10), pixelfmt_rgba8888,
                     10 * (int) sizeof(rgba_px[0]), NULL, rle_blob);
    if (rc != result_OK) { free(rle_blob); goto Failure; }
    rc = bitmap_compress(&rle_bm);
    if (rc != result_OK) { free(rle_blob); goto Failure; }

    /* a plain opaque blue bitmap in the screen's own format draws fine */
    rc = bitmap_init(&plain_bm, SIZE2D(10, 10), pixelfmt_bgrx8888,
                     10 * (int) sizeof(bgrx_px[0]), NULL, bgrx_px);
    if (rc != result_OK) { free(rle_bm.base); goto Failure; }
    bitmap_clear(&plain_bm, colour_rgb(0, 0, 255));

    memset(&spec, 0, sizeof(spec));
    spec.type = wuss_ICON_TYPE_BITMAP;

    spec.bbox = (box_t) BOX_POS_SIZE(2, 2, 10, 10); /* undrawable */
    spec.u.bitmap.image = &rle_bm;
    rc = wuss_icon_create(win_rb, &spec, &icon);
    if (rc != result_OK) { free(rle_bm.base); goto Failure; }

    spec.bbox = (box_t) BOX_POS_SIZE(20, 2, 10, 10); /* drawable */
    spec.u.bitmap.image = &plain_bm;
    rc = wuss_icon_create(win_rb, &spec, &icon);
    if (rc != result_OK) { free(rle_bm.base); goto Failure; }

    spec.bbox = (box_t) BOX_POS_SIZE(38, 2, 10, 10); /* a blank draggable */
    spec.type = wuss_ICON_TYPE_DRAGGABLE;
    spec.u.bitmap.image = NULL;
    rc = wuss_icon_create(win_rb, &spec, &icon);
    if (rc != result_OK) { free(rle_bm.base); goto Failure; }

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) { free(rle_bm.base); goto Failure; }

    red = colour_to_pixel(NULL, 0, colour_rgb(255, 0, 0), pixelfmt_bgrx8888);
    wuss_window_get_content_bounds(win_rb, &content_rb);
    cy = content_rb.y0 + 2 + 5;
    cx = content_rb.x0 + 2 + 5;
    px[0] = ((const uint32_t *) fixture.pixels)[cy * 200 + cx];
    px[1] = ((const uint32_t *) fixture.pixels)[cy * 200 + cx + 18];
    px[2] = ((const uint32_t *) fixture.pixels)[cy * 200 + cx + 36];

    if ((px[0] & 0xFFFFFF) != (red & 0xFFFFFF)) /* failed blit: red */
      { free(rle_bm.base); goto Failure; }
    if ((px[1] & 0xFFFFFF) == (red & 0xFFFFFF)) /* good blit: not red */
      { free(rle_bm.base); goto Failure; }
    if ((px[2] & 0xFFFFFF) != (red & 0xFFFFFF)) /* no bitmap at all: red */
      { free(rle_bm.base); goto Failure; }

    free(rle_bm.base);
    wuss_window_close(win_rb);
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_iconpool_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_iconpool_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
