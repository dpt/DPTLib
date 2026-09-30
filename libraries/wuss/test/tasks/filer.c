/* wuss/test/tasks/filer.c -- directory display / drag-save target task */

#if defined(WUSS_APP) && defined(WUSS_ICONBAR)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

enum { FILER_MENU_INFO };

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
    if (strcmp(task->windows[i].path, path) == 0)
      return &task->windows[i];

  return NULL;
}

static filer_window_t *filer_find_by_window(filer_task_t        *task,
                                            const wuss_window_t *window)
{
  int i;

  for (i = 0; i < task->nwindows; i++)
    if (task->windows[i].window == window)
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
    return task->have_glyph_directory ? &task->glyph_directory : NULL;

  if (entry->type.ext != NULL && strcmp(entry->type.ext, ".png") == 0)
    return task->have_glyph_png ? &task->glyph_png : NULL;

  return task->have_glyph_file ? &task->glyph_file : NULL;
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

static result_t filer_open_path(filer_task_t *task, const char *path)
{
  result_t        rc;
  filer_window_t *fw;
  dirlist_t      *listing;
  wuss_window_t  *window;

  fw = filer_find_window(task, path);
  if (fw != NULL)
  {
    wuss_window_restack(fw->window, wuss_ZORDER_FRONT);
    return result_OK;
  }

  if (task->nwindows >= FILER_MAX_WINDOWS)
  {
    logf_error("filer: too many windows open, cannot open \"%s\"", path);
    return result_OOM;
  }

  rc = dirlist_scan(path, &listing);
  if (rc != result_OK)
    return rc;

  rc = task_window_create(task->delegate, SIZE2D(320, 240), path, &window);
  if (rc != result_OK)
  {
    dirlist_destroy(listing);
    return rc;
  }

  fw = &task->windows[task->nwindows];
  fw->owner = task;
  snprintf(fw->path, sizeof(fw->path), "%s", path);

  rc = wuss_gridview_create(&fw->gridview, window, dirlist_count(listing),
                            filer_item, FILER_LABEL_CHARS, filer_activate,
                            fw);
  if (rc != result_OK)
  {
    wuss_window_close(window);
    dirlist_destroy(listing);
    return rc;
  }

  fw->window  = window;
  fw->listing = listing;
  task->nwindows++;

  {
    size2d_t cell, fit;
    int      rows;

    /* filer_find_by_window must find this slot before this call: it fires
     * wuss_EVENT_OPEN synchronously, and filer_handle's OPEN case looks the
     * window up to forward the event to wuss_gridview_handle_event, which is
     * what actually reflows the grid to the new width. */
    cell = wuss_gridview_get_cell_size(fw->gridview);
    rows = (dirlist_count(listing) + FILER_COLUMNS - 1) / FILER_COLUMNS;
    rows = MIN(MAX(rows, 1), FILER_MAX_ROWS);
    fit  = SIZE2D(FILER_COLUMNS * cell.w, rows * cell.h);
    (void) wuss_window_resize(window, fit);
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

static void filer_load_glyph(const char *resources,
                             const char *leaf,
                             bitmap_t   *bm,
                             int        *have)
{
  const char *path;

  path  = pathf("%s/resources/wuss/%s.png", resources, leaf);
  *have = bitmap_load_png(bm, path) == result_OK;
}

result_t filer_create(wuss_t *wuss, filer_task_t **out)
{
  result_t                 rc;
  filer_task_t            *task;
  wuss_task_desc_t         delegate_desc;
  wuss_iconbar_icon_spec_t spec;
  const char              *resources;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss     = wuss;
  task->nwindows = 0;

  resources = wuss_get_resources(wuss);
  filer_load_glyph(resources, "filer-file", &task->glyph_file,
                   &task->have_glyph_file);
  filer_load_glyph(resources, "filer-directory", &task->glyph_directory,
                   &task->have_glyph_directory);
  filer_load_glyph(resources, "filer-png", &task->glyph_png,
                   &task->have_glyph_png);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, FILER_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo
                                * singleton just before open, in
                                * filer_menu_open */
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

  spec.text  = "Filer";
  spec.image = task->have_glyph_directory ? &task->glyph_directory : NULL;
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

void filer_destroy(filer_task_t *task)
{
  int i;

  for (i = 0; i < task->nwindows; i++)
  {
    wuss_gridview_destroy(task->windows[i].gridview);
    dirlist_destroy(task->windows[i].listing);
  }

  if (task->have_glyph_file)
    free(task->glyph_file.base);
  if (task->have_glyph_directory)
    free(task->glyph_directory.base);
  if (task->have_glyph_png)
    free(task->glyph_png.base);

  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t filer_menu_open(filer_task_t *task)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Filer", "Directory display / drag-save target");

  wuss_proginfo_set_desc(&desc);
  task->menu_items[FILER_MENU_INFO].window =
    wuss_proginfo_window(task->delegate);

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
  {
    filer_window_t *fw;

    fw = filer_find_by_window(task, window);
    if (fw != NULL)
      (void) wuss_gridview_handle_event(fw->gridview, event);
    if (event->kind == wuss_EVENT_MOUSE &&
        (event->data.mouse.action == wuss_MOUSE_DOWN) &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return filer_menu_open(task);
    return result_OK;
  }

  case wuss_EVENT_ICON:
    if (window != NULL)
      return result_OK; /* not the icon bar icon */
    if (event->data.iconbar_icon.action != wuss_MOUSE_UP)
      return result_OK;
    return filer_open_path(task, filer_root());

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &task->menu)
      return filer_open_path(task, filer_root());
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == task->menu_items[FILER_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;
    if (event->data.pre_show.handle == NULL)
      return result_OK;
    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_MESSAGE:
    return filer_message(task, window, event);

  case wuss_EVENT_CLOSE:
  {
    filer_window_t *fw;
    int             i;

    fw = filer_find_by_window(task, window);
    if (fw == NULL)
      return result_OK;

    wuss_gridview_destroy(fw->gridview);
    dirlist_destroy(fw->listing);

    i = (int) (fw - task->windows);
    task->windows[i] = task->windows[--task->nwindows];
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
