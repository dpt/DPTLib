/* wuss/test/tasks/filer.c -- directory display / drag-save target task */

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
#include "wuss/icon-spec.h"
#include "wuss/icon.h"
#include "wuss/menu.h"
#include "wuss/message.h"

#include "filer.h"
#include "common.h"

#define FILER_LABEL_CHARS 12
#define FILER_COLUMNS     4  /* initial window width, in cells */
#define FILER_MAX_ROWS    4  /* initial window height cap, in cells */

static char g_filer_root[DPTLIB_MAXPATH] = "";

void filer_set_root(const char *root)
{
  if (root == NULL)
  {
    g_filer_root[0] = '\0';
    return;
  }

  snprintf(g_filer_root, sizeof(g_filer_root), "%s", root);
}

static const char *filer_default_root(void)
{
#ifdef __EMSCRIPTEN__
  return "/";
#else
  return ".";
#endif
}

static const char *filer_root(void)
{
  return g_filer_root[0] != '\0' ? g_filer_root : filer_default_root();
}

/* ----------------------------------------------------------------------- */

static filer_window_t *filer_find_window(filer_task_t *task,
                                         const char   *path)
{
  int i;

  for (i = 0; i < task->nwindows; i++)
    if (task->windows[i].window != NULL &&
        strcmp(task->windows[i].path, path) == 0)
      return &task->windows[i];

  return NULL;
}

static filer_window_t *filer_find_by_window(filer_task_t        *task,
                                            const wuss_window_t *window)
{
  int i;

  for (i = 0; i < task->nwindows; i++)
    if (window != NULL && task->windows[i].window == window)
      return &task->windows[i];

  return NULL;
}

static void filer_rescan(filer_window_t *fw)
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

static const bitmap_t *filer_glyph_for(filer_task_t          *task,
                                       const dirlist_entry_t *entry)
{
  if (entry->is_dir)
    return task->glyph_directory;

  if (entry->type.ext != NULL && strcmp(entry->type.ext, ".png") == 0)
    return task->glyph_png;

  return task->glyph_file;
}

static void filer_item(int index, void *opaque, wuss_gridview_item_t *out)
{
  filer_window_t        *fw;
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
  out->glyph = filer_glyph_for(fw->owner, entry);
}

static result_t filer_open_path(filer_task_t *task, const char *path);

static void filer_activate(wuss_gridview_t *gridview,
                           int              index,
                           void            *opaque)
{
  filer_window_t        *fw;
  const dirlist_entry_t *entry;
  char                   child[DPTLIB_MAXPATH];

  NOT_USED(gridview);

  fw    = opaque;
  entry = dirlist_get(fw->listing, index);
  if (entry == NULL || !entry->is_dir)
    return;

  if (snprintf(child, sizeof(child), "%s/%s", fw->path, entry->leaf) >=
      (int) sizeof(child))
    return;

  (void) filer_open_path(fw->owner, child);
}

/* ----------------------------------------------------------------------- */

/* wuss_gridview_drag_fn_t: a file pressed and moved past the drag threshold
 * starts a core drag, and its path is remembered for filer_drop. Directories
 * are not draggable. */
