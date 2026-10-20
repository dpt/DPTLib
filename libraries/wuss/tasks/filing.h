/* wuss/tasks/filing.h -- directory display / drag-save target task */

#ifndef TASKS_FILING_H
#define TASKS_FILING_H

#if defined(WUSS_APP) && defined(WUSS_ICONBAR)

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "io/dirlist.h"
#include "io/filetype.h"
#include "io/path.h"
#include "wuss/gadget/gridview.h"
#include "wuss/iconbar.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* ponytail: fixed cap on the number of directory windows open at once, so
 * filing_task_t can hold them inline with no malloc bookkeeping. Raise it if
 * this ever feels cramped. */
#define FILING_MAX_WINDOWS 16

/* Filer menu rows, which act on the window the menu was opened over.
 * Download only exists in the browser build, where saved files
 * otherwise stay trapped in Emscripten's in-memory filesystem. */
enum
{
  FILING_MENU_SELECT_ALL,
  FILING_MENU_CLEAR_SELECTION,
  FILING_MENU_OPEN_PARENT,
#ifdef __EMSCRIPTEN__
  FILING_MENU_DOWNLOAD,
#endif
  FILING_MENU__LIMIT
};

/* icon bar menu rows */
enum
{
  FILING_IB_MENU_INFO,
  FILING_IB_MENU_OPEN_ROOT,
#ifdef __EMSCRIPTEN__
  FILING_IB_MENU_OPEN_FILEDROP,
#endif
  FILING_IB_MENU_CLOSE_ALL,
  FILING_IB_MENU_HIDE,
  FILING_IB_MENU__LIMIT
};

/* Set the directory a Filer's icon bar icon/System menu row opens, applied
 * the next time a Filer task is created (see filing_create). Not retroactive:
 * a Filer already running keeps the windows it has open. Call before
 * spawning the first Filer; the default is "." (the current directory), or
 * "/" when built for Emscripten.
 *
 * \param[in] root Directory path; copied. NULL restores the default. */
void filing_set_root(const char *root);

/* Start the app that opens files of type, for a DataOpen nobody claimed.
 * Returns result_OK once the app is installed, or result_NOT_FOUND if no app
 * opens that type. */
typedef result_t (*filing_run_fn_t)(filetype_t type);

/* Set the function a Filer calls when the DataOpen for a double-clicked file
 * bounces: the RISC OS Alias$@RunType_XXX equivalent. With none set, or if
 * it fails, the Filer reports that no application opens the file's type.
 *
 * \param[in] fn Run function, or NULL for none. */
void filing_set_run_fn(filing_run_fn_t fn);

/* window D's task: a RISC OS-style Filer. A System menu row and an icon bar
 * icon both open (or, if already open, bring to the front) a window on the
 * root directory (see filing_set_root); each subsequent window is opened by
 * double-clicking a directory entry in an existing window, one window per
 * path, never duplicated. Double-clicking a file broadcasts a recorded
 * wuss_MESSAGE_DATA_OPEN (see wuss_data_open_t) for a running app to claim;
 * if it bounces, the Filer starts the app for the file's type through the
 * run function (see filing_set_run_fn) and broadcasts it once more, then
 * reports an error if that bounces too. A double ADJUST click on a file also
 * closes its window, as for a directory. Dragging a
 * file out of a window starts a core drag; when it ends over a window of
 * another task, the Filer sends that window a wuss_MESSAGE_DATA_LOAD whose
 * payload is the file's full path as a NUL-terminated string.
 *
 * Every window is a wuss_gridview_t over an io/dirlist listing of its
 * directory, titled with the directory's full path. As a DataSave drop
 * target (see wuss/message.h) it validates the dropped leafname, treats an
 * over-long resulting path as an error, overwrites an existing file of the
 * same name silently, and rescans its listing on DataLoad and whenever a
 * window opens; the drop point within the window is ignored, so a file
 * always lands in that window's own directory regardless of where it was
 * dropped. */
typedef struct filing_task filing_task_t;

typedef struct filing_window
{
  filing_task_t    *owner; /* for the shared glyphs, from wuss_gridview's
                           * opaque callbacks (filing_item/filing_activate) */
  wuss_window_t   *window;
  wuss_gridview_t *gridview;
  dirlist_t       *listing;
  char             path[DPTLIB_MAXPATH];
}
filing_window_t;

struct filing_task
{
  wuss_t                *wuss;
  wuss_task_t           *delegate;
  wuss_iconbar_icon_t   *iconbar_icon;

  filing_window_t         windows[FILING_MAX_WINDOWS];
  int                    nwindows; /* slots used so far; a closed window's
                                    * slot stays, window NULL, for reuse */

  const bitmap_t        *glyph_file; /* borrowed from the icon pool */
  const bitmap_t        *glyph_directory;
  const bitmap_t        *glyph_directory_open; /* for a directory with a
                                                * window open on it */
  const bitmap_t        *glyph_png;

  char                   drag_path[DPTLIB_MAXPATH]; /* file being dragged,
                                       * or "" */
  unsigned int           open_retry; /* my_ref of the DataOpen re-sent after
                                      * starting an app for it, or 0 */

  wuss_menu_handle_t     menu_handle;
  wuss_window_t         *menu_window; /* Filer window the menu was opened
                                       * over, or NULL */
  wuss_menu_item_t       menu_items[FILING_MENU__LIMIT];
  wuss_menu_t            menu;
  wuss_menu_item_t       ib_menu_items[FILING_IB_MENU__LIMIT];
  wuss_menu_t            ib_menu;
  int                    hidden; /* windows hidden by Hide */
};

wuss_window_fn_t filing_handle;

/* create the Filer task against the given wuss instance: an icon bar icon
 * and a System menu row, neither yet showing a window (see filing_set_root
 * for where the first window, opened on demand, points). if out is non-NULL,
 * the task block is also returned through it. */
result_t filing_create(wuss_t *wuss, filing_task_t **out);

/* create the Filer: it keeps its own icon bar icon, so there is nothing to
 * wrap and no window opens until the icon is clicked */
result_t filing_app_create(wuss_t *wuss);

/* free a task block allocated by filing_create; normally called by the
 * task's wuss_EVENT_QUIT handler, not by callers directly */
void filing_destroy(filing_task_t *task);

#endif /* WUSS_APP && WUSS_ICONBAR */

#endif /* TASKS_FILING_H */
