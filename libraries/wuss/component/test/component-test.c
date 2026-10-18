/* component/test/component-test.c -- wuss component tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "framebuf/palettes.h"
#include "io/filetype.h"
#include "io/path.h"
#include "wuss/menu.h"
#include "wuss/icon-spec.h"
#include "wuss/component/dialogue.h"
#include "wuss/component/fontmenu.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/saveas.h"
#include "wuss/gadget/colourset.h"
#include "test/tmpdir.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_COMPONENTS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

#ifdef WUSS_COMPONENTS
/* colourset changed callback: counts calls into the int at opaque */
static result_t colourset_test_changed(wuss_colourset_t *colourset,
                                       wuss_colour_t     colour,
                                       void             *opaque)
{
  NOT_USED(colourset);
  NOT_USED(colour);

  (*(int *) opaque)++;
  return result_OK;
}
#endif

/* wuss_saveas end-to-end test: a scratch directory plus a minimal fake
 * receiver task standing in for the real Filer (BUILD_APPS only), driving
 * the core DataSave protocol via wuss_saveas_handle_event. POSIX only, as
 * for the dirlist scratch-dir tests. */
#if defined(WUSS_COMPONENTS) && !defined(_WIN32) && !defined(__riscos)

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int make_scratch_dir(char *path, size_t cap)
{
  if (snprintf(path, cap, "%s/dptlib-saveas-XXXXXX", test_tmpdir()) >= (int) cap)
    return 0;

  return mkdtemp(path) != NULL;
}

static void saveas_test_remove_scratch_dir(const char *dir)
{
  char path[DPTLIB_MAXPATH];

  snprintf(path, sizeof(path), "%s/saved.txt", dir);
  remove(path);
  rmdir(dir);
}

static int saveas_test_file_exists(const char *path)
{
  struct stat st;

  return stat(path, &st) == 0;
}

/* save_fn passed to wuss_saveas_create: opaque is an int* counter, bumped
 * on every call (as colourset_test_changed does for its own callback) --
 * also writes the file so the test can confirm it landed. */
static result_t saveas_test_save(const char *path, void *opaque)
{
  FILE *fp;

  (*(int *) opaque)++;

  fp = fopen(path, "wb");
  if (fp == NULL)
    return result_BAD_ARG;
  fputs("saveas test\n", fp);
  fclose(fp);

  return result_OK;
}

typedef struct saveas_recv
{
  wuss_t        *wuss;
  wuss_task_t   *self;
  const char    *dir;
  int            suppress_ack;
}
saveas_recv_t;

/* fake Filer stand-in: on a DataSave, replies with a scratch-dir path; on
 * the DataLoad that follows, just acks -- the file's presence is checked by
 * the test itself, not by this task. */
static result_t saveas_recv_handle(wuss_window_t      *window,
                                   const wuss_event_t *event,
                                   void               *task_data)
{
  saveas_recv_t *tc;

  NOT_USED(window);

  tc = task_data;
  if (event->kind != wuss_EVENT_MESSAGE || tc->suppress_ack)
    return result_OK;

  if (event->data.message->action == wuss_MESSAGE_DATA_SAVE)
  {
    char                    path[DPTLIB_MAXPATH];
    const wuss_data_save_t *payload;

    payload = (const wuss_data_save_t *) event->data.message->data;
    snprintf(path, sizeof(path), "%s/saved.txt", tc->dir);
    (void) wuss_send(tc->wuss, tc->self, payload->reply_window,
                     wuss_MESSAGE_DATA_SAVE_ACK, path, strlen(path) + 1,
                     event->data.message->my_ref);
  }
  else if (event->data.message->action == wuss_MESSAGE_DATA_LOAD)
  {
    (void) wuss_acknowledge(tc->wuss, tc->self, event->data.message);
  }

  return result_OK;
}
#endif /* WUSS_COMPONENTS && !_WIN32 && !__riscos */

/* ----------------------------------------------------------------------- */