static int filer_drag(wuss_gridview_t *gridview, int index, void *opaque)
{
  filer_window_t        *fw;
  const dirlist_entry_t *entry;

  NOT_USED(gridview);

  fw    = opaque;
  entry = dirlist_get(fw->listing, index);
  if (entry == NULL || entry->is_dir)
    return 0;

  if (snprintf(fw->owner->drag_path, sizeof(fw->owner->drag_path), "%s/%s",
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
static void filer_drop(filer_task_t *task, const wuss_event_t *event)
{
  char              path[DPTLIB_MAXPATH];
  wuss_msg_target_t target = { NULL, NULL };

  if (task->drag_path[0] == '\0')
    return;

  memcpy(path, task->drag_path, sizeof(path));
  task->drag_path[0] = '\0';

  if (event->data.drag_end.cancelled || event->data.drag_end.drop == NULL ||
      filer_find_by_window(task, event->data.drag_end.drop) != NULL)
    return;

  target.window = event->data.drag_end.drop;
  target.icon   = event->data.drag_end.drop_icon;

  (void) wuss_send_recorded_to(task->wuss, task->delegate, &target,
                               wuss_MESSAGE_DATA_LOAD, path,
                               strlen(path) + 1, 0, NULL);
}

/* ----------------------------------------------------------------------- */

static result_t filer_open_path(filer_task_t *task, const char *path)
{
  result_t        rc;
  char            canon[DPTLIB_MAXPATH];
  filer_window_t *fw;
  int             slot;
  dirlist_t      *listing;
  wuss_window_t  *window;

#ifndef __riscos
  /* "." or "a/../b" becomes the full path, for the title and so that the
   * same directory reached two ways finds its existing window */
  if (realpath(path, canon) != NULL)
    path = canon;
#endif

  fw = filer_find_window(task, path);
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

  if (slot >= FILER_MAX_WINDOWS)
  {
    logf_error("filer: too many windows open, cannot open \"%s\"", path);
    return result_OOM;
  }

  rc = dirlist_scan(path, &listing);
  if (rc != result_OK)
    return rc;

  fw          = &task->windows[slot];
  fw->owner   = task;
  fw->listing = listing; /* wuss_gridview_create measures through filer_item */
  snprintf(fw->path, sizeof(fw->path), "%s", path);

  rc = wuss_gridview_window_create(&fw->gridview, task->delegate, path,
                                   dirlist_count(listing), filer_item,
                                   FILER_LABEL_CHARS, filer_activate, fw,
                                   &window);
  if (rc != result_OK)
  {
    dirlist_destroy(listing);
    fw->listing = NULL;
    return rc;
  }

  wuss_gridview_set_drag(fw->gridview, filer_drag);

  fw->window = window;
  if (slot == task->nwindows)
    task->nwindows++;

  /* filer_find_by_window must find this slot before this call: it fires
   * wuss_EVENT_OPEN synchronously, and filer_handle's OPEN case looks the
   * window up to forward the event to wuss_gridview_handle_event, which is
   * what actually reflows the grid to the new width. */
  wuss_gridview_fit_window(fw->gridview, FILER_COLUMNS, FILER_MAX_ROWS);

  return result_OK;
}

/* ----------------------------------------------------------------------- */

static const bitmap_t *filer_find_glyph(const wuss_task_t *task,
                                        const char        *name)
{
  const bitmap_t *bm;

  bm = wuss_task_iconpool_find(task, name);
  if (bm == NULL)
    logf_warning("filer: glyph '%s' unavailable; cells show label only",
                 name);

  return bm;
}

result_t filer_create(wuss_t *wuss, filer_task_t **out)
{
  result_t                 rc;
  filer_task_t            *task;
  wuss_task_desc_t         delegate_desc;
  wuss_iconbar_icon_spec_t spec;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss     = wuss;
  task->nwindows = 0;

  WUSS_MENU_ITEM(task->menu_items, FILER_MENU_SELECT_ALL, "Select all",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->menu_items, FILER_MENU_CLEAR_SELECTION,
                 "Clear selection", wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->menu_items, FILER_MENU_OPEN_PARENT, "Open parent",
                 wuss_MENU_ITEM_NONE);
#ifdef __EMSCRIPTEN__
  WUSS_MENU_ITEM(task->menu_items, FILER_MENU_DOWNLOAD, "Download",
                 wuss_MENU_ITEM_NONE);
#endif
  WUSS_MENU_TITLE(task->menu, "Filer", task->menu_items,
                 NELEMS(task->menu_items));

  delegate_desc.handle    = filer_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "filer";
  rc = wuss_task_create(wuss, &delegate_desc, &task->delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  task->glyph_file      = filer_find_glyph(task->delegate, "file_unk");
  task->glyph_directory = filer_find_glyph(task->delegate, "file_dir");
  task->glyph_png       = filer_find_glyph(task->delegate, "file_b60");

  spec.text  = "Filer";
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

result_t filer_app_create(wuss_t *wuss)
{
  return filer_create(wuss, NULL);
}

void filer_destroy(filer_task_t *task)
{
  int i;

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
EM_JS(void, filer_web_download, (const char *path),
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
    console.error('filer: cannot download ' + p + ': ' + e);
  }
});

/* Downloads every selected file in fw, or with download zero just counts
 * them. Directories are skipped. Returns the number of files. */
static int filer_download_selection(const filer_window_t *fw, int download)
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

    if (snprintf(path, sizeof(path), "%s/%s", fw->path, entry->leaf) >=
        (int) sizeof(path))
      continue;

    filer_web_download(path);
  }

  return n;
}
#endif

/* the directory holding path, in out (capacity cap). Returns zero if path is
 * the root or has no parent to give. */
static int filer_parent_path(const char *path, char *out, size_t cap)
{
  const char *slash;
  size_t      len;

  slash = strrchr(path, '/');
  if (slash == NULL || (slash == path && path[1] == '\0'))
    return 0;

  len = (size_t) (slash - path);
  if (len == 0)
    len = 1; /* the parent of "/a" is "/" */

  return snprintf(out, cap, "%.*s", (int) len, path) < (int) cap;
}

/* shade or unshade a menu row, on screen too if the menu is open */
static void filer_menu_enable(filer_task_t *task, int row, int enabled)
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
static void filer_menu_update(filer_task_t *task)
{
  filer_window_t *fw;
  char            parent[DPTLIB_MAXPATH];
  int             count;
  int             selected;

  fw       = filer_find_by_window(task, task->menu_window);
  count    = fw != NULL ? dirlist_count(fw->listing) : 0;
  selected = fw != NULL ? wuss_gridview_selected_count(fw->gridview) : 0;

  filer_menu_enable(task, FILER_MENU_SELECT_ALL, selected < count);
  filer_menu_enable(task, FILER_MENU_CLEAR_SELECTION, selected > 0);
  filer_menu_enable(task, FILER_MENU_OPEN_PARENT,
                    fw != NULL &&
                    filer_parent_path(fw->path, parent, sizeof(parent)));
#ifdef __EMSCRIPTEN__
  filer_menu_enable(task, FILER_MENU_DOWNLOAD,
                    fw != NULL && filer_download_selection(fw, 0) > 0);
#endif
}

static result_t filer_menu_open(filer_task_t *task, wuss_window_t *window)
{
  task->menu_window = window;
  filer_menu_update(task);

  return wuss_menu_open_at_pointer(task->delegate, &task->menu,
                                   &task->menu_handle);
}

/* validates the dropped leafname and, on success, fills 'path' (capacity
 * DPTLIB_MAXPATH) with fw's directory plus that leaf. returns result_OK, or
 * a code describing why the save cannot proceed (bad leafname, or the
 * resulting path too long) -- either way the caller must not acknowledge
 * the DataSave, so the transfer bounces. */
static result_t filer_build_save_path(const filer_window_t *fw,
                                      const char           *leaf,
                                      char                 *path,
                                      size_t                cap)
{
  if (!path_leaf_valid(leaf))
    return result_BAD_ARG;

  if (snprintf(path, cap, "%s/%s", fw->path, leaf) >= (int) cap)
    return result_BAD_ARG;

  return result_OK;
}

static result_t filer_message(filer_task_t       *task,
                              wuss_window_t      *window,
                              const wuss_event_t *event)
{
  const wuss_message_t *msg;
  filer_window_t       *fw;

  msg = event->data.message;

  if (msg->action == wuss_MESSAGE_DATA_SAVE)
  {
    char                    path[DPTLIB_MAXPATH];
    const wuss_data_save_t *payload;

    fw = filer_find_by_window(task, window);
    if (fw == NULL)
      return result_OK; /* not one of ours; leave it to bounce */

    payload = (const wuss_data_save_t *) msg->data;
    if (filer_build_save_path(fw, payload->leafname, path,
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
    fw = filer_find_by_window(task, window);
    if (fw == NULL)
      return result_OK;

    /* no state is kept between DataSaveAck and this DataLoad -- the file is
     * already on disk under fw->path, so just rescan and acknowledge */
    filer_rescan(fw);
    (void) wuss_acknowledge(task->wuss, task->delegate, msg);
    return result_OK;
  }

  return result_OK;
}

result_t filer_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  filer_task_t *task;

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
    filer_window_t *fw;

    fw = filer_find_by_window(task, window);
    if (fw != NULL)
      (void) wuss_gridview_handle_event(fw->gridview, event);
    if (event->kind == wuss_EVENT_MOUSE &&
        (event->data.mouse.action == wuss_MOUSE_DOWN) &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return filer_menu_open(task, window);
    return result_OK;
  }

  case wuss_EVENT_ICON:
    if (window != NULL)
      return result_OK; /* not the icon bar icon */
    if (event->data.iconbar_icon.action != wuss_MOUSE_UP)
      return result_OK;
    return filer_open_path(task, filer_root());

  case wuss_EVENT_MENU_SELECT:
  {
    filer_window_t *fw;
    char            parent[DPTLIB_MAXPATH];

    if (event->data.menu_select.menu != &task->menu)
      return result_OK;

    fw = filer_find_by_window(task, task->menu_window);
    switch (event->data.menu_select.index)
    {
    case FILER_MENU_SELECT_ALL:
      if (fw != NULL)
      {
        wuss_gridview_select_all(fw->gridview);
        filer_menu_update(task);
      }
      return result_OK;

    case FILER_MENU_CLEAR_SELECTION:
      if (fw != NULL)
      {
        wuss_gridview_clear_selection(fw->gridview);
        filer_menu_update(task);
      }
      return result_OK;

    case FILER_MENU_OPEN_PARENT:
      if (fw != NULL && filer_parent_path(fw->path, parent, sizeof(parent)))
        return filer_open_path(task, parent);
      return result_OK;

#ifdef __EMSCRIPTEN__
    case FILER_MENU_DOWNLOAD:
      if (fw != NULL)
        (void) filer_download_selection(fw, 1);
      return result_OK;
#endif

    default:
      return result_OK;
    }
  }

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_MESSAGE:
    return filer_message(task, window, event);

  case wuss_EVENT_DRAG_END:
    filer_drop(task, event);
    return result_OK;

  case wuss_EVENT_CLOSE:
  {
    filer_window_t *fw;

    fw = filer_find_by_window(task, window);
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
    filer_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP && WUSS_ICONBAR */
