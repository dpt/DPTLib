/* wuss/menu/menu.c -- wuss pop-up menu helper */

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

#include "wuss/icon.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

#include "../impl.h"

/* Row padding above/below the glyph, and the gutters left for the tick (left)
 * and the submenu arrow (right). All in pixels. */
#define WUSS_MENU_ROW_PAD         4
#define WUSS_MENU_TICK_W         14
#define WUSS_MENU_ARROW_W        14
#define WUSS_MENU_TEXT_PAD        6
#define WUSS_MENU_SUBMENU_OVERLAP 2 /* px a submenu overlaps its parent's right edge */

/* ----------------------------------------------------------------------- */

static result_t wuss__menu_spawn(wuss_t                *wuss,
                                 const wuss_menu_t     *menu,
                                 point_t                at,
                                 struct wuss__menu     *parent,
                                 wuss_menu_select_fn_t *on_select,
                                 void                  *ctx,
                                 struct wuss__menu    **out);

/* Close `node` and everything it opened, leaf-first. The caller clears any
 * parent's child pointer. */
static void wuss__menu_close_from(struct wuss__menu *node)
{
  if (node == NULL)
    return;

  wuss__menu_close_from(node->child);
  node->child = NULL;

  {
    wuss_t *w;

    w = node->wuss;
    if (node->borrowed)
      wuss_window_set_hidden(node->window, 1); /* caller's window: hide, keep */
    else
      wuss_window_close(node->window);
    wuss__free(w, node->icons); /* NULL for a borrowed level */
    wuss__free(w, node);
  }
}

/* Compute where a submenu (or a borrowed window standing in for one) opens
 * off the row `icon` in parent level `self`: content top-left in screen
 * space, the child's own titlebar then sitting above it so the two rows line
 * up. Mirrors the maths in wuss__menu_handle's MOUSE_MOVE branch. */
static point_t wuss__submenu_anchor(struct wuss__menu *self,
                                    const wuss_icon_t *icon)
{
  box_t   wb;
  box_t   ib;
  point_t scroll;
  point_t at;

  wuss_window_get_content_bounds(self->window, &wb);
  wuss_icon_get_bbox(icon, &ib);
  wuss_window_get_scroll(self->window, &scroll);

  at.x = wb.x1 - WUSS_MENU_SUBMENU_OVERLAP;
  at.y = wb.y0 + ib.y0 - scroll.y;
  return at;
}

/* Open item `index`'s borrowed window as level `self->child`: position it
 * where a submenu would appear, un-hide it, bring it to the front. */
static result_t wuss__menu_open_window(struct wuss__menu *self, int index)
{
  struct wuss__menu *node;
  wuss_window_t     *win;
  point_t            at;

  win = self->menu->items[index].window;

  node = wuss__malloc(self->wuss, sizeof(*node));
  if (node == NULL)
    return result_OOM;

  node->wuss       = self->wuss;
  node->window     = win;
  node->menu       = NULL;
  node->icons      = NULL;
  node->parent     = self;
  node->child      = NULL;
  node->on_select  = self->on_select;
  node->ctx        = self->ctx;
  node->open_index = -1;
  node->borrowed   = 1;

  at = wuss__submenu_anchor(self, self->icons[index]);
  wuss_window_move(win, at);
  wuss_window_set_hidden(win, 0);
  wuss_window_restack(win, wuss_ZORDER_FRONT);

  self->child = node;
  return result_OK;
}

/* ----------------------------------------------------------------------- */

