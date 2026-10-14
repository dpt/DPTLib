/* wuss/test/menu-test.c -- wuss menu session tests */

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
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/message.h"
#include "wuss/menu.h"

#if defined(WUSS_MENUS) && defined(WUSS_ICONS)

/* white-box for now: picks are driven through the icon layer and the open
 * chain is read back from struct wuss__menu / struct wuss_icon */
#include "../../core/impl.h"
#include "wuss/icon-spec.h"
#include "../../icon.h"

#include "test/all-tests.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

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

/* ----------------------------------------------------------------------- */

result_t wuss_menu_test(const char *resources)
{
  result_t rc;
  int      rowbytes;

  /* another test may have left registry entries for tasks it already
   * destroyed with its wuss */
  forget_test_tasks();

  rowbytes = 200 * 4;

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
    int            i;
    wuss_window_t *fcover;

    /* a menu needs a font for its row metrics; the core wuss above was made
     * without one */
    fontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
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

    chain = fwuss->menu.chain;
    if (chain == NULL || chain->menu != &flash_menu) goto FlashCheckFail;

    flash_pick_row(fwuss, chain, 1, wuss_BUTTON_SELECT);

    if (ftc.menu_select_count != 0) goto FlashCheckFail; /* deferred, not yet */
    if (chain->flash.frames <= 0)   goto FlashCheckFail; /* flash was armed */

    for (i = 0; i < 64; i++) /* longer than any plausible flash */
      wuss_idle(fwuss);

    if (ftc.menu_select_count != 1)  goto FlashCheckFail;
    if (ftc.last_menu_index != 1)    goto FlashCheckFail;
    if (fwuss->menu.chain != NULL)   goto FlashCheckFail; /* SELECT tore it down */

    /* --- fast ADJUST re-picks: row 0, then row 2 mid-flash --- */
    rc = wuss_menu_open(fowner, &flash_menu, POINT(40, 40), NULL);
    if (rc != result_OK) goto FlashDestroy;
    chain = fwuss->menu.chain;
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
    if (fwuss->menu.chain == NULL)  goto FlashCheckFail;
    for (i = 0; i < flash_menu.nitems; i++)
      if (wuss__icon_hovered(fwuss->menu.chain->icons[i]) != (i == 2))
        goto FlashCheckFail;

    /* --- the chain lives in the top stack, so a window opened meanwhile
     * (clear of the menu, so the pick still lands on the row) is behind it
     * from the start and stays there after an ADJUST pick --- */
    rc = wuss_window_create(fowner, &(box_t) BOX_POS_SIZE(150, 150, 40, 40),
                            "cover", wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(40, 40), SIZE2D(40, 40), &fcover);
    if (rc != result_OK) goto FlashDestroy;
    if (wuss__z_first(fwuss) != chain->window)
      goto FlashCheckFail; /* new window starts behind the menu */

    flash_pick_row(fwuss, chain, 1, wuss_BUTTON_ADJUST);
    for (i = 0; i < 64; i++)
      wuss_idle(fwuss);

    if (ftc.menu_select_count != 3) goto FlashCheckFail;
    if (wuss__z_first(fwuss) != chain->window)
      goto FlashCheckFail;

    rc = result_OK;
    goto FlashDestroy;

FlashCheckFail:
    printf("wuss_test: menu flash check failed "
           "(select_count=%d last_index=%d chain=%p)\n",
           ftc.menu_select_count, ftc.last_menu_index,
           (void *) fwuss->menu.chain);
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

    fontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
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

    root = swuss->menu.chain;
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

    fontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
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

    proot = pwuss->menu.chain;
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

    fontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
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

    proot = pwuss->menu.chain;
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

  printf("test: a menu item's shortcut widens the menu by at least the "
         "shortcut's width\n");
  {
    static wuss_menu_item_t sc_items[1];
    static const wuss_menu_t sc_menu =
    {
      "Root", sc_items, NELEMS(sc_items)
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
    int              plain_w;
    bmfont_width_t   shortcut_w;
    struct wuss__menu *sroot;

    fontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
    rc = bmfont_create(fontfile, &font);
    if (rc != result_OK)
    {
      printf("wuss_test: shortcut test could not load %s\n", fontfile);
      goto Failure;
    }

    spixels = malloc((size_t) rowbytes * 200);
    if (spixels == NULL) { rc = result_OOM; goto ScFail; }
    rc = bitmap_init(&sbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, spixels);
    if (rc != result_OK) goto ScFailFree;
    screen_for_bitmap(&sscr, &sbm);

    fdesc.font       = font;
    fdesc.font_class = wuss_FONT_CLASS_NONE;
    fdesc.name       = NULL;
    rc = wuss_create(&sscr, &fdesc, 1, NULL, 0, NULL, NULL, NULL, &swuss);
    if (rc != result_OK) goto ScFailFree;

    memset(&stc, 0, sizeof(stc));
    sowner = mk_task(swuss, test_handle, &stc);
    if (sowner == NULL) { rc = result_OOM; goto ScDestroy; }

    WUSS_MENU_ITEM(sc_items, 0, "Item", wuss_MENU_ITEM_NONE);

    rc = wuss_menu_open(sowner, &sc_menu, POINT(10, 20), NULL);
    if (rc != result_OK) goto ScDestroy;
    plain_w = swuss->menu.chain->window->doc.w;
    wuss_menu_close(swuss->menu.chain);

    /* one font only, so the "bold" shortcut measures in the system font */
    sc_items[0].shortcut = "WWW";
    shortcut_w = 0;
    wuss__text_measure(font, "WWW", 3, INT_MAX, NULL, &shortcut_w);

    rc = wuss_menu_open(sowner, &sc_menu, POINT(10, 20), NULL);
    if (rc != result_OK) goto ScDestroy;
    sroot = swuss->menu.chain;
    if (sroot->window->doc.w < plain_w + (int) shortcut_w)
    {
      printf("wuss_test: shortcut menu width %d, want >= %d + %d\n",
             sroot->window->doc.w, plain_w, (int) shortcut_w);
      rc = result_TEST_FAILED;
    }
    wuss_menu_close(sroot);
    if (rc != result_OK) goto ScDestroy;

    /* dispatch: labels match ignoring case, '^' needs Ctrl, a shift prefix
     * needs Shift (which SPACE and Fn otherwise forbid), Alt never matches,
     * a disabled row is skipped */
    {
      static const struct
      {
        const char            *label;
        int                    code;
        wuss_key_modifiers_t   modifiers;
        wuss_menu_item_flags_t flags;
        int                    want;
      }
      cases[] =
      {
        { "S",     's',             wuss_KEY_MOD_NONE,  wuss_MENU_ITEM_NONE,     1 },
        { "S",     'S',             wuss_KEY_MOD_SHIFT, wuss_MENU_ITEM_NONE,     1 },
        { "S",     's',             wuss_KEY_MOD_CTRL,  wuss_MENU_ITEM_NONE,     0 },
        { "^S",    's',             wuss_KEY_MOD_CTRL,  wuss_MENU_ITEM_NONE,     1 },
        { "^S",    's',             wuss_KEY_MOD_NONE,  wuss_MENU_ITEM_NONE,     0 },
        { "S",     's',             wuss_KEY_MOD_ALT,   wuss_MENU_ITEM_NONE,     0 },
        { "SPACE", ' ',             wuss_KEY_MOD_NONE,  wuss_MENU_ITEM_NONE,     1 },
        { "F3",    wuss_KEY_F1 + 2, wuss_KEY_MOD_NONE,  wuss_MENU_ITEM_NONE,     1 },
        { "F3",    'F',             wuss_KEY_MOD_NONE,  wuss_MENU_ITEM_NONE,     0 },
        { "S",     's',             wuss_KEY_MOD_NONE,  wuss_MENU_ITEM_DISABLED, 0 },
        { "F3",    wuss_KEY_F1 + 2, wuss_KEY_MOD_SHIFT, wuss_MENU_ITEM_NONE,     0 },
        { "SPACE", ' ',             wuss_KEY_MOD_SHIFT, wuss_MENU_ITEM_NONE,     0 },
        { WUSS_MENU_SHIFT "F1",
                   wuss_KEY_F1,     wuss_KEY_MOD_SHIFT, wuss_MENU_ITEM_NONE,     1 },
        { WUSS_MENU_SHIFT "F1",
                   wuss_KEY_F1,     wuss_KEY_MOD_NONE,  wuss_MENU_ITEM_NONE,     0 },
        { "^" WUSS_MENU_SHIFT "F1",
                   wuss_KEY_F1,     wuss_KEY_MOD_CTRL | wuss_KEY_MOD_SHIFT,
                                                        wuss_MENU_ITEM_NONE,     1 },
      };

      result_t     drc;
      wuss_event_t key;
      int          i;

      key.kind = wuss_EVENT_KEY;
      for (i = 0; i < (int) NELEMS(cases); i++)
      {
        sc_items[0].shortcut   = cases[i].label;
        sc_items[0].flags      = cases[i].flags;
        key.data.key.code      = cases[i].code;
        key.data.key.modifiers = cases[i].modifiers;
        stc.menu_select_count  = 0;
        stc.last_menu_index    = -1;

        drc = wuss_menu_dispatch_shortcut(sowner, &sc_menu, &key);
        if ((drc == result_OK) != cases[i].want ||
            stc.menu_select_count != cases[i].want ||
            (cases[i].want && stc.last_menu_index != 0))
        {
          printf("wuss_test: dispatch case %d (\"%s\") rc %x selects %d, "
                 "want %d\n", i, cases[i].label, (unsigned) drc,
                 stc.menu_select_count, cases[i].want);
          rc = result_TEST_FAILED;
        }
      }
    }

ScDestroy:
    reap_test_tasks();
    wuss_destroy(swuss);
ScFailFree:
    free(spixels);
ScFail:
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

    fontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
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
    if (mwuss->menu.chain == NULL ||
        mwuss->menu.chain->menu != &menu_a)   goto MoveCheckFail;

    /* MENU over B while menu A is still open: the old chain must close AND
     * task B must get the press so menu B opens in its place. Regression:
     * before the mouse-click fix the press was spent closing menu A and B
     * never heard it. */
    wuss_mouse_click(mwuss, POINT(160, 160), wuss_BUTTON_MENU,
                     wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(mwuss, POINT(160, 160), wuss_BUTTON_MENU,
                     wuss_MOUSE_UP, NULL);

    if (mtb.menu_press_count != 1)            goto MoveCheckFail;
    if (mwuss->menu.chain == NULL ||
        mwuss->menu.chain->menu != &menu_b)   goto MoveCheckFail;

    /* replacing menu A's chain told task A its handle was gone */
    if (mta.menu_closed_count != 1)           goto MoveCheckFail;
    if (mta.menu_handle != NULL)              goto MoveCheckFail;

    /* a plain SELECT press on bare backdrop still just dismisses */
    wuss_mouse_click(mwuss, POINT(4, 196), wuss_BUTTON_SELECT,
                     wuss_MOUSE_DOWN, NULL);
    if (mwuss->menu.chain != NULL)            goto MoveCheckFail;

    /* the click-outside dismissal told task B too. Regression: before the
     * MENU_CLOSED fix wuss freed the chain here without telling task B, so
     * mtb.menu_handle dangled and this wuss_menu_close was a use-after-free
     * (ASan: heap-use-after-free in wuss_menu_close). */
    if (mtb.menu_closed_count != 1)           goto MoveCheckFail;
    if (mtb.menu_handle != NULL)              goto MoveCheckFail;
    wuss_menu_close(mtb.menu_handle); /* NULL now: safe no-op */
    if (mwuss->menu.chain != NULL)            goto MoveCheckFail;

    {
      int claimed;

      /* Escape dismisses an open chain and tells its owner, like a click
       * outside. Regression: menus never hold the focus, so the key went
       * nowhere and the chain stayed open. */
      wuss_mouse_click(mwuss, POINT(20, 20), wuss_BUTTON_MENU,
                       wuss_MOUSE_DOWN, NULL);
      wuss_mouse_click(mwuss, POINT(20, 20), wuss_BUTTON_MENU,
                       wuss_MOUSE_UP, NULL);
      if (mwuss->menu.chain == NULL)          goto MoveCheckFail;

      wuss_key(mwuss, wuss_KEY_ESCAPE, wuss_KEY_MOD_NONE, &claimed);
      if (!claimed)                           goto MoveCheckFail;
      if (mwuss->menu.chain != NULL)          goto MoveCheckFail;
      if (mta.menu_closed_count != 2)         goto MoveCheckFail;
      if (mta.menu_handle != NULL)            goto MoveCheckFail;
    }

    rc = result_OK;
    goto MoveDestroy;

MoveCheckFail:
    printf("wuss_test: menu-move check failed "
           "(a_press=%d b_press=%d chain=%p)\n",
           mta.menu_press_count, mtb.menu_press_count,
           (void *) mwuss->menu.chain);
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

    fontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
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
    if (qwuss->menu.chain == NULL)           goto QuitCheckFail;
    if (qmt.menu_handle == NULL)             goto QuitCheckFail;

    /* Phase 1: wuss_task_destroy on the chain's owner. It abandons the chain
     * (wuss_EVENT_MENU_CLOSED), which nulls qmt.menu_handle, *before* the
     * QUIT it then delivers -- so the close_chain_on_quit handler's
     * wuss_menu_close is a NULL no-op, not a double-free of the just-freed
     * chain (ASan: heap-use-after-free in wuss_menu_close). */
    reap_test_tasks();
    if (qmt.menu_handle != NULL)             goto QuitCheckFail;
    if (qwuss->menu.chain != NULL)           goto QuitCheckFail;

    /* Phase 1b: an autoclose task losing its last window with its chain
     * still open. Its self-reap must abandon the chain before QUIT, as
     * wuss_task_destroy does; otherwise the chain outlives its freed owner
     * and the QUIT handler's wuss_menu_close is left to clean up. */
    memset(&qmt, 0, sizeof(qmt));
    qmt.menu = &q_menu;
    task_q = mk_task(qwuss, menu_open_handle, &qmt);
    if (task_q == NULL) { rc = result_OOM; goto QuitDestroy; }
    qmt.self = task_q;
    wuss_task_set_autoclose(task_q, 1);
    rc = wuss_window_create(task_q, &bq, "Q1b",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&bq), SIZE2D(0, 0), &wq);
    if (rc != result_OK) goto QuitDestroy;
    wuss_mouse_click(qwuss, POINT(20, 20), wuss_BUTTON_MENU,
                     wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(qwuss, POINT(20, 20), wuss_BUTTON_MENU,
                     wuss_MOUSE_UP, NULL);
    if (qwuss->menu.chain == NULL)           goto QuitCheckFail;

    forget_test_tasks(); /* the autoclose reap below frees task_q */
    wuss_window_close(wq);
    if (qmt.menu_closed_count != 1)          goto QuitCheckFail;
    if (qmt.menu_handle != NULL)             goto QuitCheckFail;
    if (qwuss->menu.chain != NULL)           goto QuitCheckFail;

    /* Phase 1c: a window opened as a menu of its own (wuss_menu_open_window,
     * as a Save As dialogue from ^S) and then closed by its client. The close
     * must abandon the chain rather than leave its node pointing at the
     * freed window. Regression: spheroid's QUIT closed its Save As window
     * mid-chain, and wuss_destroy's later abandon hid the freed window and
     * delivered MENU_CLOSED to freed task data (ASan: heap-use-after-free). */
    memset(&qmt, 0, sizeof(qmt));
    qmt.menu = &q_menu;
    task_q = mk_task(qwuss, menu_open_handle, &qmt);
    if (task_q == NULL) { rc = result_OOM; goto QuitDestroy; }
    qmt.self = task_q;
    rc = wuss_window_create(task_q, &bq, "Q1c",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_HIDDEN,
                            wuss_NO_BACKDROP,
                            box_size(&bq), SIZE2D(0, 0), &wq);
    if (rc != result_OK) goto QuitDestroy;
    rc = wuss_menu_open_window(task_q, wq, POINT(20, 20), &qmt.menu_handle);
    if (rc != result_OK) goto QuitDestroy;
    if (qwuss->menu.chain == NULL)           goto QuitCheckFail;
    if (wq->flags & wuss_WINDOW_HIDDEN)      goto QuitCheckFail;

    wuss_window_close(wq);
    if (qmt.menu_closed_count != 1)          goto QuitCheckFail;
    if (qmt.menu_handle != NULL)             goto QuitCheckFail;
    if (qwuss->menu.chain != NULL)           goto QuitCheckFail;
    reap_test_tasks();

    /* Phase 1d: a core drag started from a window leaf (a Save As file icon)
     * owns the pointer until it ends. Crossing the parent menu's titlebar
     * mid-drag must not collapse the leaf; once the drag ends the usual
     * titlebar collapse applies again. Regression: the IDLE titlebar check
     * polled the pointer regardless and closed the leaf under the drag. */
    {
      wuss_menu_item_t   d_items[1];
      wuss_menu_t        d_menu;
      wuss_window_t     *leaf;
      wuss_menu_handle_t root;
      box_t              content;
      box_t              row;
      point_t            arrow;
      point_t            title;

      memset(&qmt, 0, sizeof(qmt));
      task_q = mk_task(qwuss, menu_open_handle, &qmt);
      if (task_q == NULL) { rc = result_OOM; goto QuitDestroy; }
      qmt.self = task_q;
      rc = wuss_window_create(task_q, &bq, "Q1d",
                              wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                              wuss_WINDOW_HIDDEN,
                              wuss_NO_BACKDROP,
                              box_size(&bq), SIZE2D(0, 0), &leaf);
      if (rc != result_OK) goto QuitDestroy;

      WUSS_MENU_ITEM_WINDOW(d_items, 0, "Leaf", wuss_MENU_ITEM_NONE, leaf);
      WUSS_MENU_TITLE(d_menu, "D", d_items, NELEMS(d_items));
      rc = wuss_menu_open(task_q, &d_menu, POINT(20, 40), &qmt.menu_handle);
      if (rc != result_OK) goto QuitDestroy;
      root = qwuss->menu.chain;

      /* hover the row's arrow (its rightmost pixel) to open the leaf */
      wuss__content_box(root->window, &content);
      wuss_icon_get_bbox(root->icons[0], &row);
      arrow = POINT(content.x0 + row.x1 - 1, content.y0 + (row.y0 + row.y1) / 2);
      title = POINT(content.x0 + 2, content.y0 - 2);
      wuss_mouse_move(qwuss, arrow, NULL);
      if (root->child == NULL || root->child->window != leaf)
        goto QuitCheckFail;

      rc = wuss_drag_start(qwuss, leaf, SIZE2D(8, 8), POINT(4, 4));
      if (rc != result_OK) goto QuitDestroy;
      wuss_mouse_move(qwuss, title, NULL);
      wuss_idle(qwuss);
      if (root->child == NULL)                 goto QuitCheckFail;

      wuss_mouse_click(qwuss, title, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
      wuss_idle(qwuss); /* drag over: the titlebar collapse applies again */
      if (root->child != NULL)                 goto QuitCheckFail;

      wuss_menu_close(qmt.menu_handle);
      reap_test_tasks();
    }

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
    if (qwuss->menu.chain == NULL)           goto QuitCheckFail;

    forget_test_tasks(); /* drop task_q from the registry; wuss_destroy frees it */
    wuss_destroy(qwuss); /* QUIT -> task_q closes the chain; must not fault */
    qwuss = NULL;

    rc = result_OK;
    goto QuitFreeOnly;

QuitCheckFail:
    printf("wuss_test: menu-quit check failed (chain=%p handle=%p)\n",
           (void *) qwuss->menu.chain, (void *) qmt.menu_handle);
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

  return result_TEST_PASSED;

Failure:
  return result_TEST_FAILED;
}

#else

#include "test/all-tests.h"

result_t wuss_menu_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
