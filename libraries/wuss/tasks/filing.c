/* wuss/tasks/filing.c -- directory display / drag-save target task */

#if defined(WUSS_APP) && defined(WUSS_ICONBAR)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "io/path.h"
#include "wuss/component/proginfo.h"
#include "wuss/icon-spec.h"
#include "wuss/icon.h"
#include "wuss/menu.h"
#include "wuss/message.h"

#include "filing.h"
#include "common.h"

#define FILING_LABEL_CHARS 12
#define FILING_COLUMNS     4  /* initial window width, in cells */
#define FILING_MAX_ROWS    4  /* initial window height cap, in cells */

static char g_filing_root[DPTLIB_MAXPATH] = "";

void filing_set_root(const char *root)
{
  if (root == NULL)
  {
    g_filing_root[0] = '\0';
    return;
  }

  snprintf(g_filing_root, sizeof(g_filing_root), "%s", root);
}

static const char *filing_default_root(void)
{
#ifdef __EMSCRIPTEN__
  return "/";
#elif defined(__riscos)
  return "@"; /* the current directory */
#else
  return ".";
#endif
}

/* native "dir<sep>leaf" (RISC OS joins with '.'). Returns the snprintf
 * length, so >= cap means truncated. */
static int filing_join(char       *out,
                       size_t      cap,
                       const char *dir,
                       const char *leaf)
{
#ifdef __riscos
  return snprintf(out, cap, "%s.%s", dir, leaf);
#else
  return snprintf(out, cap, "%s/%s", dir, leaf);
#endif
}

static const char *filing_root(void)
{
  return g_filing_root[0] != '\0' ? g_filing_root : filing_default_root();
}

/* ----------------------------------------------------------------------- */

static filing_window_t *filing_find_window(filing_task_t *task,
                                           const char    *path)
{
  int i;

  for (i = 0; i < task->nwindows; i++)
    if (task->windows[i].window != NULL &&
        strcmp(task->windows[i].path, path) == 0)
      return &task->windows[i];

  return NULL;
}

static filing_window_t *filing_find_by_window(filing_task_t       *task,
                                              const wuss_window_t *window)
{
  int i;

  for (i = 0; i < task->nwindows; i++)
    if (window != NULL && task->windows[i].window == window)
      return &task->windows[i];

  return NULL;
}

static void filing_rescan(filing_window_t *fw)
{
  dirlist_t *fresh;

  if (dirlist_scan(fw->path, &fresh) != result_OK)
    return;

  dirlist_destroy(fw->listing);
  fw->listing = fresh;
  (void) wuss_gridview_set_count(fw->gridview, dirlist_count(fresh));
}

/* wuss_gridview_item_fn_t's opaque is fw (set at wuss_gridview_create); the
 * owning task (for its shared glyphs) is reached via fw->owner. */

static const bitmap_t *filing_glyph_for(filing_task_t         *task,
                                        const dirlist_entry_t *entry)
{
  if (entry->is_dir)
    return task->glyph_directory;

  if (entry->type.ext != NULL && strcmp(entry->type.ext, ".png") == 0)
    return task->glyph_png;

  return task->glyph_file;
}

static void filing_item(int index, void *opaque, wuss_gridview_item_t *out)
{
  filing_window_t       *fw;
  const dirlist_entry_t *entry;

  fw    = opaque;
  entry = dirlist_get(fw->listing, index);
  if (entry == NULL)
  {
    out->glyph = NULL;
    out->label = NULL;
    return;
  }

  out->label = entry->leaf;
  out->glyph = filing_glyph_for(fw->owner, entry);
}

static result_t filing_open_path(filing_task_t       *task,
                                 const char          *path,
                                 const wuss_window_t *parent,
                                 int                  over);

/* hide or show every window, relabelling the icon bar menu's row to match */
static result_t filing_set_hidden(filing_task_t *task, int hidden)
{
  wuss_menu_item_t *item;

  task->hidden = hidden;

  item       = &task->ib_menu_items[FILING_IB_MENU_HIDE];
  item->text = hidden ? "Show" : "Hide";
  wuss_menu_set_item_text_live(task->menu_handle, &task->ib_menu,
                               FILING_IB_MENU_HIDE, item->text);

  return wuss_task_set_hidden(task->wuss, task->delegate, hidden);
}

