/* gadget/test/gadget-test.c -- wuss gadget tests */

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
#include "wuss/menu.h"
#include "wuss/icon-spec.h"
#include "wuss/gadget/stringset.h"
#include "wuss/gadget/gridview.h"
#include "wuss/gadget/swatches.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_GADGETS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

#ifdef WUSS_GADGETS
/* stringset changed callback: counts calls into the int at opaque */
static result_t stringset_test_changed(wuss_stringset_t *stringset,
                                       int               index,
                                       void             *opaque)
{
  NOT_USED(stringset);
  NOT_USED(index);

  (*(int *) opaque)++;
  return result_OK;
}

/* gridview item source: no glyphs, labels from this fixed table -- must
 * match the gridview test's gv_labels array */
static const char *const gridview_test_labels[] =
{
  "Alpha", "Beta", "A very long item name that must be truncated"
};

static void gridview_test_item(int                   index,
                               void                 *opaque,
                               wuss_gridview_item_t *out)
{
  NOT_USED(opaque);

  out->glyph = NULL;
  out->label = gridview_test_labels[index];
}

/* gridview activate callback: records the activated index at opaque */
static void gridview_test_activate(wuss_gridview_t *gridview,
                                   int              index,
                                   wuss_button_t    button,
                                   void            *opaque)
{
  NOT_USED(gridview);
  NOT_USED(button);

  *(int *) opaque = index;
}

/* gridview drag callback: records the offered index at opaque, then declines
 * so no core drag starts */
static int gridview_test_drag(wuss_gridview_t *gridview,
                              int              index,
                              void            *opaque)
{
  NOT_USED(gridview);

  *(int *) opaque = index;
  return 0;
}

/* gridview drag callback that accepts, so a core drag starts */
static int gridview_test_accept_drag(wuss_gridview_t *gridview,
                                     int              index,
                                     void            *opaque)
{
  NOT_USED(gridview);
  NOT_USED(index);
  NOT_USED(opaque);

  return 1;
}

static struct
{
  int              gridview_test_drawn;     /* cells gridview_test_draw drew */
  wuss_gridview_t *gridview_forward_target; /* gridview_forward's recipient */
}
g;

/* gridview draw callback: counts the cells drawn and records the last index
 * at opaque */

static void gridview_test_draw(wuss_gridview_t    *gridview,
                               const wuss_event_t *event,
                               int                 index,
                               const box_t        *screen_box,
                               void               *opaque)
{
  NOT_USED(gridview);
  NOT_USED(event);
  NOT_USED(screen_box);

  g.gridview_test_drawn++;
  *(int *) opaque = index;
}

/* gridview item source like gridview_test_item, but with the bitmap at
 * opaque as every item's glyph */
static void gridview_test_glyph_item(int                   index,
                                     void                 *opaque,
                                     wuss_gridview_item_t *out)
{
  out->glyph = (const bitmap_t *) opaque;
  out->label = gridview_test_labels[index];
}
#endif

#ifdef WUSS_GADGETS
/* task handler that hands every event to g.g.gridview_forward_target */

static result_t gridview_forward(wuss_window_t      *window,
                                 const wuss_event_t *event,
                                 void               *task_data)
{
  NOT_USED(window);
  NOT_USED(task_data);

  if (g.gridview_forward_target != NULL)
    (void) wuss_gridview_handle_event(g.gridview_forward_target, event);

  return result_OK;
}
#endif /* WUSS_GADGETS */

/* ----------------------------------------------------------------------- */

