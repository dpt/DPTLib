/* wuss/menu/menu.c -- wuss pop-up menu helper */

#include <assert.h>
#include <limits.h>
#include <stddef.h>
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

#include "../core/impl.h"

/* Row padding above/below the glyph, and the gutters left for the tick (left)
 * and the submenu arrow (right). All in pixels. */
#define WUSS_MENU_ROW_PAD         4
#define WUSS_MENU_GUTTER_LEFT    14
#define WUSS_MENU_GUTTER_RIGHT   14
#define WUSS_MENU_TITLE_PAD       2 /* margin either side of the titlebar caption slot, matching wuss__titlebar_draw */
#define WUSS_MENU_SUBMENU_OVERLAP 2 /* px a submenu overlaps its parent's right edge */

/* SELECT-pick flash: toggle the row highlight every WUSS_MENU_FLASH_PERIOD
 * IDLE frames, for WUSS_MENU_FLASH_FRAMES frames total. At ~60 Hz that is
 * three quick on/off blinks in about 0.2s -- the RISC OS feel. */
#define WUSS_MENU_FLASH_FRAMES  12
#define WUSS_MENU_FLASH_PERIOD  2

/* ----------------------------------------------------------------------- */

static result_t wuss__menu_handle(wuss_window_t      *window,
                                  const wuss_event_t *event,
                                  void               *task_data);

static result_t wuss__menu_spawn(wuss_t             *wuss,
                                 wuss_task_t        *owner,
                                 const wuss_menu_t  *menu,
                                 point_t             at,
                                 struct wuss__menu  *parent,
                                 struct wuss__menu **out);

static void wuss__menu_flash_step(struct wuss__menu *self);
static void wuss__menu_flash_finish(struct wuss__menu *self);

static int wuss__pointer_over_row_arrow(wuss_window_t     *window,
                                        const wuss_icon_t *icon);

/* The internal task that owns every borderless menu window, created on the
 * first wuss_menu_open of a session. */
static wuss_task_t *wuss__menu_task(wuss_t *wuss)
{
  wuss_task_desc_t desc;

  if (wuss->menu_task != NULL)
    return wuss->menu_task;

  desc.handle    = wuss__menu_handle;
  desc.task_data = wuss; /* the ICON path walks the chain by window; the IDLE
                          * path (window == NULL) needs the wuss_t directly */
  desc.name      = "wuss:menu";

  if (wuss_task_create(wuss, &desc, &wuss->menu_task) != result_OK)
    return NULL;

  return wuss->menu_task;
}

/* Find the open chain node whose window is `window` (the menu window whose
 * delegate fired), or NULL. Borrowed-window levels never fire this handler
 * (their windows keep the caller's task), so a match is always a real menu
 * level with icons. */
static struct wuss__menu *wuss__menu_node_for(wuss_t              *wuss,
                                              const wuss_window_t *window)
{
  struct wuss__menu *node;

  for (node = wuss->menu_chain; node != NULL; node = node->child)
    if (node->window == window)
      return node;

  return NULL;
}

/* Close `node` and everything it opened, leaf-first. The caller clears any
 * parent's child pointer. */
static void wuss__menu_close_from(struct wuss__menu *node)
{
  if (node == NULL)
    return;

  wuss__menu_close_from(node->child);
  node->child = NULL;

  /* A pending pick flash still owes its owner a MENU_SELECT; the chain is
   * about to be freed out from under it (the caller unlinks it from
   * wuss->menu_chain first), so delivering the notification here -- rather
   * than via wuss__menu_flash_finish, whose own teardown would race this
   * one -- is the only chance the pick has of reaching its owner. */
  if (node->flash.frames > 0)
  {
    wuss_event_t sel;

    node->flash.frames        = 0;
    sel.kind                    = wuss_EVENT_MENU_SELECT;
    sel.data.menu_select.menu   = node->flash.menu;
    sel.data.menu_select.index  = node->flash.index;
    sel.data.menu_select.button = node->flash.button;
    (void) wuss__deliver(node->flash.owner, NULL, &sel);
  }

  {
    wuss_t *w;

    w = node->wuss;
    if (node->flags & wuss_MENU__BORROWED)
      wuss_window_set_hidden(node->window, 1); /* caller's window: hide, keep */
    else
      wuss_window_close(node->window);
    wuss__free(w, node->icons); /* NULL for a borrowed level */
    wuss__free(w, node);
  }
}