static void filing_activate(wuss_gridview_t *gridview,
                            int              index,
                            wuss_button_t    button,
                            void            *opaque)
{
  filing_window_t       *fw;
  const dirlist_entry_t *entry;
  char                   child[DPTLIB_MAXPATH];

  NOT_USED(gridview);

  fw    = opaque;
  entry = dirlist_get(fw->listing, index);
  if (entry == NULL || !entry->is_dir)
    return;

  if (filing_join(child, sizeof(child), fw->path, entry->leaf) >=
      (int) sizeof(child))
    return;

  /* as RISC OS: a double ADJUST opens the child in this viewer's place and
   * closes this one. That frees fw's slot and destroys gridview; neither is
   * used after. */
  (void) filing_open_path(fw->owner, child, fw->window,
                         button & wuss_BUTTON_ADJUST);

  if (button & wuss_BUTTON_ADJUST)
    (void) wuss_window_try_close(fw->window);
}

/* ----------------------------------------------------------------------- */

/* wuss_gridview_drag_fn_t: a file pressed and moved past the drag threshold
 * starts a core drag, and its path is remembered for filing_drop. Directories
 * are not draggable. */
static int filing_drag(wuss_gridview_t *gridview, int index, void *opaque)
{
  filing_window_t       *fw;
  const dirlist_entry_t *entry;

  NOT_USED(gridview);

  fw    = opaque;
  entry = dirlist_get(fw->listing, index);
  if (entry == NULL || entry->is_dir)
    return 0;

  if (filing_join(fw->owner->drag_path, sizeof(fw->owner->drag_path),
                 fw->path, entry->leaf) >= (int) sizeof(fw->owner->drag_path))
  {
    fw->owner->drag_path[0] = '\0';
    return 0;
  }

  return 1;
}

/* The drag ended: offer the file to the window or icon bar icon it was
 * dropped on. Drops on nothing, on a Filer window or after Escape do
 * nothing. */
static void filing_drop(filing_task_t *task, const wuss_event_t *event)
{
  char              path[DPTLIB_MAXPATH];
  wuss_msg_target_t target = { NULL, NULL };

  if (task->drag_path[0] == '\0')
    return;

  memcpy(path, task->drag_path, sizeof(path));
  task->drag_path[0] = '\0';

  if (event->data.drag_end.cancelled || event->data.drag_end.drop == NULL ||
      filing_find_by_window(task, event->data.drag_end.drop) != NULL)
    return;

  target.window = event->data.drag_end.drop;
  target.icon   = event->data.drag_end.drop_icon;

  (void) wuss_send_recorded_to(task->wuss, task->delegate, &target,
                               wuss_MESSAGE_DATA_LOAD, path,
                               strlen(path) + 1, 0, NULL);
}

/* ----------------------------------------------------------------------- */

/* Opens a viewer on path, or brings an open one to the front. A new viewer
 * cascades from parent, or with over set takes parent's place (top-left) for
 * a parent that is about to close. */