result_t wuss_component_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;
  int            rowbytes;

  rowbytes = 200 * 4;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: wuss_fontmenu lists the bmfonts dir and resolves a pick\n");
  {
    const char        *dir;
    const wuss_menu_t *fmm;
    wuss_event_t       ev;
    const char        *name;
    int                i;

    dir = pathf("%s/resources/bmfonts", resources);

    fmm = wuss_fontmenu_menu(dir, "Font", NULL);
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
    name = wuss_fontmenu_selected(&ev);
    if (name == NULL || strcmp(name, fmm->items[1].text) != 0)
      goto FontMenuFail;

    /* wrong event kind, foreign menu and out-of-range index all decline */
    ev.kind = wuss_EVENT_IDLE;
    if (wuss_fontmenu_selected(&ev) != NULL)             goto FontMenuFail;
    ev.kind                   = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu  = NULL;
    if (wuss_fontmenu_selected(&ev) != NULL)             goto FontMenuFail;
    ev.data.menu_select.menu  = fmm;
    ev.data.menu_select.index = fmm->nitems;
    if (wuss_fontmenu_selected(&ev) != NULL)             goto FontMenuFail;

    if (wuss_fontmenu_menu(NULL, "Font", NULL) != NULL)  goto FontMenuFail;

    /* a SYSTEM-class wuss font is dropped from the menu; NONE-class and
     * unnamed slots are not -- also exercises the singleton rebuilding when
     * handed a different wuss_t for the same dir */
    {
      const char      *sysfontfile;
      bmfont_t        *sysfont;
      wuss_font_desc_t sysdescs[2];
      wuss_t          *syswuss;

      sysfontfile = pathf("%s/resources/bmfonts/Symbols/Regular.png",
                          resources);
      rc = bmfont_create(sysfontfile, &sysfont);
      if (rc != result_OK)                               goto FontMenuFail;

      sysdescs[0].font       = sysfont;
      sysdescs[0].font_class = wuss_FONT_CLASS_SYSTEM;
      sysdescs[0].name       = "Symbols";
      sysdescs[1].font       = sysfont;
      sysdescs[1].font_class = wuss_FONT_CLASS_NONE;
      sysdescs[1].name       = "Tiny"; /* NONE class: not skipped though named */

      rc = wuss_create(&fixture.scr, sysdescs, 2, NULL, 0, NULL, NULL, NULL, &syswuss);
      if (rc != result_OK)
      {
        bmfont_destroy(sysfont);
        goto FontMenuFail;
      }

      /* dir points into pathf's shared scratch buffer, which sysfontfile's
       * pathf call above just overwrote -- re-derive it rather than reuse
       * the now-stale pointer */
      dir = pathf("%s/resources/bmfonts", resources);

      fmm = wuss_fontmenu_menu(dir, "Font", syswuss);
      wuss_destroy(syswuss);
      bmfont_destroy(sysfont);
      if (fmm == NULL)                                    goto FontMenuFail;
      for (i = 0; i < fmm->nitems; i++)
        if (strcmp(fmm->items[i].text, "Symbols Regular") == 0)
          goto FontMenuFail;
      for (i = 0; i < fmm->nitems; i++)
        if (strcmp(fmm->items[i].text, "Tiny Regular") == 0)
          break;
      if (i == fmm->nitems)                               goto FontMenuFail;
    }

    /* missing directory is surfaced by leaving the singleton unbuilt, not
     * swallowed */
    if (wuss_fontmenu_menu("no/such/dir/here", NULL, NULL) != NULL)
      goto FontMenuFail;

    rc = result_OK;
    goto FontMenuOK;

FontMenuFail:
    printf("wuss_test: fontmenu check failed\n");
    return result_TEST_FAILED;
FontMenuOK: ;
  }

  printf("test: wuss_colourmenu covers the palette and resolves a pick\n");
  {
    const wuss_menu_t *cmm;
    wuss_event_t       ev;
    wuss_colour_t      picked;
    int                ok;
    int                i;

    wuss_colourmenu_set_none(0);
    cmm = wuss_colourmenu_menu(wuss);
    if (cmm == NULL)                                     goto ColourMenuFail;
    rc = wuss_colourmenu_set_title("Colour");
    if (rc != result_OK)                                 goto ColourMenuFail;
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
    picked = wuss_colourmenu_selected(&ev, &ok);
    if (!ok || picked != (wuss_colour_t) 1)             goto ColourMenuFail;

    /* wrong event kind, foreign menu and out-of-range index all decline */
    ev.kind = wuss_EVENT_IDLE;
    if (wuss_colourmenu_selected(&ev, &ok) != 0 || ok)
      goto ColourMenuFail;
    ev.kind                   = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu  = NULL;
    if (wuss_colourmenu_selected(&ev, &ok) != 0 || ok)
      goto ColourMenuFail;
    ev.data.menu_select.menu  = cmm;
    ev.data.menu_select.index = cmm->nitems;
    if (wuss_colourmenu_selected(&ev, &ok) != 0 || ok)
      goto ColourMenuFail;

    if (wuss_colourmenu_menu(NULL) != NULL)              goto ColourMenuFail;

    /* with_none (the default) appends a dashed-off "None" row resolving to
     * wuss_NO_BACKGROUND */
    wuss_colourmenu_set_none(1);
    cmm = wuss_colourmenu_menu(wuss);
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
    picked = wuss_colourmenu_selected(&ev, &ok);
    if (!ok || picked != wuss_NO_BACKGROUND)             goto ColourMenuFail;

    /* set_ticked marks exactly one row (swatch and tick together); the RGB
     * form finds it by exact palette match; an unmatched value clears all */
    {
      const colour_t *pal;
      int             npal;
      int             t;

#define CM_TICKED(i) ((cmm->items[i].flags & wuss_MENU_ITEM_TICKED) != 0)

      pal = wuss_get_palette(wuss, &npal);
      if (pal == NULL || npal < 2)                       goto ColourMenuFail;

      wuss_colourmenu_set_ticked((wuss_colour_t) 1);
      for (t = 0; t < cmm->nitems; t++)
        if (CM_TICKED(t) != (t == 1))                    goto ColourMenuFail;

      wuss_colourmenu_set_ticked_rgb(pal[0]);
      for (t = 0; t < cmm->nitems; t++)
        if (CM_TICKED(t) != (t == 0))                    goto ColourMenuFail;

      wuss_colourmenu_set_ticked(wuss_NO_BACKGROUND);
      for (t = 0; t < cmm->nitems; t++)
        if (CM_TICKED(t) != (t == cmm->nitems - 1))      goto ColourMenuFail;

      wuss_colourmenu_set_ticked_rgb(colour_rgba(1, 2, 3, 4));
      for (t = 0; t < cmm->nitems; t++)
        if (CM_TICKED(t))                                goto ColourMenuFail;

#undef CM_TICKED
    }

    /* actually open and redraw it, so the "None" row's hatched chip gets
     * rasterised for real, not just checked as data -- needs its own
     * font-equipped wuss, unlike the shared fontless "wuss" above; also
     * exercises the singleton rebuilding when handed a different wuss_t */
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
      const wuss_menu_t *cmrealm;
      int                cmrowbytes;

      cmfontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
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

      wuss_colourmenu_set_none(1);
      cmrealm = wuss_colourmenu_menu(cmwuss);
      if (cmrealm == NULL) { free(cmpixels); goto ColourMenuFail; }

      rc = wuss_menu_open(cmowner, cmrealm, POINT(40, 40), NULL);
      if (rc != result_OK) goto ColourMenuFail;

      rc = wuss_redraw_dirty(cmwuss);
      if (rc != result_OK) goto ColourMenuFail;

      wuss_menu_close(wuss__menu_chain(cmwuss));
      wuss_destroy(cmwuss);
      free(cmpixels);
      bmfont_destroy(cmfont);
    }

    rc = result_OK;
    goto ColourMenuOK;

