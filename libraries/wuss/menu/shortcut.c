/* wuss/menu/shortcut.c -- dispatch key presses via menu shortcut labels */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"

#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/wuss.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

/* ASCII-only case fold: shortcut labels are written in UPPERCASE but the
 * frontend delivers a Ctrl letter lowercase. */
static int fold(int c)
{
  return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

/* The key code "name" (a label with any '^' and shift prefix removed) stands
 * for: a single character, "SPACE" or "F1".."F12". -1 for anything else.
 * "*named" is set for SPACE and Fn, whose Shift state matters. */
static int shortcut_code(const char *name, int *named)
{
  int n;

  *named = 0;

  if (name[0] != '\0' && name[1] == '\0')
    return fold((unsigned char) name[0]);

  *named = 1;

  if (strcmp(name, "SPACE") == 0)
    return ' ';

  if (name[0] == 'F' && name[1] >= '1' && name[1] <= '9')
  {
    n = atoi(name + 1);
    if (n >= 1 && n <= 12)
      return wuss_KEY_F1 + n - 1;
  }

  return -1;
}

/* Whether "label" names the key in "key". A '^' prefix requires Ctrl, no
 * prefix forbids it; Alt never matches. A following WUSS_MENU_SHIFT requires
 * Shift; without it Shift is forbidden for SPACE and Fn but ignored for a
 * single character (whose case already carries it). */
static int shortcut_matches(const char *label, const wuss_event_t *key)
{
  static const size_t glyph_len = sizeof(WUSS_MENU_SHIFT) - 1;

  wuss_key_modifiers_t mods;
  int                  want_ctrl;
  int                  want_shift;
  int                  named;
  int                  code;

  mods = key->data.key.modifiers;
  if (mods & wuss_KEY_MOD_ALT)
    return 0;

  want_ctrl = (label[0] == '^');
  if (want_ctrl != ((mods & wuss_KEY_MOD_CTRL) != 0))
    return 0;

  label += want_ctrl;

  want_shift = (strncmp(label, WUSS_MENU_SHIFT, glyph_len) == 0);
  if (want_shift)
    label += glyph_len;

  code = shortcut_code(label, &named);
  if (code < 0 || code != fold(key->data.key.code))
    return 0;

  if (want_shift || named)
    return want_shift == ((mods & wuss_KEY_MOD_SHIFT) != 0);

  return 1;
}

/* Depth-first search of "menu" and its static submenus for the first enabled
 * row whose shortcut matches "key". Rows with wuss_MENU_ITEM_PRE_OPEN are not
 * descended into: their submenu is only settled when the row opens. */
static int find_shortcut(const wuss_menu_t  *menu,
                         const wuss_event_t *key,
                         const wuss_menu_t **found)
{
  int i;

  for (i = 0; i < menu->nitems; i++)
  {
    const wuss_menu_item_t *item;

    item = &menu->items[i];
    if (item->flags & wuss_MENU_ITEM_DISABLED)
      continue;

    if (item->shortcut != NULL && shortcut_matches(item->shortcut, key))
    {
      *found = menu;
      return i;
    }

    if (item->submenu != NULL && !(item->flags & wuss_MENU_ITEM_PRE_OPEN))
    {
      int index;

      index = find_shortcut(item->submenu, key, found);
      if (index >= 0)
        return index;
    }
  }

  return -1;
}

/* ----------------------------------------------------------------------- */

result_t wuss_menu_dispatch_shortcut(wuss_task_t        *task,
                                     const wuss_menu_t  *menu,
                                     const wuss_event_t *key)
{
  const wuss_menu_t *found;
  int                index;
  wuss_event_t       sel;

  assert(task != NULL);
  assert(menu != NULL);
  assert(key != NULL);

  if (key->kind != wuss_EVENT_KEY)
    return result_WUSS_KEY_UNCLAIMED;

  index = find_shortcut(menu, key, &found);
  if (index < 0)
    return result_WUSS_KEY_UNCLAIMED;

  sel.kind                    = wuss_EVENT_MENU_SELECT;
  sel.data.menu_select.menu   = found;
  sel.data.menu_select.index  = index;
  sel.data.menu_select.button = wuss_BUTTON_SELECT;

  return wuss__deliver(task, NULL, &sel);
}