static result_t filing_open_path(filing_task_t       *task,
                                 const char          *path,
                                 const wuss_window_t *parent,
                                 int                  over)
{
  result_t         rc;
  char             canon[DPTLIB_MAXPATH];
  filing_window_t *fw;
  int              slot;
  dirlist_t       *listing;
  wuss_place_t     place;
  wuss_window_t   *window;

#ifndef __riscos
  /* "." or "a/../b" becomes the full path, for the title and so that the
   * same directory reached two ways finds its existing window */
  if (realpath(path, canon) != NULL)
    path = canon;
#endif

  if (task->hidden)
    (void) filing_set_hidden(task, 0);

  fw = filing_find_window(task, path);
  if (fw != NULL)
  {
    wuss_window_restack(fw->window, wuss_ZORDER_FRONT);
    return result_OK;
  }

  /* Reuse a slot freed by a CLOSE, else append. Slots never move: each one's
   * address is its gridview's opaque. */
  for (slot = 0; slot < task->nwindows; slot++)
    if (task->windows[slot].window == NULL)
      break;

  if (slot >= FILING_MAX_WINDOWS)
  {
    logf_error("filing: too many windows open, cannot open \"%s\"", path);
    return result_OOM;
  }

  rc = dirlist_scan(path, &listing);
  if (rc != result_OK)
    return rc;

  fw          = &task->windows[slot];
  fw->owner   = task;
  fw->listing = listing; /* wuss_gridview_create measures through filing_item */
  snprintf(fw->path, sizeof(fw->path), "%s", path);

  place = task_place_from(parent);
  if (over)
    place.offset = POINT(0, 0);

  rc = wuss_gridview_window_create(&fw->gridview, task->delegate, &place,
                                   path, dirlist_count(listing), filing_item,
                                   FILING_LABEL_CHARS, filing_activate, fw,
                                   &window);
  if (rc != result_OK)
  {
    dirlist_destroy(listing);
    fw->listing = NULL;
    return rc;
  }

  wuss_gridview_set_drag(fw->gridview, filing_drag);

  fw->window = window;
  if (slot == task->nwindows)
    task->nwindows++;

  /* filing_find_by_window must find this slot before this call: it fires
   * wuss_EVENT_OPEN synchronously, and filing_handle's OPEN case looks the
   * window up to forward the event to wuss_gridview_handle_event, which is
   * what actually reflows the grid to the new width. */
  wuss_gridview_fit_window(fw->gridview, FILING_COLUMNS, FILING_MAX_ROWS);

  return result_OK;
}

/* ----------------------------------------------------------------------- */

static const bitmap_t *filing_find_glyph(const wuss_task_t *task,
                                         const char        *name)
{
  const bitmap_t *bm;

  bm = wuss_task_iconpool_find(task, name);
  if (bm == NULL)
    logf_warning("filing: glyph '%s' unavailable; cells show label only",
                 name);

  return bm;
}

result_t filing_create(wuss_t *wuss, filing_task_t **out)
{
  result_t                 rc;
  filing_task_t           *task;
  wuss_task_desc_t         delegate_desc;
  wuss_iconbar_icon_spec_t spec;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss     = wuss;
  task->nwindows = 0;

  WUSS_MENU_ITEM(task->menu_items, FILING_MENU_SELECT_ALL, "Select all",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->menu_items, FILING_MENU_CLEAR_SELECTION,
                 "Clear selection", wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->menu_items, FILING_MENU_OPEN_PARENT, "Open parent",
                 wuss_MENU_ITEM_NONE);
#ifdef __EMSCRIPTEN__
  WUSS_MENU_ITEM(task->menu_items, FILING_MENU_DOWNLOAD, "Download",
                 wuss_MENU_ITEM_NONE);
#endif
  WUSS_MENU_TITLE(task->menu, "Filing", task->menu_items,
                 NELEMS(task->menu_items));

  WUSS_MENU_ITEM_WINDOW(task->ib_menu_items, FILING_IB_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU |
                        wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo
                                * singleton when the menu opens */
  WUSS_MENU_ITEM(task->ib_menu_items, FILING_IB_MENU_OPEN_ROOT, "Open root",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->ib_menu_items, FILING_IB_MENU_CLOSE_ALL, "Close all",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->ib_menu_items, FILING_IB_MENU_HIDE, "Hide",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->ib_menu, "Filing", task->ib_menu_items,
                  NELEMS(task->ib_menu_items));

  delegate_desc.handle    = filing_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "filing";
  rc = wuss_task_create(wuss, &delegate_desc, &task->delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  task->glyph_file      = filing_find_glyph(task->delegate, "file_unk");
  task->glyph_directory = filing_find_glyph(task->delegate, "file_dir");
  task->glyph_png       = filing_find_glyph(task->delegate, "file_b60");

  spec.text  = "Filing";
  spec.side  = wuss_ICONBAR_LEFT;
  spec.image = task->glyph_directory;
  rc = wuss_iconbar_icon_create(wuss, task->delegate, &spec,
                                &task->iconbar_icon);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* unregister; its QUIT frees the
                                        * task block */
    return rc;
  }

  if (out)
    *out = task;

  return result_OK;
}

/* the task installed by filing_app_create, so installing it again (e.g. from
 * both the startup list and AppsFiler) does not add a second icon */
static filing_task_t *g_filing;

result_t filing_app_create(wuss_t *wuss)
{
  if (g_filing != NULL)
    return result_OK;

  return filing_create(wuss, &g_filing);
}

void filing_destroy(filing_task_t *task)
{
  int i;

  if (g_filing == task)
    g_filing = NULL;

  for (i = 0; i < task->nwindows; i++)
  {
    wuss_gridview_destroy(task->windows[i].gridview);
    dirlist_destroy(task->windows[i].listing);
  }

  free(task);
}

/* ----------------------------------------------------------------------- */

#ifdef __EMSCRIPTEN__
/* Hands the file at path, in Emscripten's in-memory filesystem, to the
 * browser as a download named after its leaf. */
EM_JS(void, filing_web_download, (const char *path),
{
  var p = UTF8ToString(path);
  var a;

  try
  {
    a          = document.createElement('a');
    a.href     = URL.createObjectURL(new Blob([FS.readFile(p)],
                                              { type: 'application/octet-stream' }));
    a.download = p.split('/').pop();
    a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); }, 0);
  }
  catch (e)
  {
    console.error('filing: cannot download ' + p + ': ' + e);
  }
});