result_t wuss_gadget_test(const char *resources)
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

  printf("test: wuss_stringset opens its menu and tracks picks\n");
  {
    static const char *const ss_strings[] = { "Red", "Green", "Blue" };

    const char       *ssfontfile;
    bmfont_t         *ssfont;
    screen_t          ssscr;
    bitmap_t          ssbm;
    void             *sspixels;
    wuss_t           *sswuss;
    wuss_font_desc_t  ssfdesc;
    test_task_t       sstc;
    wuss_task_t      *ssowner;
    wuss_window_t    *sswin;
    wuss_stringset_t *ss;
    wuss_icon_t      *arrow;
    wuss_event_t      ev;
    box_t             sscontent;
    result_t          ssrc;
    int               calls;
    int               i;

    /* a menu needs a font, and the arrow the icon set, so neither the
     * shared fontless wuss nor a resource-less one will do */
    ssfontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
    rc = bmfont_create(ssfontfile, &ssfont);
    if (rc != result_OK) goto StringSetFail;

    sspixels = malloc((size_t) rowbytes * 200);
    if (sspixels == NULL) { rc = result_OOM; goto StringSetFail; }
    rc = bitmap_init(&ssbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, sspixels);
    if (rc != result_OK) goto StringSetFail;
    screen_for_bitmap(&ssscr, &ssbm);

    ssfdesc.font       = ssfont;
    ssfdesc.font_class = wuss_FONT_CLASS_NONE;
    ssfdesc.name       = NULL;
    rc = wuss_create(&ssscr, &ssfdesc, 1, NULL, 0, NULL, NULL, resources,
                     &sswuss);
    if (rc != result_OK) goto StringSetFail;

    memset(&sstc, 0, sizeof(sstc));
    ssowner = mk_task(sswuss, test_handle, &sstc);
    if (ssowner == NULL) { rc = result_OOM; goto StringSetFail; }

    sscontent = (box_t) BOX_POS_SIZE(10, 10, 150, 100);
    rc = wuss_window_create(ssowner, &sscontent, "SS", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, SIZE2D(150, 100), SIZE2D(0, 0),
                            &sswin);
    if (rc != result_OK) goto StringSetFail;

    /* a zero count is refused */
    if (wuss_stringset_create(&ss, sswin, (box_t) BOX_POS_SIZE(4, 4, 120, 22),
                              "Colour", -1, ss_strings, 0, NULL, NULL) !=
        result_BAD_ARG)
      goto StringSetFail;

    calls = 0;
    rc = wuss_stringset_create(&ss, sswin, (box_t) BOX_POS_SIZE(4, 4, 120, 22),
                               "Colour", 40, ss_strings, NELEMS(ss_strings),
                               stringset_test_changed, &calls);
    if (rc != result_OK) goto StringSetFail;
    /* the field starts after the title */
    if (sswin->icons[0]->spec.bbox.x0 != 4 + 40 + wuss_STD_GAP)
      goto StringSetFail;
    if (wuss_stringset_get_index(ss) != 0) goto StringSetFail;

    /* the arrow sits at the right edge, drawn from the icon set */
    arrow = wuss__icon_hit_test(sswin, POINT(4 + 120 - 2, 4 + 11));
    if (arrow == NULL || wuss_icon_get_type(arrow) != wuss_ICON_TYPE_BITMAP)
      goto StringSetFail;

    /* a foreign event is declined */
    ev.kind = wuss_EVENT_IDLE;
    if (wuss_stringset_handle_event(ss, &ev, &ssrc)) goto StringSetFail;

    /* a Select click on the arrow opens the gadget's menu */
    ev.kind              = wuss_EVENT_ICON;
    ev.data.icon.icon    = arrow;
    ev.data.icon.action  = wuss_MOUSE_UP;
    ev.data.icon.button  = wuss_BUTTON_SELECT;
    ev.data.icon.value   = 0;
    if (!wuss_stringset_handle_event(ss, &ev, &ssrc) || ssrc != result_OK)
      goto StringSetFail;
    if (wuss__menu_chain(sswuss) == NULL) goto StringSetFail;

    /* a pick of another entry updates the index and fires the callback */
    ev.kind                    = wuss_EVENT_MENU_SELECT;
    ev.data.menu_select.menu   = wuss_menu_handle_menu(wuss__menu_chain(sswuss));
    ev.data.menu_select.index  = 2;
    ev.data.menu_select.button = wuss_BUTTON_ADJUST;
    if (!wuss_stringset_handle_event(ss, &ev, &ssrc) || ssrc != result_OK)
      goto StringSetFail;
    if (wuss_stringset_get_index(ss) != 2 || calls != 1) goto StringSetFail;

    /* re-picking the current entry does not */
    if (!wuss_stringset_handle_event(ss, &ev, &ssrc) || calls != 1)
      goto StringSetFail;

    /* the programmatic path is range-checked and silent */
    if (wuss_stringset_set_index(ss, 3) != result_BAD_ARG) goto StringSetFail;
    if (wuss_stringset_set_index(ss, 1) != result_OK)      goto StringSetFail;
    if (wuss_stringset_get_index(ss) != 1 || calls != 1)   goto StringSetFail;

    /* destroying it closes the menu */
    wuss_stringset_destroy(ss);
    if (wuss__menu_chain(sswuss) != NULL) goto StringSetFail;

    /* once the chain is freed behind the gadget -- after a Select pick, or
     * a wuss-initiated close reported by MENU_CLOSED -- destroying it must
     * not touch the dead handle (ASan catches it) */
    for (i = 0; i < 2; i++)
    {
      rc = wuss_stringset_create(&ss, sswin,
                                 (box_t) BOX_POS_SIZE(4, 40, 120, 22), "Colour",
                                 -1, ss_strings, NELEMS(ss_strings), NULL, NULL);
      if (rc != result_OK) goto StringSetFail;

      ev.kind             = wuss_EVENT_ICON;
      ev.data.icon.icon   = wuss__icon_hit_test(sswin, POINT(122, 51));
      ev.data.icon.action = wuss_MOUSE_UP;
      ev.data.icon.button = wuss_BUTTON_SELECT;
      ev.data.icon.value  = 0;
      if (!wuss_stringset_handle_event(ss, &ev, &ssrc) || ssrc != result_OK)
        goto StringSetFail;
      if (wuss__menu_chain(sswuss) == NULL) goto StringSetFail;

      if (i == 0)
      {
        ev.kind                    = wuss_EVENT_MENU_SELECT;
        ev.data.menu_select.menu   = wuss_menu_handle_menu(wuss__menu_chain(sswuss));
        ev.data.menu_select.index  = 1;
        ev.data.menu_select.button = wuss_BUTTON_SELECT;
        if (!wuss_stringset_handle_event(ss, &ev, &ssrc)) goto StringSetFail;
        wuss_menu_close(wuss__menu_chain(sswuss)); /* as wuss does after Select */
      }
      else
      {
        wuss_menu_close(wuss__menu_chain(sswuss)); /* as a click outside does */
        ev.kind = wuss_EVENT_MENU_CLOSED;
        if (wuss_stringset_handle_event(ss, &ev, &ssrc)) goto StringSetFail;
      }

      wuss_stringset_destroy(ss);
    }

    reap_test_tasks();
    wuss_destroy(sswuss);
    free(sspixels);
    bmfont_destroy(ssfont);
    goto StringSetOK;

StringSetFail:
    printf("wuss_test: stringset check failed\n");
    return result_TEST_FAILED;
StringSetOK: ;
  }

  printf("test: wuss_gridview reflows, hit-tests and tracks selection\n");
  {
    const char      *gvfontfile;
    bmfont_t        *gvfont;
    screen_t         gvscr;
    bitmap_t         gvbm;
    void            *gvpixels;
    wuss_t          *gvwuss;
    wuss_font_desc_t gvfdesc;
    test_task_t      gvtc;
    wuss_task_t     *gvowner;
    wuss_window_t   *gvwin;
    wuss_gridview_t *gv;
    wuss_event_t     ev;
    box_t            gvcontent;
    int              cell_h;
    int              cell_w;
    int              activated;

    gvfontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
    rc = bmfont_create(gvfontfile, &gvfont);
    if (rc != result_OK) goto GridViewFail;

    gvpixels = malloc((size_t) rowbytes * 200);
    if (gvpixels == NULL) { rc = result_OOM; goto GridViewFail; }
    rc = bitmap_init(&gvbm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes,
                     NULL, gvpixels);
    if (rc != result_OK) goto GridViewFail;
    screen_for_bitmap(&gvscr, &gvbm);

    gvfdesc.font       = gvfont;
    gvfdesc.font_class = wuss_FONT_CLASS_NONE;
    gvfdesc.name       = NULL;
    rc = wuss_create(&gvscr, &gvfdesc, 1, NULL, 0, NULL, NULL, resources,
                     &gvwuss);
    if (rc != result_OK) goto GridViewFail;

    memset(&gvtc, 0, sizeof(gvtc));
    gvowner = mk_task(gvwuss, test_handle, &gvtc);
    if (gvowner == NULL) { rc = result_OOM; goto GridViewFail; }

    /* narrower than two cells, so the grid reflows to a single column and
     * item 1 lands directly below item 0 -- a known hit-test point without
     * reaching into the gadget's opaque cell-size state */
    gvcontent = (box_t) BOX_POS_SIZE(10, 10, 40, 100);
    rc = wuss_window_create(gvowner, &gvcontent, "GV", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, SIZE2D(40, 100), SIZE2D(0, 0),
                            &gvwin);
    if (rc != result_OK) goto GridViewFail;

    /* a negative count is refused */
    if (wuss_gridview_create(&gv, gvwin, -1, gridview_test_item, 8, NULL,
                             NULL) != result_BAD_ARG)
      goto GridViewFail;

    activated = -1;
    rc = wuss_gridview_create(&gv, gvwin, NELEMS(gridview_test_labels),
                              gridview_test_item, 8, gridview_test_activate,
                              &activated);
    if (rc != result_OK) goto GridViewFail;

    /* an OPEN reflows to one column and sets the doc extent to fit every
     * row (white-box: struct wuss_window is visible via impl.h above) */
    ev.kind = wuss_EVENT_OPEN;
    (void) wuss_gridview_handle_event(gv, &ev);
    if (gvwin->doc.w <= 0 || gvwin->doc.h <= 0) goto GridViewFail;
    if (gvwin->doc.h % (int) NELEMS(gridview_test_labels) != 0) goto GridViewFail;
    cell_h = gvwin->doc.h / (int) NELEMS(gridview_test_labels);

    /* one column fits, and natural sizing and cell boxes follow the
     * layout */
    cell_w = wuss_gridview_get_cell_size(gv).w;
    if (wuss_gridview_get_columns(gv) != 1) goto GridViewFail;

    /* the minimum is one cell (already label_chars wide here, from the long
     * label); wuss_KEEP leaves an axis alone */
    if (gvwin->min_doc.w != wuss_gridview_width_for_columns(gv, 1) ||
        gvwin->min_doc.h != cell_h) goto GridViewFail;
    if (wuss_window_set_min_size(gvwin, SIZE2D(wuss_KEEP, 9)) != result_OK) goto GridViewFail;
    if (gvwin->min_doc.h != 9 ||
        gvwin->min_doc.w != wuss_gridview_width_for_columns(gv, 1)) goto GridViewFail;
    if (wuss_window_set_min_size(gvwin, SIZE2D(-2, 0)) != result_BAD_ARG) goto GridViewFail;
    if (wuss_gridview_width_for_columns(gv, 3) != 3 * cell_w) goto GridViewFail;
    if (wuss_gridview_natural_size(gv, 3, 1).h != cell_h) goto GridViewFail;
    if (wuss_gridview_cell_box(gv, 1).y0 != cell_h) goto GridViewFail;

    /* RELAYOUT and PALETTE re-measure without consuming the event */
    ev.kind = wuss_EVENT_RELAYOUT;
    if (wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;

    /* an inset widens the width for a column count */
    wuss_gridview_set_layout(gv, 0, 1, 4);
    if (wuss_gridview_width_for_columns(gv, 2) != 2 * cell_w + 8) goto GridViewFail;
    wuss_gridview_set_layout(gv, 0, 0, 0);

    /* a foreign event is declined */
    ev.kind = wuss_EVENT_IDLE;
    if (wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;

    /* one short label: the minimum is still wide enough for a ten-character
     * label, so wider than the one narrow column, and so is the natural
     * size; gv then reflows the window back */
    {
      wuss_gridview_t *short_gv;
      int              one_col;

      rc = wuss_gridview_create(&short_gv, gvwin, 1, gridview_test_item, 12,
                                NULL, NULL);
      if (rc != result_OK) goto GridViewFail;
      one_col = wuss_gridview_width_for_columns(short_gv, 1);
      if (gvwin->min_doc.w <= one_col ||
          wuss_gridview_natural_size(short_gv, 1, 1).w != gvwin->min_doc.w)
      {
        wuss_gridview_destroy(short_gv);
        goto GridViewFail;
      }
      wuss_gridview_destroy(short_gv);

      /* no items: no glyph to measure, yet the minimum keeps room for an
       * icon above the label */
      rc = wuss_gridview_create(&short_gv, gvwin, 0, gridview_test_item, 12,
                                NULL, NULL);
      if (rc != result_OK) goto GridViewFail;
      if (gvwin->min_doc.h < 34)
      {
        wuss_gridview_destroy(short_gv);
        goto GridViewFail;
      }
      wuss_gridview_destroy(short_gv);

      ev.kind = wuss_EVENT_OPEN;
      (void) wuss_gridview_handle_event(gv, &ev);
    }

    /* a redraw is consumed and draws without crashing (ASan-checked) */
    ev.kind                = wuss_EVENT_REDRAW;
    ev.data.redraw.scr     = &gvscr;
    ev.data.redraw.content = &gvcontent;
    ev.data.redraw.bounds  = &gvcontent;
    ev.data.redraw.scroll  = POINT(0, 0);
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;

    /* a Select DOWN on item 0 (top-left of the grid) selects only it */
    ev.kind              = wuss_EVENT_MOUSE;
    ev.data.mouse.action = wuss_MOUSE_DOWN;
    ev.data.mouse.point  = POINT(2, 2);
    ev.data.mouse.button = wuss_BUTTON_SELECT;
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    if (!wuss_gridview_is_selected(gv, 0)) goto GridViewFail;
    if (wuss_gridview_is_selected(gv, 1)) goto GridViewFail;

    /* Adjust on item 1 toggles it on without clearing item 0 */
    ev.data.mouse.point  = POINT(2, 2 + cell_h);
    ev.data.mouse.button = wuss_BUTTON_ADJUST;
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    if (!wuss_gridview_is_selected(gv, 0)) goto GridViewFail;
    if (!wuss_gridview_is_selected(gv, 1)) goto GridViewFail;

    /* Adjust again toggles item 1 back off */
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    if (wuss_gridview_is_selected(gv, 1)) goto GridViewFail;

    /* Select on item 1, then a double-click on it activates it */
    ev.data.mouse.button = wuss_BUTTON_SELECT;
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    if (wuss_gridview_is_selected(gv, 0)) goto GridViewFail;
    if (!wuss_gridview_is_selected(gv, 1)) goto GridViewFail;

    ev.data.mouse.button = wuss_BUTTON_SELECT | wuss_BUTTON_DOUBLE;
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    if (activated != 1) goto GridViewFail;

    /* the activated item is deselected */
    if (wuss_gridview_is_selected(gv, 1)) goto GridViewFail;

    /* a hit test finds items by document point, and misses past the last */
    if (wuss_gridview_hit_test(gv, POINT(2, 2 + cell_h)) != 1) goto GridViewFail;
    if (wuss_gridview_hit_test(gv, POINT(2, 2 + cell_h * (int) NELEMS(gridview_test_labels))) != -1)
      goto GridViewFail;

    /* Select on an already-selected item changes nothing: the rest of the
     * selection stays */
    ev.data.mouse.button = wuss_BUTTON_SELECT;
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    ev.data.mouse.point  = POINT(2, 2);
    ev.data.mouse.button = wuss_BUTTON_ADJUST;
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    ev.data.mouse.button = wuss_BUTTON_SELECT;
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    if (!wuss_gridview_is_selected(gv, 0) ||
        !wuss_gridview_is_selected(gv, 1)) goto GridViewFail;

    /* Adjust on empty space leaves the selection alone */
    ev.data.mouse.point  = POINT(2, 2 + cell_h * (int) NELEMS(gridview_test_labels));
    ev.data.mouse.button = wuss_BUTTON_ADJUST;
    if (wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    if (!wuss_gridview_is_selected(gv, 0) ||
        !wuss_gridview_is_selected(gv, 1)) goto GridViewFail;

    /* Select on empty space clears the selection */
    ev.data.mouse.button = wuss_BUTTON_SELECT;
    if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
    if (wuss_gridview_is_selected(gv, 0) ||
        wuss_gridview_is_selected(gv, 1)) goto GridViewFail;

    /* a press and a move past the drag threshold offers the item to the drag
     * callback, once */
    wuss_gridview_set_drag(gv, gridview_test_drag);
    ev.data.mouse.point  = POINT(2, 2 + cell_h);
    ev.data.mouse.button = wuss_BUTTON_SELECT;
    ev.data.mouse.action = wuss_MOUSE_DOWN;
    (void) wuss_gridview_handle_event(gv, &ev);
    activated            = -1;
    ev.data.mouse.action = wuss_MOUSE_MOVE;
    ev.data.mouse.point  = POINT(3, 2 + cell_h);
    (void) wuss_gridview_handle_event(gv, &ev);
    if (activated != -1) goto GridViewFail;
    ev.data.mouse.point  = POINT(30, 2 + cell_h);
    (void) wuss_gridview_handle_event(gv, &ev);
    if (activated != 1) goto GridViewFail;
    activated = -1;
    (void) wuss_gridview_handle_event(gv, &ev);
    if (activated != -1) goto GridViewFail;

    /* leaving the window forgets a press */
    ev.data.mouse.action = wuss_MOUSE_DOWN;
    ev.data.mouse.point  = POINT(2, 2 + cell_h);
    (void) wuss_gridview_handle_event(gv, &ev);
    ev.kind = wuss_EVENT_POINTER_EXIT;
    (void) wuss_gridview_handle_event(gv, &ev);
    ev.kind              = wuss_EVENT_MOUSE;
    ev.data.mouse.action = wuss_MOUSE_MOVE;
    ev.data.mouse.point  = POINT(30, 2 + cell_h);
    (void) wuss_gridview_handle_event(gv, &ev);
    if (activated != -1) goto GridViewFail;
    wuss_gridview_set_drag(gv, NULL);

    /* select all, then clear */
    wuss_gridview_select_all(gv);
    if (wuss_gridview_selected_count(gv) != (int) NELEMS(gridview_test_labels))
      goto GridViewFail;
    wuss_gridview_clear_selection(gv);
    if (wuss_gridview_selected_count(gv) != 0) goto GridViewFail;

    /* select replaces the selection; out of range is ignored */
    wuss_gridview_select(gv, 1);
    wuss_gridview_select(gv, 2);
    if (wuss_gridview_selected_count(gv) != 1 ||
        wuss_gridview_is_selected(gv, 1) || !wuss_gridview_is_selected(gv, 2))
      goto GridViewFail;
    wuss_gridview_select(gv, -1);
    wuss_gridview_select(gv, (int) NELEMS(gridview_test_labels));
    if (!wuss_gridview_is_selected(gv, 2)) goto GridViewFail;
    wuss_gridview_clear_selection(gv);

    /* a changed item list re-measures the cell: dropping the long label
     * narrows it, bringing the label back widens it again */
    rc = wuss_gridview_set_count(gv, 2);
    if (rc != result_OK) goto GridViewFail;
    if (wuss_gridview_get_cell_size(gv).w >= cell_w) goto GridViewFail;
    rc = wuss_gridview_set_count(gv, NELEMS(gridview_test_labels));
    if (rc != result_OK) goto GridViewFail;
    if (wuss_gridview_get_cell_size(gv).w != cell_w) goto GridViewFail;
    if (wuss_gridview_set_count(gv, -1) != result_BAD_ARG) goto GridViewFail;

    /* a fixed cell size holds across a change of count; with three short
     * rows the rest of the window shows the background */
    {
      unsigned int    *px;
      unsigned int     plain;
      int              claimed;
      bitmap_t         glyph;
      unsigned int     glyph_px[4 * 4];
      wuss_gridview_t *glyph_gv;
      wuss_window_t   *glyph_win;
      box_t            glyph_content;

      px = (unsigned int *) gvpixels;

      wuss_gridview_set_cell_size(gv, SIZE2D(30, 10));
      rc = wuss_gridview_set_count(gv, NELEMS(gridview_test_labels));
      if (rc != result_OK) goto GridViewFail;
      if (wuss_gridview_get_cell_size(gv).w != 30 ||
          wuss_gridview_get_columns(gv) != 1) goto GridViewFail;

      /* a full redraw paints the selected cell 0 unlike the plain cell 1,
       * truncating the long label to fit */
      wuss_gridview_select(gv, 0);
      gvscr.clip             = gvcontent;
      ev.kind                = wuss_EVENT_REDRAW;
      ev.data.redraw.scr     = &gvscr;
      ev.data.redraw.content = &gvcontent;
      ev.data.redraw.bounds  = &gvcontent;
      ev.data.redraw.scroll  = POINT(0, 0);
      if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
      if (px[11 * 200 + 11] == px[21 * 200 + 11]) goto GridViewFail;
      plain = px[100 * 200 + 12];

      wuss_gridview_set_background(gv, colour_rgba(0xFF, 0x00, 0x00, 0xFF));
      if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
      if (px[100 * 200 + 12] == plain) goto GridViewFail;
      wuss_gridview_clear_selection(gv);

      /* a draw callback replaces the cell drawing, and a clip over the
       * second row draws only item 1 */
      g.gridview_test_drawn = 0;
      activated           = -1;
      wuss_gridview_set_draw(gv, gridview_test_draw, 1);
      gvscr.clip = (box_t) BOX_POS_SIZE(10, 20, 40, 10);
      if (!wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
      if (g.gridview_test_drawn != 1 || activated != 1) goto GridViewFail;
      gvscr.clip = gvcontent;
      wuss_gridview_set_draw(gv, NULL, 1);

      /* a MENU click on an item, with nothing selected, selects it until
       * the menu closes unpicked; it is not consumed */
      ev.kind              = wuss_EVENT_MOUSE;
      ev.data.mouse.action = wuss_MOUSE_DOWN;
      ev.data.mouse.point  = POINT(2, 12);
      ev.data.mouse.button = wuss_BUTTON_MENU;
      if (wuss_gridview_handle_event(gv, &ev)) goto GridViewFail;
      if (wuss_gridview_selected_count(gv) != 1 ||
          !wuss_gridview_is_selected(gv, 1)) goto GridViewFail;
      ev.kind                    = wuss_EVENT_MENU_CLOSED;
      ev.data.menu_closed.picked = 0;
      (void) wuss_gridview_handle_event(gv, &ev);
      if (wuss_gridview_selected_count(gv) != 0) goto GridViewFail;

      /* after a pick the selection lasts until the next event, so the
       * menu's action still sees it */
      ev.kind              = wuss_EVENT_MOUSE;
      ev.data.mouse.action = wuss_MOUSE_DOWN;
      ev.data.mouse.point  = POINT(2, 12);
      ev.data.mouse.button = wuss_BUTTON_MENU;
      (void) wuss_gridview_handle_event(gv, &ev);
      ev.kind                    = wuss_EVENT_MENU_CLOSED;
      ev.data.menu_closed.picked = 1;
      (void) wuss_gridview_handle_event(gv, &ev);
      if (wuss_gridview_selected_count(gv) != 1) goto GridViewFail;
      ev.kind = wuss_EVENT_IDLE;
      (void) wuss_gridview_handle_event(gv, &ev);
      if (wuss_gridview_selected_count(gv) != 0) goto GridViewFail;

      /* with an existing selection a MENU click leaves it alone */
      wuss_gridview_select(gv, 0);
      ev.kind              = wuss_EVENT_MOUSE;
      ev.data.mouse.action = wuss_MOUSE_DOWN;
      ev.data.mouse.point  = POINT(2, 12);
      ev.data.mouse.button = wuss_BUTTON_MENU;
      (void) wuss_gridview_handle_event(gv, &ev);
      if (wuss_gridview_selected_count(gv) != 1 ||
          !wuss_gridview_is_selected(gv, 0)) goto GridViewFail;
      wuss_gridview_clear_selection(gv);

      /* an accepting drag callback starts a core drag; Escape cancels it */
      wuss_gridview_set_drag(gv, gridview_test_accept_drag);
      ev.data.mouse.button = wuss_BUTTON_SELECT;
      (void) wuss_gridview_handle_event(gv, &ev);
      ev.data.mouse.action = wuss_MOUSE_MOVE;
      ev.data.mouse.point  = POINT(30, 12);
      (void) wuss_gridview_handle_event(gv, &ev);
      if (!wuss_is_dragging(gvwuss)) goto GridViewFail;
      rc = wuss_key(gvwuss, wuss_KEY_ESCAPE, wuss_KEY_MOD_NONE, &claimed);
      if (rc != result_OK || wuss_is_dragging(gvwuss)) goto GridViewFail;
      wuss_gridview_set_drag(gv, NULL);
      wuss_gridview_clear_selection(gv);

      /* a gridview window needs somewhere to return both handles */
      if (wuss_gridview_window_create(NULL, gvowner, NULL, "Grid", 1,
                                      gridview_test_item, 8, NULL, NULL, 0,
                                      &glyph_win) != result_NULL_ARG)
        goto GridViewFail;

      /* fitting three of the wide cells across the screen drops to the
       * columns that fit; glyphs draw above the labels */
      memset(glyph_px, 0xFF, sizeof(glyph_px));
      rc = bitmap_init(&glyph, SIZE2D(4, 4), pixelfmt_bgrx8888, 4 * 4, NULL,
                       glyph_px);
      if (rc != result_OK) goto GridViewFail;
      rc = wuss_gridview_window_create(&glyph_gv, gvowner, NULL, "Grid",
                                       NELEMS(gridview_test_labels),
                                       gridview_test_glyph_item, 8, NULL,
                                       &glyph, 0, &glyph_win);
      if (rc != result_OK) goto GridViewFail;
      wuss_gridview_fit_window(glyph_gv, 3, 1);
      wuss_window_get_content_bounds(glyph_win, &glyph_content);
      if (glyph_content.x1 - glyph_content.x0 <
          wuss_gridview_get_cell_size(glyph_gv).w ||
          glyph_content.x1 > 200)
      {
        wuss_gridview_destroy(glyph_gv);
        goto GridViewFail;
      }

      gvscr.clip             = glyph_content;
      ev.kind                = wuss_EVENT_REDRAW;
      ev.data.redraw.scr     = &gvscr;
      ev.data.redraw.content = &glyph_content;
      ev.data.redraw.bounds  = &glyph_content;
      ev.data.redraw.scroll  = POINT(0, 0);
      if (!wuss_gridview_handle_event(glyph_gv, &ev))
      {
        wuss_gridview_destroy(glyph_gv);
        goto GridViewFail;
      }

      wuss_gridview_destroy(glyph_gv);
      wuss_window_close(glyph_win);
    }

    wuss_gridview_destroy(gv);

    reap_test_tasks();
    wuss_destroy(gvwuss);
    free(gvpixels);
    bmfont_destroy(gvfont);
    goto GridViewOK;

GridViewFail:
    printf("wuss_test: gridview check failed\n");
    return result_TEST_FAILED;
GridViewOK: ;
  }

  printf("test: wuss_resize relays out a gridview window\n");
  {
    const char      *rlfontfile;
    bmfont_t        *rlfont;
    screen_t         rlscr, rlsmall;
    bitmap_t         rlbm, rlsmallbm;
    void            *rlpixels, *rlsmallpixels;
    wuss_t          *rlwuss;
    wuss_font_desc_t rlfdesc;
    wuss_task_t     *rlowner;
    wuss_window_t   *rlwin;
    wuss_gridview_t *rlgv;
    box_t            rlcontent, rlbounds;
    int              big_doc_w;
    int              min_w;

    rlfontfile = pathf("%s/resources/bmfonts/Tiny/Regular.png", resources);
    rc = bmfont_create(rlfontfile, &rlfont);
    if (rc != result_OK) goto RelayoutFail;

    rlpixels      = malloc((size_t) rowbytes * 300);
    rlsmallpixels = malloc((size_t) rowbytes * 100);
    if (rlpixels == NULL || rlsmallpixels == NULL) { rc = result_OOM; goto RelayoutFail; }
    rc = bitmap_init(&rlbm, SIZE2D(300, 300), pixelfmt_bgrx8888, rowbytes,
                     NULL, rlpixels);
    if (rc != result_OK) goto RelayoutFail;
    rc = bitmap_init(&rlsmallbm, SIZE2D(100, 100), pixelfmt_bgrx8888,
                     rowbytes, NULL, rlsmallpixels);
    if (rc != result_OK) goto RelayoutFail;
    screen_for_bitmap(&rlscr, &rlbm);
    screen_for_bitmap(&rlsmall, &rlsmallbm);

    rlfdesc.font       = rlfont;
    rlfdesc.font_class = wuss_FONT_CLASS_NONE;
    rlfdesc.name       = NULL;
    rc = wuss_create(&rlscr, &rlfdesc, 1, NULL, 0, NULL, NULL, resources,
                     &rlwuss);
    if (rc != result_OK) goto RelayoutFail;

    rlowner = mk_task(rlwuss, gridview_forward, NULL);
    if (rlowner == NULL) { rc = result_OOM; goto RelayoutFail; }

    rlbounds = (box_t) BOX_POS_SIZE(10, 10, 280, 200);
    rc = wuss_window_create(rlowner, &rlbounds, "RL", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, SIZE2D(280, 200),
                            SIZE2D(0, 0), &rlwin);
    if (rc != result_OK) goto RelayoutFail;

    rc = wuss_gridview_create(&rlgv, rlwin, NELEMS(gridview_test_labels),
                              gridview_test_item, 8, NULL, NULL);
    if (rc != result_OK) goto RelayoutFail;
    g.gridview_forward_target = rlgv; /* events now reach the gridview */

    big_doc_w = rlwin->doc.w;
    min_w     = wuss_gridview_width_for_columns(rlgv, 1);

    /* a smaller screen shrinks the window to it, the gridview rewraps to
     * fit and the extent falls with the screen */
    rc = wuss_resize(rlwuss, &rlsmall);
    if (rc != result_OK) goto RelayoutFail;
    wuss_window_get_content_bounds(rlwin, &rlcontent);
    if (rlcontent.x1 > 100 || rlcontent.y1 > 100) goto RelayoutFail;
    if (rlwin->doc.w >= big_doc_w) goto RelayoutFail;
    if (wuss_gridview_get_columns(rlgv) < 1) goto RelayoutFail;
    if (rlwin->min_doc.w != min_w) goto RelayoutFail;

    /* a bigger screen raises the extent again so the window can be dragged
     * wider */
    rc = wuss_resize(rlwuss, &rlscr);
    if (rc != result_OK) goto RelayoutFail;
    if (rlwin->doc.w != big_doc_w) goto RelayoutFail;

    wuss_gridview_destroy(rlgv);
    g.gridview_forward_target = NULL;

    reap_test_tasks();
    wuss_destroy(rlwuss);
    free(rlpixels);
    free(rlsmallpixels);
    bmfont_destroy(rlfont);
    goto RelayoutOK;

RelayoutFail:
    printf("wuss_test: gridview relayout check failed\n");
    return result_TEST_FAILED;
RelayoutOK: ;
  }

  printf("test: wuss_swatches hit-tests cells and misses gaps\n");
  {
    colour_t        swcolours[5];
    wuss_swatches_t sw;

    memset(swcolours, 0, sizeof(swcolours));

    /* 5 colours, roughly square: 3 columns of 10px cells, 2px gaps */
    sw.bbox     = (box_t) BOX_POS_SIZE(10, 20, 34, 22);
    sw.columns  = 0;
    sw.gap      = 2;
    sw.outline  = 0;
    sw.colours  = swcolours;
    sw.ncolours = 5;

    if (wuss_swatches_hit_test(&sw, POINT(10, 20)) != 0 ||
        wuss_swatches_hit_test(&sw, POINT(19, 29)) != 0 ||
        wuss_swatches_hit_test(&sw, POINT(20, 20)) != -1 || /* gap */
        wuss_swatches_hit_test(&sw, POINT(22, 20)) != 1 ||
        wuss_swatches_hit_test(&sw, POINT(22, 32)) != 4 || /* row 1 */
        wuss_swatches_hit_test(&sw, POINT(34, 32)) != -1 || /* no 6th */
        wuss_swatches_hit_test(&sw, POINT(9, 20)) != -1 ||
        wuss_swatches_hit_test(&sw, POINT(10, 42)) != -1)
    {
      printf("wuss_test: swatches hit-test failed\n");
      return result_TEST_FAILED;
    }
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_gadget_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_gadget_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