ColourMenuFail:
    printf("wuss_test: colourmenu check failed\n");
    return result_TEST_FAILED;
ColourMenuOK: ;
  }

  printf("test: wuss_colourlist names its rows and applies a pick to its target\n");
  {
    wuss_colourlist_t  pair;
    colour_t           fg, bg;
    wuss_event_t       ev;
    const wuss_menu_t *cpm;
    result_t           cprc;

    fg = colour_rgb(1, 2, 3);
    bg = colour_rgb(4, 5, 6);

    wuss_colourlist_init_pair(&pair, wuss, &fg, &bg);
    if (pair.menu.nitems != 2)                           goto ColourPairFail;
    if (strcmp(pair.items[0].text, "Foreground") != 0)   goto ColourPairFail;
    if (strcmp(pair.items[1].text, "Background") != 0)   goto ColourPairFail;
    if (pair.items[0].submenu == NULL)                   goto ColourPairFail;
    if (pair.target != NULL)                             goto ColourPairFail;

    /* a pick before any row has been hovered changes nothing */
    wuss_colourmenu_set_none(0);
    cpm = wuss_colourmenu_menu(wuss);
    if (cpm == NULL)                                     goto ColourPairFail;
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = cpm;
    ev.data.menu_select.index  = 1;
    ev.data.menu_select.button = wuss_BUTTON_SELECT;
    if (wuss_colourlist_selected(&pair, &ev))            goto ColourPairFail;

    /* a PRE_SUBMENU_OPEN for some other menu is declined */
    ev.kind                          = wuss_EVENT_PRE_SUBMENU_OPEN;
    ev.data.pre_submenu_open.handle  = NULL;
    ev.data.pre_submenu_open.index   = 0;
    cprc = result_OK;
    if (wuss_colourlist_pre_open(&pair, wuss, &ev, &cprc))
      goto ColourPairFail;

    /* once targeted, a pick lands in that field only */
    pair.target                = &bg;
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = cpm;
    ev.data.menu_select.index  = 1;
    if (!wuss_colourlist_selected(&pair, &ev))           goto ColourPairFail;
    if (bg.primary != wuss_get_palette(wuss, NULL)[1].primary)
      goto ColourPairFail;
    if (fg.primary != colour_rgb(1, 2, 3).primary)       goto ColourPairFail;

    /* a foreign menu is not a colour pick */
    ev.data.menu_select.menu = NULL;
    if (wuss_colourlist_selected(&pair, &ev))            goto ColourPairFail;

    /* N named rows: labels carried through, bad counts refused */
    {
      static const char *const names3[3] = { "Sphere", "Light", "Sky" };
      colour_t          c3[3], *f3[3];
      wuss_colourlist_t list3;
      int               k;

      for (k = 0; k < 3; k++)
        f3[k] = &c3[k];

      if (wuss_colourlist_init(&list3, wuss, 3, f3, names3) != result_OK)
        goto ColourPairFail;
      if (list3.menu.nitems != 3)                        goto ColourPairFail;
      if (strcmp(list3.items[2].text, "Sky") != 0)       goto ColourPairFail;
      if (list3.items[2].submenu == NULL)                goto ColourPairFail;

      if (wuss_colourlist_init(&list3, wuss, 3, f3, NULL) == result_OK)
        goto ColourPairFail; /* default names only cover two rows */
      if (wuss_colourlist_init(&list3, wuss, 0, f3, names3) == result_OK)
        goto ColourPairFail;
      if (wuss_colourlist_init(&list3, wuss, WUSS_COLOURLIST_MAX + 1, f3,
                               names3) == result_OK)
        goto ColourPairFail;
    }

    goto ColourPairOK;

ColourPairFail:
    printf("wuss_test: colourlist check failed\n");
    return result_TEST_FAILED;
ColourPairOK: ;
  }

  printf("test: wuss_colourset fills its field and tracks colour picks\n");
  {
    const char         *csfontfile;
    bmfont_t           *csfont;
    screen_t            csscr;
    bitmap_t            csbm;
    void               *cspixels;
    wuss_t             *cswuss;
    wuss_font_desc_t    csfdesc;
    test_task_t         cstc;
    wuss_task_t        *csowner;
    wuss_window_t      *cswin;
    wuss_colourset_t   *cs;
    wuss_icon_t        *button, *field;
    const unsigned int *fpx;
    wuss_event_t        ev;
    box_t               cscontent;
    result_t            csrc;
    int                 calls;

    /* the menu needs a font, and the button the icon set; the default
     * palette is just black (0) and white (1) */
    csfontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
    rc = bmfont_create(csfontfile, &csfont);
    if (rc != result_OK) goto ColourSetFail;

    cspixels = malloc((size_t) rowbytes * 200);
    if (cspixels == NULL) { rc = result_OOM; goto ColourSetFail; }
    rc = bitmap_init(&csbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, cspixels);
    if (rc != result_OK) goto ColourSetFail;
    screen_for_bitmap(&csscr, &csbm);

    csfdesc.font       = csfont;
    csfdesc.font_class = wuss_FONT_CLASS_NONE;
    csfdesc.name       = NULL;
    rc = wuss_create(&csscr, &csfdesc, 1, NULL, 0, NULL, NULL, resources,
                     &cswuss);
    if (rc != result_OK) goto ColourSetFail;

    memset(&cstc, 0, sizeof(cstc));
    csowner = mk_task(cswuss, test_handle, &cstc);
    if (csowner == NULL) { rc = result_OOM; goto ColourSetFail; }

    cscontent = (box_t) BOX_POS_SIZE(10, 10, 150, 100);
    rc = wuss_window_create(csowner, &cscontent, "CS", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, SIZE2D(150, 100), SIZE2D(0, 0),
                            &cswin);
    if (rc != result_OK) goto ColourSetFail;

    /* an out-of-range colour is refused */
    if (wuss_colourset_create(&cs, cswin, (box_t) BOX_POS_SIZE(4, 4, 120, 22),
                              "Fill", 0, wuss_NO_BACKGROUND, NULL, NULL) !=
        result_WUSS_BAD_COLOUR)
      goto ColourSetFail;

    calls = 0;
    rc = wuss_colourset_create(&cs, cswin, (box_t) BOX_POS_SIZE(4, 4, 120, 22),
                               "Fill", 40, 0, colourset_test_changed, &calls);
    if (rc != result_OK) goto ColourSetFail;
    if (wuss_colourset_get_colour(cs) != 0) goto ColourSetFail;

    /* the button sits at the right edge, drawn from the icon set; the field
     * just left of it is a bitmap filled with the colour */
    button = wuss__icon_hit_test(cswin, POINT(4 + 120 - 2, 4 + 11));
    if (button == NULL || wuss_icon_get_type(button) != wuss_ICON_TYPE_BITMAP)
      goto ColourSetFail;
    field = cswin->icons[0];
    if (field->spec.type != wuss_ICON_TYPE_BITMAP) goto ColourSetFail;
    fpx = field->spec.u.bitmap.image->base;
    if ((*fpx & 0xFFFFFF) != 0) goto ColourSetFail;
    if (field->spec.bbox.x0 != 4 + 40 + wuss_STD_GAP) /* the title width */
      goto ColourSetFail;
    /* the field ends where button_width says */
    if (field->spec.bbox.x1 != 4 + 120 - wuss_STD_GAP -
                               wuss_colourset_button_width(cswuss, 22))
      goto ColourSetFail;

    /* a colour menu pick while the gadget has not opened it is declined */
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = wuss_colourmenu_menu(cswuss);
    ev.data.menu_select.index  = 1;
    ev.data.menu_select.button = wuss_BUTTON_SELECT;
    if (wuss_colourset_handle_event(cs, &ev, &csrc)) goto ColourSetFail;

    /* a Select click on the button opens the colour menu */
    ev.kind              = wuss_EVENT_ICON;
    ev.data.icon.icon    = button;
    ev.data.icon.action  = wuss_MOUSE_UP;
    ev.data.icon.button  = wuss_BUTTON_SELECT;
    ev.data.icon.value   = 0;
    if (!wuss_colourset_handle_event(cs, &ev, &csrc) || csrc != result_OK)
      goto ColourSetFail;
    if (wuss__menu_chain(cswuss) == NULL) goto ColourSetFail;

    /* an Adjust pick of another colour refills the field and fires the
     * callback, keeping the menu open */
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = wuss_menu_handle_menu(wuss__menu_chain(cswuss));
    ev.data.menu_select.index  = 1;
    ev.data.menu_select.button = wuss_BUTTON_ADJUST;
    if (!wuss_colourset_handle_event(cs, &ev, &csrc) || csrc != result_OK)
      goto ColourSetFail;
    if (wuss_colourset_get_colour(cs) != 1 || (*fpx & 0xFFFFFF) != 0xFFFFFF ||
        calls != 1)
      goto ColourSetFail;

    /* re-picking the current colour does not fire it */
    if (!wuss_colourset_handle_event(cs, &ev, &csrc) || calls != 1)
      goto ColourSetFail;

    /* the programmatic path is range-checked and silent */
    if (wuss_colourset_set_colour(cs, wuss_NO_BACKGROUND) !=
        result_WUSS_BAD_COLOUR)
      goto ColourSetFail;
    if (wuss_colourset_set_colour(cs, 0) != result_OK) goto ColourSetFail;
    if (wuss_colourset_get_colour(cs) != 0 || calls != 1) goto ColourSetFail;

    /* an RGB shows exactly, not snapped to the palette, and its nearest
     * entry stands as the colour; picking that entry is still a change */
    wuss_colourset_set_rgb(cs, colour_rgb(0x12, 0x34, 0x56));
    if ((*fpx & 0xFFFFFF) != 0x123456 || wuss_colourset_get_colour(cs) != 0 ||
        calls != 1)
      goto ColourSetFail;
    ev.data.menu_select.index = 0;
    if (!wuss_colourset_handle_event(cs, &ev, &csrc) || csrc != result_OK ||
        (*fpx & 0xFFFFFF) != 0 || calls != 2)
      goto ColourSetFail;

    /* a Select pick: wuss closes the chain and delivers MENU_CLOSED (picked)
     * before MENU_SELECT, which is still the gadget's */
    wuss_menu_close(wuss__menu_chain(cswuss));
    ev.kind                    = wuss_EVENT_MENU_CLOSED;
    ev.data.menu_closed.picked = 1;
    if (wuss_colourset_handle_event(cs, &ev, &csrc)) goto ColourSetFail;
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = wuss_colourmenu_menu(cswuss);
    ev.data.menu_select.index  = 1;
    ev.data.menu_select.button = wuss_BUTTON_SELECT;
    if (!wuss_colourset_handle_event(cs, &ev, &csrc) || csrc != result_OK)
      goto ColourSetFail;
    if (wuss_colourset_get_colour(cs) != 1 || calls != 3) goto ColourSetFail;

    /* ... but once that pick is spent, a later one is not */
    if (wuss_colourset_handle_event(cs, &ev, &csrc)) goto ColourSetFail;

    /* an abandoned chain leaves no pick in flight either */
    ev.kind              = wuss_EVENT_ICON;
    ev.data.icon.icon    = button;
    ev.data.icon.action  = wuss_MOUSE_UP;
    ev.data.icon.button  = wuss_BUTTON_SELECT;
    ev.data.icon.value   = 0;
    if (!wuss_colourset_handle_event(cs, &ev, &csrc) || csrc != result_OK)
      goto ColourSetFail;
    wuss_menu_close(wuss__menu_chain(cswuss)); /* as a click outside does */
    ev.kind                    = wuss_EVENT_MENU_CLOSED;
    ev.data.menu_closed.picked = 0;
    if (wuss_colourset_handle_event(cs, &ev, &csrc)) goto ColourSetFail;
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = wuss_colourmenu_menu(cswuss);
    ev.data.menu_select.index  = 0;
    ev.data.menu_select.button = wuss_BUTTON_SELECT;
    if (wuss_colourset_handle_event(cs, &ev, &csrc)) goto ColourSetFail;

    /* destroying it closes the menu */
    ev.kind              = wuss_EVENT_ICON;
    ev.data.icon.icon    = button;
    ev.data.icon.action  = wuss_MOUSE_UP;
    ev.data.icon.button  = wuss_BUTTON_SELECT;
    ev.data.icon.value   = 0;
    if (!wuss_colourset_handle_event(cs, &ev, &csrc) || csrc != result_OK)
      goto ColourSetFail;
    wuss_colourset_destroy(cs);
    if (wuss__menu_chain(cswuss) != NULL) goto ColourSetFail;

    reap_test_tasks();
    wuss_destroy(cswuss);
    free(cspixels);
    bmfont_destroy(csfont);
    goto ColourSetOK;

ColourSetFail:
    printf("wuss_test: colourset check failed\n");
    return result_TEST_FAILED;
ColourSetOK: ;
  }