/* Downloads every selected file in fw, or with download zero just counts
 * them. Directories are skipped. Returns the number of files. */
static int filing_download_selection(const filing_window_t *fw, int download)
{
  const dirlist_entry_t *entry;
  char                   path[DPTLIB_MAXPATH];
  int                    n;
  int                    i;

  n = 0;
  for (i = 0; i < dirlist_count(fw->listing); i++)
  {
    if (!wuss_gridview_is_selected(fw->gridview, i))
      continue;

    entry = dirlist_get(fw->listing, i);
    if (entry == NULL || entry->is_dir)
      continue;

    n++;
    if (!download)
      continue;

    if (filing_join(path, sizeof(path), fw->path, entry->leaf) >=
        (int) sizeof(path))
      continue;

    filing_web_download(path);
  }

  return n;
}
#endif

/* the directory holding path, in out (capacity cap). Returns zero if path is
 * the root or has no parent to give. */
static int filing_parent_path(const char *path, char *out, size_t cap)
{
  const char *slash;
#ifndef __riscos
  size_t      len;
#endif

#ifdef __riscos
  /* "adfs::Disc.$.Dir" -> "adfs::Disc.$"; stop at the root or a bare name */
  slash = strrchr(path, '.');
  if (slash == NULL || slash == path)
    return 0;

  return snprintf(out, cap, "%.*s", (int) (slash - path), path) < (int) cap;
#else
  slash = strrchr(path, '/');
  if (slash == NULL || (slash == path && path[1] == '\0'))
    return 0;

  len = (size_t) (slash - path);
  if (len == 0)
    len = 1; /* the parent of "/a" is "/" */

  return snprintf(out, cap, "%.*s", (int) len, path) < (int) cap;
#endif
}

/* shade or unshade a menu row, on screen too if the menu is open */
static void filing_menu_enable(filing_task_t *task, int row, int enabled)
{
  if (enabled)
    task->menu_items[row].flags &= ~wuss_MENU_ITEM_DISABLED;
  else
    task->menu_items[row].flags |= wuss_MENU_ITEM_DISABLED;

  if (task->menu_handle != NULL)
    wuss_menu_disable_item_live(task->menu_handle, &task->menu, row, !enabled);
}

/* set which rows apply to the window the menu was opened over, from its
 * listing and selection. Called as the menu opens and again after a pick that
 * leaves it open (ADJUST) and changes the selection. */
static void filing_menu_update(filing_task_t *task)
{
  filing_window_t *fw;
  char             parent[DPTLIB_MAXPATH];
  int              count;
  int              selected;

  fw       = filing_find_by_window(task, task->menu_window);
  count    = fw != NULL ? dirlist_count(fw->listing) : 0;
  selected = fw != NULL ? wuss_gridview_selected_count(fw->gridview) : 0;

  filing_menu_enable(task, FILING_MENU_SELECT_ALL, selected < count);
  filing_menu_enable(task, FILING_MENU_CLEAR_SELECTION, selected > 0);
  filing_menu_enable(task, FILING_MENU_OPEN_PARENT,
                    fw != NULL &&
                    filing_parent_path(fw->path, parent, sizeof(parent)));
#ifdef __EMSCRIPTEN__
  filing_menu_enable(task, FILING_MENU_DOWNLOAD,
                    fw != NULL && filing_download_selection(fw, 0) > 0);
#endif
}

