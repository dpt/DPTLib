/* menu/test/menu-desc-test.c -- wuss menu descriptor tests */

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
#include "wuss/menu.h"
#include "wuss/menu-desc.h"
#include "wuss/icon-spec.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_MENUS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */

result_t wuss_menu_desc_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

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
    if (sub->items[2].shortcut != NULL)                   goto MenuFail;
    if (sub->items[2].window != NULL)                     goto MenuFail;

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

    /* malformed: '~' vararg not 0/1 */
    rc = wuss_menu_create_from_desc(&m, "T, ~Export", 2);
    if (rc != result_BAD_ARG)
      goto MenuFail;

    /* malformed: a name made only of options */
    rc = wuss_menu_create_from_desc(&m, "T, !, A", 0);
    if (rc != result_BAD_ARG)
      goto MenuFail;

    /* malformed: nesting deeper than eight levels */
    rc = wuss_menu_create_from_desc(&m,
           "T, A { S, B { S, C { S, D { S, E { S, F { S, G { S, H { S, I"
           " { S, J } } } } } } } } }");
    if (rc != result_BAD_ARG)
      goto MenuFail;

    /* malformed: } without { */
    rc = wuss_menu_create_from_desc(&m, "T, A }");
    if (rc != result_BAD_ARG)
      goto MenuFail;

    /* malformed: { } with no preceding item */
    rc = wuss_menu_create_from_desc(&m, "T, { S, A }");
    if (rc != result_BAD_ARG)
      goto MenuFail;

    /* malformed: %s expansion overflows the 64-byte text buffer */
    rc = wuss_menu_create_from_desc(&m, "T, %s",
           "0123456789012345678901234567890123456789012345678901234567890123");
    if (rc != result_BAD_ARG)
      goto MenuFail;

    /* malformed: literal text overflows the 64-byte text buffer */
    rc = wuss_menu_create_from_desc(&m,
           "T, 0123456789012345678901234567890123456789012345678901234567890123");
    if (rc != result_BAD_ARG)
      goto MenuFail;

    /* '>' deep-copies a borrowed menu whose items have their own submenus */
    {
      static const wuss_menu_item_t inner_items[] =
      {
        { "Leaf", wuss_MENU_ITEM_NONE, NULL }
      };
      static const wuss_menu_t inner =
      {
        NULL, inner_items, NELEMS(inner_items)
      };
      static const wuss_menu_item_t outer_items[] =
      {
        { "Branch", wuss_MENU_ITEM_NONE, &inner }
      };
      static const wuss_menu_t outer =
      {
        "Outer", outer_items, NELEMS(outer_items)
      };

      rc = wuss_menu_create_from_desc(&m, "T, >Tree", &outer);
      if (rc != result_OK)
        goto MenuFail;
      sub = m->items[0].submenu;
      if (sub == NULL || sub == &outer)                     goto NestFail;
      sub = sub->items[0].submenu;
      if (sub == NULL || sub == &inner)                     goto NestFail;
      if (sub->nitems != 1)                                 goto NestFail;
      if (strcmp(sub->items[0].text, "Leaf") != 0)          goto NestFail;
      wuss_menu_destroy(m);
      goto NestOK;

NestFail:
      wuss_menu_destroy(m);
      goto MenuFail;
NestOK: ;
    }

    rc = result_OK;
    goto MenuOK;

MenuFail:
    printf("wuss_test: menu-desc check failed\n");
    return result_TEST_FAILED;
MenuOK: ;
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_menu_desc_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_menu_desc_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