/* The menu window's task delegate. task_data is the struct wuss__menu. */
static result_t wuss__menu_handle(wuss_window_t      *window,
                                  const wuss_event_t *event,
                                  void               *task_data)
{
  struct wuss__menu      *self;
  const wuss_icon_t      *icon;
  const wuss_menu_item_t *item;
  int                     index;
  int                     i;

  NOT_USED(window);

  self = task_data;

  if (event->kind != wuss_EVENT_ICON)
    return result_OK;

  icon  = event->data.icon.icon;
  index = -1;
  for (i = 0; i < self->menu->nitems; i++)
  {
    if (self->icons[i] == icon)
    {
      index = i;
      break;
    }
  }
  if (index < 0)
    return result_OK;

  item = &self->menu->items[index];

  if (item->flags & wuss_MENU_ITEM_DISABLED)
    return result_OK;

  if (event->data.icon.action == wuss_MOUSE_MOVE)
  {
    point_t at;

    /* Hovering a different row closes any submenu the previous row opened. */
    if (self->child != NULL && self->open_index != index)
    {
      wuss__menu_close_from(self->child);
      self->child      = NULL;
      self->open_index = -1;
    }

    if (self->child != NULL)
      return result_OK;

    /* A borrowed window opens where a submenu would; a submenu spawns as a
     * fresh menu level. Either lines its row 0 up with this row -- the
     * child's own titlebar sits above `at`. */
    if (item->window != NULL)
    {
      if (wuss__menu_open_window(self, index) == result_OK)
        self->open_index = index;
      return result_OK;
    }

    if (item->submenu == NULL)
      return result_OK;

    at = wuss__submenu_anchor(self, icon);
    if (wuss__menu_spawn(self->wuss, item->submenu, at, self,
                         self->on_select, self->ctx, &self->child) == result_OK)
      self->open_index = index;

    return result_OK;
  }

  if (event->data.icon.action == wuss_MOUSE_UP)
  {
    wuss_button_t          button;
    struct wuss__menu     *root;
    wuss_menu_select_fn_t *on_select;
    const wuss_menu_t     *menu;
    void                  *ctx;

    button = event->data.icon.button;

    if (item->submenu != NULL || item->window != NULL)
      return result_OK; /* a submenu/window row opens on hover, not a pick */

    /* Capture what the callback needs, then tear the chain down *before*
     * invoking it: on_select may itself open a new menu (freeing this one),
     * so `self` must not be touched afterwards. */
    on_select = self->on_select;
    menu      = self->menu;
    ctx       = self->ctx;

    if (!(button & wuss_BUTTON_ADJUST))
    {
      root = self;
      while (root->parent != NULL)
        root = root->parent;

      root->wuss->menu_chain = NULL;
      wuss__menu_close_from(root);
    }

    if (on_select != NULL)
      on_select(menu, index, button, ctx);
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

/* Measure the menu, create its borderless window and one MENU_ENTRY icon per
 * item, and hang the node off *out. */
static result_t wuss__menu_spawn(wuss_t                *wuss,
                                 const wuss_menu_t     *menu,
                                 point_t                at,
                                 struct wuss__menu     *parent,
                                 wuss_menu_select_fn_t *on_select,
                                 void                  *ctx,
                                 struct wuss__menu    **out)
{
  struct wuss__menu *node;
  wuss_icon_spec_t  *specs;
  wuss_icon_t      **made;
  wuss_task_t        task;
  result_t           rc;
  int                fh;
  int                pitch;
  int                sep_h;
  int                y;
  int                widest;
  int                width, height;
  int                doc_h;
  int                max_h;
  int                i;
  int                ndashed;
  int                nspecs;
  int                s;
  int                outline_px;
  int                titlebar_height;
  point_t            carve;
  wuss_window_flags_t menu_flags;
  size2d_t           doc;
  size2d_t           min_doc;
  box_t              content;

  assert(wuss != NULL);
  assert(menu != NULL);
  assert(wuss->font != NULL);

  if (menu->nitems <= 0)
    return result_WUSS_BAD_ICON;

  menu_flags = wuss_WINDOW_NO_CLOSE | wuss_WINDOW_NO_BACK
             | wuss_WINDOW_NO_TOGGLE_SIZE
             | wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL
             | wuss_WINDOW_NO_RESIZE;

  outline_px      = wuss__outline_px_for(menu_flags);
  titlebar_height = wuss__titlebar_height_for(wuss, menu_flags);

  bmfont_get_info(wuss->font, NULL, &fh);
  pitch = fh + 2 * WUSS_MENU_ROW_PAD;
  sep_h = 2 * WUSS_MENU_ROW_PAD;

  ndashed = 0;
  for (i = 0; i < menu->nitems; i++)
    if (menu->items[i].flags & wuss_MENU_ITEM_DASHED)
      ndashed++;
  nspecs = menu->nitems + ndashed;

  widest = 0;
  for (i = 0; i < menu->nitems; i++)
  {
    const char    *text;
    int            len;
    int            split;
    bmfont_width_t w;

    text = menu->items[i].text ? menu->items[i].text : "";
    len  = (int) strlen(text);
    if (len == 0)
      continue; /* empty label (bare rule row); nothing to measure */
    if (bmfont_measure(wuss->font, text, len,
                       INT_MAX, &split, &w) == result_OK && (int) w > widest)
      widest = (int) w;
  }

  width  = WUSS_MENU_TICK_W + widest + WUSS_MENU_TEXT_PAD + WUSS_MENU_ARROW_W;

  /* every item is a full row now; a dashed item also gets a sep_h rule above.
   * doc_h is the whole menu; `height` is what the window actually shows. When
   * the menu is taller than the space the screen leaves for it, cap `height`
   * and give the window a real vertical scrollbar -- the icon draw and hit
   * test already honour window->scroll, so the rows just move under it. */
  doc_h = menu->nitems * pitch + ndashed * sep_h;

  max_h = wuss->scr->size.h - 2 * outline_px - titlebar_height;

  if (doc_h > max_h)
  {
    height      = max_h;
    menu_flags &= (wuss_window_flags_t) ~wuss_WINDOW_NO_VSCROLL;
  }
  else
  {
    height = doc_h;
  }

  wuss__furniture_carve_for(menu_flags, wuss__button_size_for(wuss, menu_flags),
                            &carve);

  /* Nudge onto the screen where it can be, but keep the top-left on screen.
   * `at` is the content top-left; the window's visible box also spans the
   * outline on all four sides, the titlebar above and (for a scrolling menu)
   * the scrollbar carve on the right, so clamp against that whole extent --
   * otherwise wuss_window_create's own on-screen size clamp trims the content
   * and the last row (e.g. a trailing "Quit") is cropped. */
  if (at.x + width + outline_px + carve.x > wuss->scr->size.w)
    at.x = wuss->scr->size.w - width - outline_px - carve.x;
  if (at.y + height + outline_px > wuss->scr->size.h)
    at.y = wuss->scr->size.h - height - outline_px;
  if (at.x - outline_px < 0)
    at.x = outline_px;
  if (at.y - outline_px - titlebar_height < 0)
    at.y = outline_px + titlebar_height;

  node = wuss__malloc(wuss, sizeof(*node));
  if (node == NULL)
    return result_OOM;

  node->icons = wuss__malloc(wuss, (size_t) menu->nitems * sizeof(*node->icons));
  specs       = wuss__malloc(wuss, (size_t) nspecs * sizeof(*specs));
  made        = wuss__malloc(wuss, (size_t) nspecs * sizeof(*made));
  if (node->icons == NULL || specs == NULL || made == NULL)
  {
    wuss__free(wuss, made);
    wuss__free(wuss, specs);
    wuss__free(wuss, node->icons);
    wuss__free(wuss, node);
    return result_OOM;
  }
  memset(node->icons, 0, (size_t) menu->nitems * sizeof(*node->icons));

  node->wuss       = wuss;
  node->window     = NULL;
  node->menu       = menu;
  node->parent     = parent;
  node->child      = NULL;
  node->on_select  = on_select;
  node->ctx        = ctx;
  node->open_index = -1;
  node->borrowed   = 0;

  /* One MENU_ENTRY icon per item, in item order, preceded by an inert
   * wuss_ICON_TYPE_RULE icon for each dashed item. specs[] therefore holds
   * nspecs entries; s walks it while i walks the items. */
  y = 0;
  s = 0;
  for (i = 0; i < menu->nitems; i++)
  {
    const wuss_menu_item_t *item;
    wuss_icon_flags_t       flags;

    item  = &menu->items[i];
    flags = wuss_ICON_FLAGS_NONE;

    if (item->flags & wuss_MENU_ITEM_DASHED)
    {
      flags |= wuss_ICON_FLAGS_SEPARATOR;

      specs[s].type    = wuss_ICON_TYPE_RULE;
      specs[s].bbox.x0 = 0;
      specs[s].bbox.y0 = y;
      specs[s].bbox.x1 = width;
      specs[s].bbox.y1 = y + sep_h;
      specs[s].text    = "";
      specs[s].fg      = wuss->palettecache.black;
      specs[s].bg      = wuss_NO_BACKGROUND;
      specs[s].flags   = wuss_ICON_FLAGS_NONE;
      s++;
      y += sep_h;
    }

    if (item->flags & wuss_MENU_ITEM_DISABLED)
      flags |= wuss_ICON_FLAGS_DISABLED;
    if (item->submenu != NULL || item->window != NULL)
      flags |= wuss_ICON_FLAGS_SUBMENU;

    specs[s].type    = wuss_ICON_TYPE_MENU_ENTRY;
    specs[s].bbox.x0 = 0;
    specs[s].bbox.y0 = y;
    specs[s].bbox.x1 = width;
    specs[s].bbox.y1 = y + pitch;
    specs[s].text    = item->text ? item->text : "";
    specs[s].fg      = wuss->palettecache.black;
    specs[s].bg      = wuss_NO_BACKGROUND;
    specs[s].flags   = flags;
    s++;

    y += pitch;
  }

  /* doc spans the whole menu so the window can scroll it; min_doc is what the
   * window shows, so its own resize floor never exceeds what fits. */
  doc     = SIZE2D(width, doc_h);
  min_doc = SIZE2D(width, height);
  task    = wuss_task_start(wuss__menu_handle, node);

  /* `at` is the content top-left, already clamped on screen above. Create the
   * window there directly -- creating it elsewhere and wuss_window_move-ing it
   * afterwards would blit its not-yet-rendered pixels and leave the titlebar
   * unpainted. */
  content.x0 = at.x;
  content.y0 = at.y;
  content.x1 = at.x + width;
  content.y1 = at.y + height;

  rc = wuss_window_create(wuss, &content,
                          menu->title ? menu->title : "",
                          menu_flags,
                          wuss_BACKDROP_COLOUR(wuss->palettecache.white),
                          &task, doc, min_doc, &node->window);
  if (rc != result_OK)
  {
    wuss__free(wuss, made);
    wuss__free(wuss, specs);
    wuss__free(wuss, node->icons);
    wuss__free(wuss, node);
    return rc;
  }

  rc = wuss_icon_create_array(node->window, specs, nspecs, made);
  wuss__free(wuss, specs);
  if (rc != result_OK)
  {
    wuss__free(wuss, made);
    wuss_window_close(node->window);
    wuss__free(wuss, node->icons);
    wuss__free(wuss, node);
    return rc;
  }

  /* keep only the entry handles, item-indexed; the rule icons stay owned by
   * the window and need no further handling. Walk made[] with the same
   * rule-then-entry interleave as the specs loop. */
  s = 0;
  for (i = 0; i < menu->nitems; i++)
  {
    if (menu->items[i].flags & wuss_MENU_ITEM_DASHED)
      s++;
    node->icons[i] = made[s];
    s++;

    if (menu->items[i].flags & wuss_MENU_ITEM_TICKED)
      wuss_icon_set_selected(node->icons[i], 1);
  }
  wuss__free(wuss, made);

  *out = node;
  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t wuss_menu_open(wuss_t                *wuss,
                        const wuss_menu_t     *menu,
                        point_t                at,
                        wuss_menu_select_fn_t *on_select,
                        void                  *ctx,
                        wuss_menu_handle_t    *out)
{
  struct wuss__menu *root;
  result_t           rc;

  assert(wuss != NULL);
  assert(menu != NULL);

  if (wuss->menu_chain != NULL)
  {
    wuss__menu_close_from(wuss->menu_chain);
    wuss->menu_chain = NULL;
  }

  /* RISC OS convention: the pointer opens the menu sitting a little inside its
   * first item, not on the top-left corner. Shift the content top-left up and
   * left so `at` (the pointer) lands over row 0. */
  at.x -= WUSS_MENU_TICK_W;
  at.y -= WUSS_MENU_ROW_PAD;

  rc = wuss__menu_spawn(wuss, menu, at, NULL, on_select, ctx, &root);
  if (rc != result_OK)
    return rc;

  wuss->menu_chain = root;

  /* A menu is opened from a task's MOUSE_DOWN handler; the matching MOUSE_UP is
   * still to come and would land on the fresh menu's row 0. Mark it to be
   * eaten. wuss_mouse_click clears this on the next MOUSE_UP whether or not it
   * hit the menu. */
  wuss->menu_eat_up = 1;

  if (out != NULL)
    *out = root;
  return result_OK;
}

void wuss_menu_close(wuss_menu_handle_t handle)
{
  struct wuss__menu *root;

  if (handle == NULL)
    return;

  root = handle;
  while (root->parent != NULL)
    root = root->parent;

  if (root->wuss->menu_chain != root)
    return; /* stale handle */

  root->wuss->menu_chain = NULL;
  wuss__menu_close_from(root);
}

int wuss_menu_is_open(wuss_menu_handle_t handle)
{
  struct wuss__menu *root;

  if (handle == NULL)
    return 0;

  root = handle;
  while (root->parent != NULL)
    root = root->parent;

  return root->wuss->menu_chain == root;
}

/* ----------------------------------------------------------------------- */

int wuss__menu_click_outside(wuss_t *wuss, const wuss_window_t *hit)
{
  struct wuss__menu *node;

  if (wuss->menu_chain == NULL)
    return 0;

  for (node = wuss->menu_chain; node != NULL; node = node->child)
  {
    if (node->window == hit)
      return 0;
  }

  wuss__menu_close_from(wuss->menu_chain);
  wuss->menu_chain = NULL;
  return 1;
}