static result_t filing_iconbar_menu_open(filing_task_t *task)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Filing", "Directory display and file save target");

  wuss_proginfo_set_desc(&desc);
  task->ib_menu_items[FILING_IB_MENU_INFO].window =
    wuss_proginfo_window(task->delegate);
  if (wuss_task_has_windows(task->wuss, task->delegate))
  {
    task->ib_menu_items[FILING_IB_MENU_CLOSE_ALL].flags &=
      (wuss_menu_item_flags_t) ~wuss_MENU_ITEM_DISABLED;
    task->ib_menu_items[FILING_IB_MENU_HIDE].flags &=
      (wuss_menu_item_flags_t) ~wuss_MENU_ITEM_DISABLED;
  }
  else
  {
    task->ib_menu_items[FILING_IB_MENU_CLOSE_ALL].flags |=
      wuss_MENU_ITEM_DISABLED;
    task->ib_menu_items[FILING_IB_MENU_HIDE].flags |= wuss_MENU_ITEM_DISABLED;
  }

  return wuss_iconbar_menu_open(task->delegate, &task->ib_menu,
                                &task->menu_handle);
}

static result_t filing_menu_open(filing_task_t *task, wuss_window_t *window)
{
  task->menu_window = window;
  filing_menu_update(task);

  return wuss_menu_open_at_pointer(task->delegate, &task->menu,
                                   &task->menu_handle);
}

/* validates the dropped leafname and, on success, fills 'path' (capacity
 * DPTLIB_MAXPATH) with fw's directory plus that leaf. returns result_OK, or
 * a code describing why the save cannot proceed (bad leafname, or the
 * resulting path too long) -- either way the caller must not acknowledge
 * the DataSave, so the transfer bounces. */
static result_t filing_build_save_path(const filing_window_t *fw,
                                       const char            *leaf,
                                       char                  *path,
                                       size_t                 cap)
{
  if (!path_leaf_valid(leaf))
    return result_BAD_ARG;

  if (filing_join(path, cap, fw->path, leaf) >= (int) cap)
    return result_BAD_ARG;

  return result_OK;
}

static result_t filing_message(filing_task_t      *task,
                               wuss_window_t      *window,
                               const wuss_event_t *event)
{
  const wuss_message_t *msg;
  filing_window_t      *fw;

  msg = event->data.message;

  if (msg->action == wuss_MESSAGE_DATA_SAVE)
  {
    char                    path[DPTLIB_MAXPATH];
    const wuss_data_save_t *payload;

    fw = filing_find_by_window(task, window);
    if (fw == NULL)
      return result_OK; /* not one of ours; leave it to bounce */

    payload = (const wuss_data_save_t *) msg->data;
    if (filing_build_save_path(fw, payload->leafname, path,
                              sizeof(path)) != result_OK)
      return result_OK; /* invalid leaf or path too long: never
                         * acknowledged, so the send bounces naturally */

    (void) wuss_send(task->wuss, task->delegate, payload->reply_window,
                     wuss_MESSAGE_DATA_SAVE_ACK, path, strlen(path) + 1,
                     msg->my_ref);
    return result_OK;
  }

  if (msg->action == wuss_MESSAGE_DATA_LOAD)
  {
    fw = filing_find_by_window(task, window);
    if (fw == NULL)
      return result_OK;

    /* no state is kept between DataSaveAck and this DataLoad -- the file is
     * already on disk under fw->path, so just rescan and acknowledge */
    filing_rescan(fw);
    (void) wuss_acknowledge(task->wuss, task->delegate, msg);
    return result_OK;
  }

  return result_OK;
}

