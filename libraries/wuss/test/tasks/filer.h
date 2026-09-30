/* wuss/test/tasks/filer.h -- directory display / drag-save target task */

#ifndef TASKS_FILER_H
#define TASKS_FILER_H

#if defined(WUSS_APP) && defined(WUSS_ICONBAR)

#include "framebuf/bitmap.h"
#include "io/dirlist.h"
#include "io/path.h"
#include "wuss/component/proginfo.h"
#include "wuss/gadget/gridview.h"
#include "wuss/iconbar.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* ponytail: fixed cap on the number of directory windows open at once, so
 * filer_task_t can hold them inline with no malloc bookkeeping. Raise it if
 * this ever feels cramped. */
#define FILER_MAX_WINDOWS 16

/* Set the directory a Filer's icon bar icon/System menu row opens, applied
 * the next time a Filer task is created (see filer_create). Not retroactive:
 * a Filer already running keeps the windows it has open. Call before
 * spawning the first Filer; the default is "." (the current directory), or
 * "/" when built for Emscripten.
 *
 * \param[in] root Directory path; copied. NULL restores the default. */
void filer_set_root(const char *root);

/* window D's task: a RISC OS-style Filer. A System menu row and an icon bar
 * icon both open (or, if already open, bring to the front) a window on the
 * root directory (see filer_set_root); each subsequent window is opened by
 * double-clicking a directory entry in an existing window, one window per
 * path, never duplicated. Double-clicking a file does nothing (v1 has no
 * load side).
 *
 * Every window is a wuss_gridview_t over an io/dirlist listing of its
 * directory, titled with the directory's full path. As a DataSave drop
 * target (see wuss/message.h) it validates the dropped leafname, treats an
 * over-long resulting path as an error, overwrites an existing file of the
 * same name silently, and rescans its listing on DataLoad and whenever a
 * window opens; the drop point within the window is ignored, so a file
 * always lands in that window's own directory regardless of where it was
 * dropped. */
typedef struct filer_task filer_task_t;

typedef struct filer_window
{
  filer_task_t    *owner; /* for the shared glyphs, from wuss_gridview's
                           * opaque callbacks (filer_item/filer_activate) */
  wuss_window_t   *window;
  wuss_gridview_t *gridview;
  dirlist_t       *listing;
  char             path[DPTLIB_MAXPATH];
}
filer_window_t;

struct filer_task
{
  wuss_t                *wuss;
  wuss_task_t           *delegate;
  wuss_iconbar_icon_t   *iconbar_icon;

  filer_window_t         windows[FILER_MAX_WINDOWS];
  int                    nwindows;

  bitmap_t               glyph_file;
  bitmap_t               glyph_directory;
  bitmap_t               glyph_png;
  int                    have_glyph_file;
  int                    have_glyph_directory;
  int                    have_glyph_png;

  wuss_menu_handle_t     menu_handle;
  wuss_menu_item_t       menu_items[1]; /* Info */
  wuss_menu_t            menu;
};

wuss_window_fn_t filer_handle;

/* create the Filer task against the given wuss instance: an icon bar icon
 * and a System menu row, neither yet showing a window (see filer_set_root
 * for where the first window, opened on demand, points). if out is non-NULL,
 * the task block is also returned through it. */
result_t filer_create(wuss_t *wuss, filer_task_t **out);

/* free a task block allocated by filer_create; normally called by the
 * task's wuss_EVENT_QUIT handler, not by callers directly */
void filer_destroy(filer_task_t *task);

#endif /* WUSS_APP && WUSS_ICONBAR */

#endif /* TASKS_FILER_H */