/* Tear the whole open chain down because wuss decided to, not the client: a
 * click outside every menu window, or another wuss_menu_open. The task that
 * opened it still holds the handle wuss_menu_open handed back, so tell it the
 * chain is gone (wuss_EVENT_MENU_CLOSED) before the nodes are freed -- a pick
 * has wuss_EVENT_MENU_SELECT for that, but these paths have nothing. Unlinks
 * wuss->menu_chain first so a re-entrant wuss_menu_close from the handler is a
 * no-op. Also used by wuss_task_destroy when the chain's owner is the task
 * going away, so declared in core impl.h. */
void wuss__menu_abandon(wuss_t *wuss)
{
  struct wuss__menu *root;
  wuss_task_t       *owner;

  root = wuss->menu_chain;
  if (root == NULL)
    return;

  owner            = root->owner;
  wuss->menu_chain = NULL;

  if (owner != NULL)
  {
    wuss_event_t ev;

    ev.kind = wuss_EVENT_MENU_CLOSED;
    (void) wuss__deliver(owner, NULL, &ev);
  }

  wuss__menu_close_from(root);
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

/* Allocate and link a borrowed-window child level onto self->child, then
 * bring its window to front. Shared by wuss_menu_open_window_now (the
 * flagged row's explicit opt-in) and wuss__menu_open_window's direct-open
 * path for an unflagged row. */
static result_t wuss__menu_link_borrowed(struct wuss__menu *self,
                                         wuss_window_t     *win)
{
  struct wuss__menu *node;

  node = wuss__malloc(self->wuss, sizeof(*node));
  if (node == NULL)
    return result_OOM;

  node->flags         = wuss_MENU__BORROWED;
  node->wuss          = self->wuss;
  node->owner         = self->owner;
  node->window        = win;
  node->menu          = NULL;
  node->icons         = NULL;
  node->parent        = self;
  node->child         = NULL;
  node->open_index    = -1;
  node->pending_index = -1;
  memset(&node->flash, 0, sizeof(node->flash));

  self->child = node;

  wuss_window_restack(win, wuss_ZORDER_FRONT);

  return result_OK;
}

/* Open item `index`'s borrowed window as level `self->child`: position it
 * where a submenu would appear, then reveal it. Unflagged
 * (wuss_MENU_ITEM_PRE_OPEN unset), this reveals and links directly with no
 * event fired. Flagged, wuss_window_set_hidden fires wuss_EVENT_PRE_SHOW to
 * the window's own task with `self` and `index` in the payload, and the
 * handler must call wuss_menu_open_window_now to proceed and link the
 * child level -- not calling it leaves the row inert. */
static result_t wuss__menu_open_window(struct wuss__menu *self, int index)
{
  result_t                rc;
  const wuss_menu_item_t *item;
  wuss_window_t           *win;
  point_t                  at;

  item = &self->menu->items[index];
  win  = item->window;

  at = wuss__submenu_anchor(self, self->icons[index]);
  wuss_window_move(win, at);
  /* the anchor sits off the parent's right edge; if the parent menu is near
   * the screen edge that puts the window (partly) off-screen. wuss_window_move
   * does not clamp -- drags rely on it not clamping -- so pull it back here. */
  wuss__nudge_visible_onscreen(win);

  if (!(item->flags & wuss_MENU_ITEM_PRE_OPEN))
  {
    rc = wuss_window_set_hidden(win, 0);
    if (rc != result_OK)
      return rc;

    if (self->child == NULL && !(win->flags & wuss_WINDOW_HIDDEN))
      return wuss__menu_link_borrowed(self, win);

    return result_OK;
  }

  self->pending_index = index;
  rc                  = wuss__window_set_hidden_ex(win, 0, self, index);
  self->pending_index = -1;

  return rc;
}

/* Called back from wuss_EVENT_PRE_SHOW, synchronously, to opt in to
 * revealing `index`'s borrowed window: tells wuss__window_set_hidden_ex to
 * proceed with the actual flag-flip/SHOW once this handler returns, and
 * links the child level. Positioning has already been done by
 * wuss__menu_open_window before PRE_SHOW was fired. */
result_t wuss_menu_open_window_now(wuss_menu_handle_t handle, int index)
{
  struct wuss__menu *self;
  wuss_window_t     *win;

  self = handle;

  assert(self != NULL);
  assert(self->pending_index == index);

  win = self->menu->items[index].window;

  self->wuss->pre_show_proceed = 1;

  return wuss__menu_link_borrowed(self, win);
}

/* Called back from wuss_EVENT_PRE_SUBMENU_OPEN, synchronously, to opt in to
 * opening `index`'s submenu: spawns `menu` (the item's own submenu, or
 * another one retitled/retargeted for this row) as the child level, anchored
 * off the row's arrow exactly as a plain submenu would be. */
result_t wuss_menu_open_submenu_now(wuss_menu_handle_t handle,
                                    int                index,
                                    const wuss_menu_t *menu)
{
  result_t           rc;
  struct wuss__menu *self;
  point_t            at;

  self = handle;

  assert(self != NULL);
  assert(self->pending_index == index);

  at = wuss__submenu_anchor(self, self->icons[index]);

  rc = wuss__menu_spawn(self->wuss, self->owner, menu, at, self, &self->child);
  if (rc == result_OK)
    self->open_index = index;

  return rc;
}

/* True if the wuss pointer sits over `window`'s chrome above its content area
 * -- the titlebar strip -- and `window` is the frontmost window there. Core
 * routes furniture hovers straight to the window manager and delivers no event
 * to the menu delegate (see wuss_mouse_move), so a titlebar hover on a parent
 * level cannot be caught in the ICON handler; the IDLE tick polls this instead
 * to close a submenu the pointer has slid up onto the parent's titlebar to
 * reach (e.g. to drag the parent). The frontmost check stops a child menu
 * dragged back over its parent's titlebar -- the child window is then on top
 * at that point -- from being read as a parent-titlebar hover and closed. */
static int wuss__pointer_over_titlebar(const wuss_window_t *window)
{
  point_t p;
  box_t   content;

  p = wuss_get_pointer(window->wuss);
  wuss__content_box(window, &content);

  return box_contains_point(&window->visible, p.x, p.y)
      && p.y < content.y0
      && wuss__window_at(window->wuss, p) == window;
}

/* ----------------------------------------------------------------------- */

/* The menu window's task delegate. Shared across every borderless menu
 * window; the firing level is located by its window. */
static result_t wuss__menu_handle(wuss_window_t      *window,
                                  const wuss_event_t *event,
                                  void               *task_data)
{
  struct wuss__menu      *self;
  const wuss_icon_t      *icon;
  const wuss_menu_item_t *item;
  wuss_t                 *wuss;
  int                     index;
  int                     i;

  /* IDLE arrives with window == NULL; task_data is the wuss_t (see
   * wuss__menu_task). Drive any running SELECT-pick flash off it, and close a
   * submenu the pointer has slid up onto its parent's titlebar. */
  if (event->kind == wuss_EVENT_IDLE)
  {
    struct wuss__menu *node;

    wuss = task_data;

    for (node = wuss->menu_chain; node != NULL; node = node->child)
      if (!(node->flags & wuss_MENU__BORROWED) && node->flash.frames > 0)
      {
        wuss__menu_flash_step(node);
        break; /* node may be freed; the chain is gone if the flash ended */
      }

    for (node = wuss->menu_chain; node != NULL; node = node->child)
      if (node->child != NULL
          && node->flash.frames == 0
          && wuss__pointer_over_titlebar(node->window))
      {
        wuss_icon_t *was_parent;

        was_parent = (node->open_index >= 0) ? node->icons[node->open_index]
                                             : NULL;

        wuss__menu_close_from(node->child);
        node->child      = NULL;
        node->open_index = -1;

        if (was_parent != NULL)
        {
          wuss__icon_set_state(was_parent, wuss_ICON_STATE_HOVERED, 0);
          wuss__icon_invalidate(node->window, was_parent);
        }
        break; /* closing the child re-shaped the chain past this node */
      }

    return result_OK;
  }

  if (event->kind != wuss_EVENT_ICON)
    return result_OK;

  wuss = window->wuss;
  self = wuss__menu_node_for(wuss, window);
  if (self == NULL)
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

  if (event->data.icon.action == wuss_MOUSE_MOVE)
  {
    int has_child_row;
    int on_arrow;

    /* A disabled row still has to run the close-on-move-away logic below --
     * hovering off an open submenu onto a disabled sibling must collapse it
     * -- but can never itself open a submenu. */
    has_child_row = !(item->flags & wuss_MENU_ITEM_DISABLED)
                 && (item->submenu != NULL || item->window != NULL);
    on_arrow      = has_child_row
                 && wuss__pointer_over_row_arrow(self->window, icon);

    /* A submenu opens only while the pointer is over its row's arrow. Any
     * other move that re-enters this level -- onto a different row, or onto
     * this row's text off the arrow -- closes the child it opened. */
    if (self->child != NULL && !(self->open_index == index && on_arrow))
    {
      wuss_icon_t *was_parent;

      was_parent = (self->open_index >= 0) ? self->icons[self->open_index]
                                           : NULL;

      wuss__menu_close_from(self->child);
      self->child      = NULL;
      self->open_index = -1;

      /* The ex-parent row was held highlit while its submenu was open (see
       * wuss__menu_row_pinned). Now the submenu is gone, drop that highlight
       * unless the pointer has landed back on that very row. */
      if (was_parent != NULL && was_parent != icon)
      {
        wuss__icon_set_state(was_parent, wuss_ICON_STATE_HOVERED, 0);
        wuss__icon_invalidate(self->window, was_parent);
      }
    }

    if (self->child != NULL)
      return result_OK; /* still on the open row's arrow: leave it up */

    if (!on_arrow)
      return result_OK;

    /* A borrowed window opens where a submenu would; a submenu spawns as a
     * fresh menu level. Either lines its row 0 up with this row -- the
     * child's own titlebar sits above the row's arrow. */
    if (item->window != NULL)
    {
      if (wuss__menu_open_window(self, index) == result_OK && self->child != NULL)
        self->open_index = index;
      return result_OK;
    }

    if (!(item->flags & wuss_MENU_ITEM_PRE_OPEN))
    {
      /* unflagged: no event, just open the row's own static submenu */
      point_t at;

      at = wuss__submenu_anchor(self, self->icons[index]);
      if (wuss__menu_spawn(self->wuss, self->owner, item->submenu, at,
                           self, &self->child) == result_OK)
        self->open_index = index;

      return result_OK;
    }

    {
      result_t     rc;
      wuss_event_t pre_open;

      self->pending_index                = index;
      pre_open.kind                      = wuss_EVENT_PRE_SUBMENU_OPEN;
      pre_open.data.pre_submenu_open.handle = self;
      pre_open.data.pre_submenu_open.index  = index;
      rc = wuss__deliver(self->owner, NULL, &pre_open);
      self->pending_index = -1;
      if (rc != result_OK)
        return rc;

      /* flagged but the handler did not call wuss_menu_open_submenu_now:
       * row stays inert */
    }

    return result_OK;
  }

  if (event->data.icon.action == wuss_MOUSE_UP)
  {
    wuss_button_t      button;
    wuss_task_t       *owner;
    const wuss_menu_t *menu;

    if (item->flags & wuss_MENU_ITEM_DISABLED)
      return result_OK;

    button = event->data.icon.button;

    if (!(button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      return result_OK; /* MENU release picks nothing -- core never arms the
                         * row's press for it, so this UP is unpaired */

    if (item->submenu != NULL || item->window != NULL)
      return result_OK; /* a submenu/window row opens on hover, not a pick */

    /* owner (the task that opened the chain) is copied onto every level */
    owner = self->owner;
    menu  = self->menu;

    /* Flash the picked row, then deliver MENU_SELECT from the IDLE handler.
     * SELECT tears the whole chain down first; ADJUST keeps it open so the
     * row can be re-picked. Capture everything the notification needs now --
     * a SELECT flash frees `self` when it ends. A re-pick while a flash on
     * this level is still running (fast clicks) finishes that flash first so
     * its row is cleared and its MENU_SELECT is not lost; if that previous
     * pick was a SELECT the finish frees the whole chain (including `self`),
     * so this pick is spent on it and we must not touch `self` again. */
    {
      wuss_icon_t *row;

      if (self->flash.frames > 0)
      {
        wuss__menu_flash_finish(self);

        /* the finish's own SELECT tears the chain down when it wasn't a
         * keep_open pick, and even a keep_open pick's MENU_SELECT delivery
         * may re-enter and close the chain from the client side (e.g. the
         * owner calling wuss_menu_close()) -- either way `self` is gone the
         * moment the chain no longer starts at it */
        if (wuss__menu_node_for(wuss, window) != self)
          return result_OK; /* `self` and the chain are gone */
      }

      row = self->icons[index];

      self->flash.frames = WUSS_MENU_FLASH_FRAMES;
      self->flash.index  = index;
      self->flash.owner  = owner;
      self->flash.menu   = menu;
      self->flash.button = button;
      if (button & wuss_BUTTON_ADJUST)
        self->flags |= wuss_MENU__FLASH_KEEP_OPEN;
      else
        self->flags &= ~wuss_MENU__FLASH_KEEP_OPEN;

      /* first blink now: the row is already highlit under the pointer, so
       * drop the highlight this frame for an immediate visible change */
      wuss__icon_set_state(row, wuss_ICON_STATE_HOVERED, 0);
      wuss__icon_invalidate(self->window, row);
    }
    return result_OK;
  }

  return result_OK;
}

/* True if the wuss pointer currently sits over icon `icon` in `window`. */
static int wuss__pointer_over_icon(wuss_window_t     *window,
                                   const wuss_icon_t *icon)
{
  point_t p;
  box_t   content;
  point_t doc;

  p = wuss_get_pointer(window->wuss);
  wuss__content_box(window, &content);
  doc.x = p.x - content.x0 + window->scroll.x;
  doc.y = p.y - content.y0 + window->scroll.y;

  return wuss__icon_hit_test(window, doc) == icon;
}

/* True if the wuss pointer sits over the submenu-arrow gutter of row `icon`
 * in `window`: the pointer is within the row vertically and inside the
 * rightmost WUSS_MENU_GUTTER_RIGHT pixels of it. A submenu opens only from here,
 * so re-entering the parent anywhere else closes the child. */
static int wuss__pointer_over_row_arrow(wuss_window_t     *window,
                                        const wuss_icon_t *icon)
{
  point_t p;
  box_t   content;
  box_t   bbox;
  point_t doc;

  p = wuss_get_pointer(window->wuss);
  wuss__content_box(window, &content);
  doc.x = p.x - content.x0 + window->scroll.x;
  doc.y = p.y - content.y0 + window->scroll.y;

  wuss_icon_get_bbox(icon, &bbox);

  return doc.y >= bbox.y0 && doc.y < bbox.y1
      && doc.x >= bbox.x1 - WUSS_MENU_GUTTER_RIGHT && doc.x < bbox.x1;
}

/* End the pick flash on `self` now: leave the flashed row un-highlit unless the
 * pointer is still over it (on an ADJUST re-pick no MOUSE_MOVE follows to bring
 * the highlight back), then deliver the MENU_SELECT the flash stands in for --
 * tearing the chain down first unless the pick was an ADJUST (keep_open). On a
 * non-keep_open return `self` has been freed. Callers guard against calling
 * this with no flash armed. */
static void wuss__menu_flash_finish(struct wuss__menu *self)
{
  struct wuss__menu *root;
  wuss_event_t       sel;
  wuss_icon_t       *row       = self->icons[self->flash.index];
  wuss_task_t       *owner     = self->flash.owner;
  const wuss_menu_t *menu      = self->flash.menu;
  int                index     = self->flash.index;
  wuss_button_t      button    = self->flash.button;
  int                keep_open = (self->flags & wuss_MENU__FLASH_KEEP_OPEN) != 0;
  int                on;

  self->flash.frames = 0;

  on = keep_open && wuss__pointer_over_icon(self->window, row);
  wuss__icon_set_state(row, wuss_ICON_STATE_HOVERED, on);
  wuss__icon_invalidate(self->window, row);

  if (!keep_open)
  {
    root = self;
    while (root->parent != NULL)
      root = root->parent;

    root->wuss->menu_chain = NULL;
    wuss__menu_close_from(root); /* frees `self` */
  }

  sel.kind                    = wuss_EVENT_MENU_SELECT;
  sel.data.menu_select.menu   = menu;
  sel.data.menu_select.index  = index;
  sel.data.menu_select.button = button;
  (void) wuss__deliver(owner, NULL, &sel);
}

/* Advance a running pick flash by one IDLE frame; hand off to
 * wuss__menu_flash_finish when it runs out. `self` must be a real
 * (non-borrowed) level. */
static void wuss__menu_flash_step(struct wuss__menu *self)
{
  wuss_icon_t *row;

  if (self->flash.frames <= 0)
    return;

  row = self->icons[self->flash.index];

  self->flash.frames--;

  if (self->flash.frames == 0)
  {
    wuss__menu_flash_finish(self); /* delivers MENU_SELECT; may free `self` */
    return;
  }

  /* toggle the highlight at each period boundary; the pick frame already did
   * the first (off) blink, so phase here starts the row coming back on */
  if (self->flash.frames % WUSS_MENU_FLASH_PERIOD == 0)
  {
    int on = ((self->flash.frames / WUSS_MENU_FLASH_PERIOD) & 1) == 0;

    wuss__icon_set_state(row, wuss_ICON_STATE_HOVERED, on);
    wuss__icon_invalidate(self->window, row);
  }
}

/* ----------------------------------------------------------------------- */

/* Measure the menu, create its borderless window and one MENU_ENTRY icon per
 * item, and hang the node off *out. */
static result_t wuss__menu_spawn(wuss_t             *wuss,
                                 wuss_task_t        *owner,
                                 const wuss_menu_t  *menu,
                                 point_t             at,
                                 struct wuss__menu  *parent,
                                 struct wuss__menu **out)
{
  result_t           rc;
  struct wuss__menu *node;
  wuss_task_t       *menu_task;
  wuss_icon_spec_t  *specs;
  wuss_icon_t      **made;
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
  assert(wuss->fonts.fonts[0] != NULL);

  if (menu->nitems <= 0)
    return result_WUSS_BAD_ICON;

  menu_task = wuss__menu_task(wuss);
  if (menu_task == NULL)
    return result_OOM;

  menu_flags = 0;

  outline_px      = wuss__outline_px_for(menu_flags);
  titlebar_height = wuss__titlebar_height_for(wuss, menu_flags);

  bmfont_get_info(wuss->fonts.fonts[0], NULL, &fh, NULL, NULL);
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
    if (wuss__text_measure(wuss->fonts.fonts[0], text, len,
                           INT_MAX, &split, &w) == result_OK && (int) w > widest)
      widest = (int) w;
  }

  /* the row draws its text as if a space padded it either side (see
   * wuss__icon_draw_menu_entry), so the text column must be wide enough
   * for two of those */
  {
    bmfont_width_t space_w = 0;

    wuss__text_measure(wuss->fonts.fonts[0], " ", 1, INT_MAX, NULL, &space_w);
    widest += 2 * (int) space_w;
  }

  width  = WUSS_MENU_GUTTER_LEFT + widest + WUSS_MENU_GUTTER_RIGHT;

  /* widen for the titlebar caption too, so a title longer than every item
   * label (e.g. a one-item menu) isn't clipped; titles draw in the bold
   * weight (font slot 1), falling back to the system font, same as
   * wuss__titlebar_draw */
  if (menu->title != NULL && menu->title[0] != '\0')
  {
    bmfont_t      *titlefont;
    int            titlelen;
    int            split;
    bmfont_width_t title_w;

    titlefont = (wuss->fonts.nfonts > 1 && wuss->fonts.fonts[1] != NULL)
              ? wuss->fonts.fonts[1]
              : wuss->fonts.fonts[0];
    titlelen  = (int) strlen(menu->title);
    if (wuss__text_measure(titlefont, menu->title, titlelen, INT_MAX, &split,
                           &title_w) == result_OK &&
        (int) title_w + 2 * WUSS_MENU_TITLE_PAD > width)
      width = (int) title_w + 2 * WUSS_MENU_TITLE_PAD;
  }

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
    menu_flags |= wuss_WINDOW_VSCROLL;
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

  node->flags      = 0;
  node->wuss       = wuss;
  node->owner      = owner;
  node->window     = NULL;
  node->menu       = menu;
  node->parent     = parent;
  node->child      = NULL;
  node->open_index = -1;
  memset(&node->flash, 0, sizeof(node->flash));

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
      specs[s].fg      = wuss_COLOUR_BLACK;
      specs[s].bg      = wuss_NO_BACKGROUND;
      specs[s].u.menu_entry.swatch = wuss_NO_BACKGROUND;
      specs[s].flags   = wuss_ICON_FLAGS_NONE;
      s++;
      y += sep_h;
    }

    if (item->flags & wuss_MENU_ITEM_DISABLED)
      flags |= wuss_ICON_FLAGS_DISABLED;
    if (item->submenu != NULL || item->window != NULL)
      flags |= wuss_ICON_FLAGS_SUBMENU;
    if (item->flags & wuss_MENU_ITEM_SWATCH)
      flags |= wuss_ICON_FLAGS_SWATCH;

    specs[s].type    = wuss_ICON_TYPE_MENU_ENTRY;
    specs[s].bbox.x0 = 0;
    specs[s].bbox.y0 = y;
    specs[s].bbox.x1 = width;
    specs[s].bbox.y1 = y + pitch;
    specs[s].text    = item->text ? item->text : "";
    specs[s].fg      = wuss_COLOUR_BLACK;
    specs[s].bg      = wuss_NO_BACKGROUND;
    specs[s].u.menu_entry.swatch =
      (item->flags & wuss_MENU_ITEM_SWATCH) ? item->swatch : wuss_NO_BACKGROUND;
    specs[s].flags   = flags;
    s++;

    y += pitch;
  }

  /* doc spans the whole menu so the window can scroll it; min_doc is what the
   * window shows, so its own resize floor never exceeds what fits. */
  doc     = SIZE2D(width, doc_h);
  min_doc = SIZE2D(width, height);

  /* `at` is the content top-left, already clamped on screen above. Create the
   * window there directly -- creating it elsewhere and wuss_window_move-ing it
   * afterwards would blit its not-yet-rendered pixels and leave the titlebar
   * unpainted. */
  content.x0 = at.x;
  content.y0 = at.y;
  content.x1 = at.x + width;
  content.y1 = at.y + height;

  rc = wuss_window_create(menu_task, &content,
                          menu->title ? menu->title : "",
                          menu_flags,
                          wuss_BACKDROP_COLOUR(wuss_COLOUR_MENU),
                          doc, min_doc, &node->window);
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
      wuss_icon_set_selected(node->window, node->icons[i], 1);
  }
  wuss__free(wuss, made);

  *out = node;
  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t wuss_menu_open(wuss_task_t        *task,
                        const wuss_menu_t  *menu,
                        point_t             at,
                        wuss_menu_handle_t *out)
{
  result_t           rc;
  struct wuss__menu *root;
  wuss_t            *wuss;

  assert(task != NULL);
  assert(menu != NULL);

  wuss = task->wuss;

  if (wuss->menu_chain != NULL)
    wuss__menu_abandon(wuss);

  /* RISC OS convention: the pointer opens the menu sitting a little inside its
   * first item, not on the top-left corner. Shift the content top-left up and
   * left so `at` (the pointer) lands over row 0. */
  at.x -= WUSS_MENU_GUTTER_LEFT;
  at.y -= WUSS_MENU_ROW_PAD;

  rc = wuss__menu_spawn(wuss, task, menu, at, NULL, &root);
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

int wuss_menu_should_keep_open(const wuss_event_t *ev)
{
  if (ev == NULL || ev->kind != wuss_EVENT_MENU_SELECT)
    return 0;

  return (ev->data.menu_select.button & wuss_BUTTON_ADJUST) != 0;
}

result_t wuss_menu_open_ticked(wuss_task_t        *task,
                               wuss_menu_t        *menu,
                               unsigned int        ticked,
                               point_t             at,
                               wuss_menu_handle_t *out)
{
  wuss_menu_tick_set(menu, ticked);

  return wuss_menu_open(task, menu, at, out);
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

const wuss_menu_t *wuss_menu_handle_menu(wuss_menu_handle_t handle)
{
  struct wuss__menu *self;

  self = handle;

  return self ? self->menu : NULL;
}

/* Find the open chain level showing `menu`, or NULL if `handle` is stale or
 * `menu` is not a level of its chain. Shared by wuss_menu_tick_exclusive_live
 * and wuss_menu_tick_item_live. */
static struct wuss__menu *wuss__menu_open_level(wuss_menu_handle_t handle,
                                                const wuss_menu_t *menu)
{
  struct wuss__menu *root;
  struct wuss__menu *node;

  if (handle == NULL || menu == NULL)
    return NULL;

  root = handle;
  while (root->parent != NULL)
    root = root->parent;

  if (root->wuss->menu_chain != root)
    return NULL; /* stale handle */

  for (node = root; node != NULL; node = node->child)
    if (node->menu == menu)
      return node;

  return NULL; /* menu is not a level of this chain */
}

/* ----------------------------------------------------------------------- */

void wuss_menu_tick_set(wuss_menu_t *menu, unsigned int ticked)
{
  wuss_menu_item_t *item;
  int               i;

  assert(menu != NULL);
  assert(menu->nitems <= (int) (sizeof(ticked) * CHAR_BIT));

  for (i = 0; i < menu->nitems; i++)
  {
    item = (wuss_menu_item_t *) &menu->items[i];
    if (ticked & (1u << i))
      item->flags |= wuss_MENU_ITEM_TICKED;
    else
      item->flags &= ~(wuss_menu_item_flags_t) wuss_MENU_ITEM_TICKED;
  }
}

void wuss_menu_tick_exclusive(wuss_menu_t *menu, int index)
{
  wuss_menu_item_t *item;
  int               i;

  assert(menu != NULL);

  for (i = 0; i < menu->nitems; i++)
  {
    item = (wuss_menu_item_t *) &menu->items[i];
    if (i == index)
      item->flags |= wuss_MENU_ITEM_TICKED;
    else
      item->flags &= ~(wuss_menu_item_flags_t) wuss_MENU_ITEM_TICKED;
  }
}

void wuss_menu_tick_item(wuss_menu_t *menu, int index, int ticked)
{
  wuss_menu_item_t *item;

  assert(menu != NULL);

  if (index < 0 || index >= menu->nitems)
    return;

  item = (wuss_menu_item_t *) &menu->items[index];
  if (ticked)
    item->flags |= wuss_MENU_ITEM_TICKED;
  else
    item->flags &= ~(wuss_menu_item_flags_t) wuss_MENU_ITEM_TICKED;
}

/* ----------------------------------------------------------------------- */

void wuss_menu_tick_exclusive_live(wuss_menu_handle_t handle,
                                   const wuss_menu_t *menu,
                                   int                index)
{
  struct wuss__menu *node;
  int                i;

  node = wuss__menu_open_level(handle, menu);
  if (node == NULL)
    return;

  for (i = 0; i < menu->nitems; i++)
    wuss_icon_set_selected(node->window, node->icons[i], i == index);
}

void wuss_menu_tick_set_live(wuss_menu_handle_t handle,
                             const wuss_menu_t *menu,
                             unsigned int       ticked)
{
  struct wuss__menu *node;
  int                i;

  assert(menu->nitems <= (int) (sizeof(ticked) * CHAR_BIT));

  node = wuss__menu_open_level(handle, menu);
  if (node == NULL)
    return;

  for (i = 0; i < menu->nitems; i++)
    wuss_icon_set_selected(node->window, node->icons[i],
                           (ticked & (1u << i)) != 0);
}

void wuss_menu_tick_item_live(wuss_menu_handle_t handle,
                              const wuss_menu_t *menu,
                              int                index,
                              int                ticked)
{
  struct wuss__menu *node;

  node = wuss__menu_open_level(handle, menu);
  if (node == NULL)
    return;

  if (index < 0 || index >= menu->nitems)
    return;

  wuss_icon_set_selected(node->window, node->icons[index], ticked);
}

/* ----------------------------------------------------------------------- */

int wuss__menu_row_pinned(const wuss_t *wuss, const wuss_icon_t *icon)
{
  const struct wuss__menu *node;

  for (node = wuss->menu_chain; node != NULL; node = node->child)
  {
    if ((node->flags & wuss_MENU__BORROWED) || node->child == NULL ||
        node->open_index < 0)
      continue;
    if (node->icons[node->open_index] == icon)
      return 1;
  }

  return 0;
}

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

  wuss__menu_abandon(wuss);
  return 1;
}