result_t filing_handle(wuss_window_t      *window,
                       const wuss_event_t *event,
                       void               *task_data)
{
  filing_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
  case wuss_EVENT_MOUSE:
  case wuss_EVENT_OPEN:
  case wuss_EVENT_PALETTE:
  case wuss_EVENT_POINTER_EXIT:
  case wuss_EVENT_RELAYOUT:
  {
    filing_window_t *fw;

    fw = filing_find_by_window(task, window);
    if (fw != NULL)
      (void) wuss_gridview_handle_event(fw->gridview, event);
    if (event->kind == wuss_EVENT_MOUSE &&
        (event->data.mouse.action == wuss_MOUSE_DOWN) &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return filing_menu_open(task, window);
    return result_OK;
  }

  case wuss_EVENT_ICON:
    if (window != NULL)
      return result_OK; /* not the icon bar icon */
    if (event->data.iconbar_icon.action != wuss_MOUSE_UP)
      return result_OK;
    if (event->data.iconbar_icon.button & wuss_BUTTON_MENU)
      return filing_iconbar_menu_open(task);
    if (task->hidden) /* the click only reveals; it opens nothing new */
      return filing_set_hidden(task, 0);
    return filing_open_path(task, filing_root(), NULL, 0);

  case wuss_EVENT_MENU_SELECT:
  {
    filing_window_t *fw;
    char             parent[DPTLIB_MAXPATH];

    if (event->data.menu_select.menu == &task->ib_menu)
    {
      if (event->data.menu_select.index == FILING_IB_MENU_OPEN_ROOT)
        return filing_open_path(task, filing_root(), NULL, 0);
      if (event->data.menu_select.index == FILING_IB_MENU_CLOSE_ALL)
      {
        int i;

        for (i = 0; i < task->nwindows; i++)
          if (task->windows[i].window != NULL)
            (void) wuss_window_try_close(task->windows[i].window);
        return result_OK;
      }
      if (event->data.menu_select.index == FILING_IB_MENU_HIDE)
      {
        return filing_set_hidden(task, !task->hidden);
      }
      return result_OK;
    }
    if (event->data.menu_select.menu != &task->menu)
      return result_OK;

    fw = filing_find_by_window(task, task->menu_window);
    switch (event->data.menu_select.index)
    {
    case FILING_MENU_SELECT_ALL:
      if (fw != NULL)
      {
        wuss_gridview_select_all(fw->gridview);
        filing_menu_update(task);
      }
      return result_OK;

    case FILING_MENU_CLEAR_SELECTION:
      if (fw != NULL)
      {
        wuss_gridview_clear_selection(fw->gridview);
        filing_menu_update(task);
      }
      return result_OK;

    case FILING_MENU_OPEN_PARENT:
      if (fw != NULL && filing_parent_path(fw->path, parent, sizeof(parent)))
        return filing_open_path(task, parent, fw->window, 0);
      return result_OK;

#ifdef __EMSCRIPTEN__
    case FILING_MENU_DOWNLOAD:
      if (fw != NULL)
        (void) filing_download_selection(fw, 1);
      return result_OK;
#endif

    default:
      return result_OK;
    }
  }

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == task->ib_menu_items[FILING_IB_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK || event->data.pre_show.handle == NULL)
      return rc;

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_MESSAGE:
    return filing_message(task, window, event);

  case wuss_EVENT_DRAG_END:
    filing_drop(task, event);
    return result_OK;

  case wuss_EVENT_CLOSE_ADJUST:
  {
    filing_window_t *fw;
    int              keep;
    char             parent[DPTLIB_MAXPATH];

    /* as RISC OS: open the parent in this viewer's place and close this one
     * -- or, with Shift held, cascade the parent and keep this one. The
     * close frees fw's slot, so fw is not used after it. */
    fw = filing_find_by_window(task, window);
    if (fw == NULL)
      return result_OK;

    keep = (event->data.close_adjust.modifiers & wuss_KEY_MOD_SHIFT) != 0;
    if (filing_parent_path(fw->path, parent, sizeof(parent)))
      (void) filing_open_path(task, parent, fw->window, !keep);
    if (!keep)
      return wuss_window_try_close(window);
    return result_OK;
  }

  case wuss_EVENT_CLOSE:
  {
    filing_window_t *fw;

    fw = filing_find_by_window(task, window);
    if (fw == NULL)
      return result_OK;

    wuss_gridview_destroy(fw->gridview);
    dirlist_destroy(fw->listing);

    /* Free the slot in place: moving another into it would leave that one's
     * gridview opaque pointing at its old address. */
    fw->window   = NULL;
    fw->gridview = NULL;
    fw->listing  = NULL;
    return result_OK;
  }

  case wuss_EVENT_QUIT:
    filing_destroy(task);
    return result_OK;

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP && WUSS_ICONBAR */