#if defined(WUSS_COMPONENTS) && !defined(_WIN32) && !defined(__riscos)
  printf("test: wuss_saveas drives the core DataSave protocol end to end\n");
  {
    static char      sa_scratch[64];

    const char      *safontfile;
    bmfont_t        *safont;
    screen_t         sascr;
    bitmap_t         sabm;
    void            *sapixels;
    wuss_t          *sawuss;
    wuss_font_desc_t safdesc;
    saveas_recv_t    sarecv_tc;
    wuss_task_t     *sarecv;
    wuss_window_t   *sawin, *sarecvwin;
    wuss_saveas_t   *sa;
    filetype_t       sa_ft;
    box_t            sarecvbox;
    wuss_event_t     ev;
    wuss_icon_t     *sa_leaf, *sa_cancel, *sa_save;
    char             sa_path[DPTLIB_MAXPATH];
    int              sa_save_calls;

    if (!make_scratch_dir(sa_scratch, sizeof(sa_scratch))) goto SaveAsFail;

    safontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
    rc = bmfont_create(safontfile, &safont);
    if (rc != result_OK) goto SaveAsFail;

    sapixels = malloc((size_t) rowbytes * 200);
    if (sapixels == NULL) { rc = result_OOM; goto SaveAsFail; }
    rc = bitmap_init(&sabm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, sapixels);
    if (rc != result_OK) goto SaveAsFail;
    screen_for_bitmap(&sascr, &sabm);

    safdesc.font       = safont;
    safdesc.font_class = wuss_FONT_CLASS_NONE;
    safdesc.name       = NULL;
    rc = wuss_create(&sascr, &safdesc, 1, NULL, 0, NULL, NULL, resources,
                     &sawuss);
    if (rc != result_OK) goto SaveAsFail;

    memset(&sarecv_tc, 0, sizeof(sarecv_tc));
    sarecv_tc.wuss = sawuss;
    sarecv_tc.dir  = sa_scratch;
    sarecv = mk_task(sawuss, saveas_recv_handle, &sarecv_tc);
    if (sarecv == NULL) { rc = result_OOM; goto SaveAsFail; }
    sarecv_tc.self = sarecv;

    sarecvbox = (box_t) BOX_POS_SIZE(10, 10, 100, 100);
    rc = wuss_window_create(sarecv, &sarecvbox, "Dir", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, SIZE2D(0, 0), SIZE2D(0, 0),
                            &sarecvwin);
    if (rc != result_OK) goto SaveAsFail;

    sa_ft.riscos = 0xfff;
    sa_ft.ext    = "txt";
    sa_save_calls = 0;
    rc = wuss_saveas_create(&sa, sawuss, &sa_ft, "Untitled",
                            saveas_test_save, &sa_save_calls);
    if (rc != result_OK) goto SaveAsFail;
    sawin = wuss_saveas_window(sa);

    sa_leaf   = sawin->icons[1];
    sa_cancel = sawin->icons[2];
    sa_save   = sawin->icons[3];

    /* Save with no full path yet does nothing (prompts, does not call
     * save_fn) */
    ev.kind             = wuss_EVENT_ICON;
    ev.data.icon.icon   = sa_save;
    ev.data.icon.action = wuss_MOUSE_UP;
    ev.data.icon.button = wuss_BUTTON_SELECT;
    ev.data.icon.value  = 0;
    if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail;
    if (sa_save_calls != 0) goto SaveAsFail;

    /* dragging the file icon onto the receiver's window starts the DataSave
     * protocol; the receiver picks a scratch path and acks it, saveas
     * writes the file and sends DataLoad, the receiver acks that too and
     * saveas hides itself */
    ev.kind                    = wuss_EVENT_DRAG_END;
    ev.data.drag_end.drop      = sarecvwin;
    ev.data.drag_end.point     = POINT(50, 50);
    ev.data.drag_end.cancelled = 0;
    ev.data.drag_end.button    = wuss_BUTTON_SELECT;
    if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail;

    rc = wuss_idle(sawuss);
    if (rc != result_OK) goto SaveAsFail;

    /* the DataLoadAck closes a SELECT drag-save */
    if (sa_save_calls != 1) goto SaveAsFail;
    if ((sawin->flags & wuss_WINDOW_HIDDEN) == 0) goto SaveAsFail;
    if (!path_is_full(wuss_saveas_get_path(sa))) goto SaveAsFail;
    strncpy(sa_path, wuss_saveas_get_path(sa), sizeof(sa_path) - 1);
    sa_path[sizeof(sa_path) - 1] = '\0';
    if (!saveas_test_file_exists(sa_path)) goto SaveAsFail;

    /* reveal the dialogue again and try the direct-save shortcut, now the
     * writable already holds a full path -- no drag needed */
    (void) wuss_window_set_hidden(sawin, 0);
    sa_save_calls = 0;
    ev.kind             = wuss_EVENT_ICON;
    ev.data.icon.icon   = sa_save;
    ev.data.icon.action = wuss_MOUSE_UP;
    ev.data.icon.button = wuss_BUTTON_SELECT;
    ev.data.icon.value  = 0;
    if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail;
    if (sa_save_calls != 1) goto SaveAsFail;

    /* Cancel hides the dialogue */
    (void) wuss_window_set_hidden(sawin, 0);
    ev.kind             = wuss_EVENT_ICON;
    ev.data.icon.icon   = sa_cancel;
    ev.data.icon.action = wuss_MOUSE_UP;
    ev.data.icon.button = wuss_BUTTON_SELECT;
    ev.data.icon.value  = 0;
    if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail;
    if ((sawin->flags & wuss_WINDOW_HIDDEN) == 0) goto SaveAsFail;

    /* an ADJUST drag-save completes but leaves the dialogue open */
    (void) wuss_window_set_hidden(sawin, 0);
    (void) wuss_icon_set_text(sawin, sa_leaf, "adjust");
    ev.kind                    = wuss_EVENT_DRAG_END;
    ev.data.drag_end.drop      = sarecvwin;
    ev.data.drag_end.point     = POINT(50, 50);
    ev.data.drag_end.cancelled = 0;
    ev.data.drag_end.button    = wuss_BUTTON_ADJUST;
    if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail;
    rc = wuss_idle(sawuss);
    if (rc != result_OK) goto SaveAsFail;
    if (sa_save_calls != 2) goto SaveAsFail;
    if ((sawin->flags & wuss_WINDOW_HIDDEN) != 0) goto SaveAsFail;
    ev.data.drag_end.button    = wuss_BUTTON_SELECT;

    /* a drag while a transfer is already running is ignored: start one (the
     * receiver held back from replying), then drag again before it does */
    (void) wuss_icon_set_text(sawin, sa_leaf, "second");
    sarecv_tc.suppress_ack     = 1;
    ev.kind                    = wuss_EVENT_DRAG_END;
    ev.data.drag_end.drop      = sarecvwin;
    ev.data.drag_end.point     = POINT(50, 50);
    ev.data.drag_end.cancelled = 0;
    if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail;
    if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail; /* 2nd, ignored */
    rc = wuss_idle(sawuss);
    if (rc != result_OK) goto SaveAsFail;
    sarecv_tc.suppress_ack = 0;

    /* an unacknowledged DataSave (dropped on a window whose task never
     * replies) bounces to the saver and is absorbed silently, no crash */
    {
      wuss_task_t   *silent;
      wuss_window_t *silentwin;
      box_t          silentbox;

      silent = mk_task(sawuss, NULL, NULL);
      if (silent == NULL) { rc = result_OOM; goto SaveAsFail; }

      silentbox = (box_t) BOX_POS_SIZE(10, 120, 100, 60);
      rc = wuss_window_create(silent, &silentbox, "Silent",
                              wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                              SIZE2D(0, 0), SIZE2D(0, 0), &silentwin);
      if (rc != result_OK) goto SaveAsFail;

      (void) wuss_icon_set_text(sawin, sa_leaf, "third");
      ev.kind                    = wuss_EVENT_DRAG_END;
      ev.data.drag_end.drop      = silentwin;
      ev.data.drag_end.point     = POINT(50, 50);
      ev.data.drag_end.cancelled = 0;
      if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail;

      rc = wuss_idle(sawuss);
      if (rc != result_OK) goto SaveAsFail;

      /* the transfer was abandoned silently: a fresh drag is accepted, not
       * ignored as "already running" */
      (void) wuss_icon_set_text(sawin, sa_leaf, "fourth");
      ev.data.drag_end.drop = sarecvwin;
      if (!wuss_saveas_handle_event(sa, sawin, &ev)) goto SaveAsFail;
      rc = wuss_idle(sawuss);
      if (rc != result_OK) goto SaveAsFail;
      if (sa_save_calls != 3) goto SaveAsFail;
    }

    wuss_saveas_destroy(sa);
    reap_test_tasks();
    wuss_destroy(sawuss);
    free(sapixels);
    bmfont_destroy(safont);
    saveas_test_remove_scratch_dir(sa_scratch);
    goto SaveAsOK;

SaveAsFail:
    printf("wuss_test: saveas check failed\n");
    return result_TEST_FAILED;
SaveAsOK: ;
  }
#endif /* WUSS_COMPONENTS && !_WIN32 && !__riscos */

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_component_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_component_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
