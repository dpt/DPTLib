/* wuss/component/proginfo.c -- a "program information" standard dialogue */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "geom/box.h"
#include "geom/size.h"

#include "wuss/icon.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "wuss/component/proginfo.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

/* ponytail: flat list of LABEL icons, no bevelled field borders, no "Web
 * site" action button -- the four text rows are the whole dialogue. Add a
 * button row through here if a task ever needs one. */

#define PROGINFO_MARGIN   12 /* px border around the row block */
#define PROGINFO_GAP      12 /* px between the name column and the value column */
#define PROGINFO_ROW_PAD   6 /* px added to the font height for the row pitch */
#define PROGINFO_FIELD_PAD 4 /* px each side of the text inside a value field */

/* The rows, in display order. Only fields set in the desc get a row. */
static const char *const proginfo_labels[] =
{
  "Name", "Purpose", "Author", "Version"
};

/* The dialogue is one window on a task the caller passes in. It carries
 * wuss_WINDOW_NO_CLOSE so a menu chain that borrows it as a
 * wuss_menu_item_t::window can't have it pulled from under it, and
 * wuss_proginfo_destroy closes it explicitly. The task must therefore not be
 * an autoclose one -- its window list would never empty -- and must outlive
 * the handle. */
struct wuss_proginfo
{
  wuss_alloc_t   alloc;  /* copied hooks; the wuss_t itself is not retained */
  wuss_window_t *window; /* owned; closed by wuss_proginfo_destroy */
};

/* ----------------------------------------------------------------------- */

result_t wuss_proginfo_create(wuss_proginfo_t           **out,
                              wuss_task_t                *task,
                              const wuss_proginfo_desc_t *desc)
{
  result_t            rc;
  const wuss_alloc_t *a;
  wuss_t             *wuss;
  bmfont_t           *font;
  wuss_proginfo_t    *pi;
  const char         *values[4];
  int                 nrows;
  int                 fonth;
  int                 rowh;
  int                 namew;
  int                 valuew;
  int                 w;
  int                 h;
  box_t               content;
  wuss_icon_spec_t    specs[8];
  int                 nspecs;
  int                 i;

  if (out == NULL || task == NULL || desc == NULL || desc->name == NULL)
    return result_NULL_ARG;

  wuss = task->wuss;
  a    = &wuss->alloc;
  font = wuss_get_font(wuss);

  values[0] = desc->name;
  values[1] = desc->purpose;
  values[2] = desc->author;
  values[3] = desc->version;

  /* Column widths: the widest name label on the left, the widest value on the
   * right. bmfont_measure with no target width returns the whole string's
   * width in actual_width. A NULL font leaves both at 0; the icons still lay
   * out, just with nothing measured to size them. */
  nrows  = 0;
  namew  = 0;
  valuew = 0;
  for (i = 0; i < 4; i++)
  {
    bmfont_width_t lw;
    bmfont_width_t vw;

    if (values[i] == NULL)
      continue;

    lw = vw = 0;
    if (font != NULL)
    {
      size_t vlen;

      vlen = strlen(values[i]);
      bmfont_measure(font, proginfo_labels[i], (int) strlen(proginfo_labels[i]),
                     INT_MAX, NULL, &lw);
      if (vlen > 0)
        bmfont_measure(font, values[i], (int) vlen, INT_MAX, NULL, &vw);
    }
    if (lw > namew)
      namew = lw;
    if (vw > valuew)
      valuew = vw;
    nrows++;
  }

  fonth = 0;
  if (font != NULL)
    bmfont_get_info(font, NULL, &fonth);
  rowh = fonth + PROGINFO_ROW_PAD;
  if (rowh < 12)
    rowh = 12;

  if (namew < 1)
    namew = 1;
  if (valuew < 1)
    valuew = 1;
  valuew += PROGINFO_FIELD_PAD * 2; /* room for the groove border + inset */

  w = PROGINFO_MARGIN * 2 + namew + PROGINFO_GAP + valuew;
  h = PROGINFO_MARGIN * 2 + rowh * nrows;

  pi = a->malloc(sizeof(*pi));
  if (pi == NULL)
    return result_OOM;
  pi->alloc  = *a;
  pi->window = NULL;
  a = &pi->alloc; /* use the copy from here on -- outlives the wuss_t */

  content = (box_t) BOX_POS_SIZE(0, 0, w, h);
  rc = wuss_window_create(task,
                          &content,
                          "About this program",
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_CLOSE |
                          wuss_WINDOW_NO_TOGGLE_SIZE | wuss_WINDOW_NO_VSCROLL |
                          wuss_WINDOW_NO_HSCROLL | wuss_WINDOW_NO_RESIZE |
                          wuss_WINDOW_NO_REDRAW | wuss_WINDOW_HIDDEN,
                          wuss_BACKDROP_COLOUR(wuss_COLOUR_WHITE),
                          SIZE2D(w, h),
                          SIZE2D(w, h),
                          &pi->window);
  if (rc != result_OK)
  {
    a->free(pi);
    return rc;
  }

  /* One right-justified name label and one left value label per row. Zero the
   * whole array so the icon-spec fields this component does not set (pattern,
   * bitmap, group, swatch) are their safe defaults. */
  memset(specs, 0, sizeof(specs));
  nspecs = 0;
  for (i = 0; i < 4; i++)
  {
    int y;

    if (values[i] == NULL)
      continue;

    y = PROGINFO_MARGIN + rowh * (nspecs / 2);

    specs[nspecs].bbox  = (box_t) BOX_POS_SIZE(PROGINFO_MARGIN, y, namew, rowh);
    specs[nspecs].type  = wuss_ICON_TYPE_LABEL;
    specs[nspecs].text  = proginfo_labels[i];
    specs[nspecs].fg    = wuss_COLOUR_BLACK;
    specs[nspecs].bg    = wuss_NO_BACKGROUND;
    specs[nspecs].flags = wuss_ICON_FLAGS_JUSTIFY_RIGHT;
    nspecs++;

    specs[nspecs].bbox   = (box_t) BOX_POS_SIZE(PROGINFO_MARGIN + namew +
                                               PROGINFO_GAP, y, valuew, rowh);
    specs[nspecs].type   = wuss_ICON_TYPE_LABEL;
    specs[nspecs].text   = values[i];
    specs[nspecs].fg     = wuss_COLOUR_BLACK;
    specs[nspecs].bg     = wuss_NO_BACKGROUND;
    specs[nspecs].border = wuss_ICON_BORDER_GROOVE; /* RISC OS display field */
    specs[nspecs].flags  = wuss_ICON_FLAGS_JUSTIFY_CENTRE;
    nspecs++;
  }

  rc = wuss_icon_create_array(pi->window, specs, nspecs, NULL);
  if (rc != result_OK)
  {
    wuss_window_close(pi->window); /* frees the window and any icons on it */
    a->free(pi);
    return rc;
  }

  *out = pi;
  return result_OK;
}

void wuss_proginfo_destroy(wuss_proginfo_t *doomed)
{
  wuss_alloc_t alloc;

  if (doomed == NULL)
    return;

  alloc = doomed->alloc;
  wuss_window_close(doomed->window); /* frees the window and its icons */
  alloc.free(doomed);
}

wuss_window_t *wuss_proginfo_window(const wuss_proginfo_t *pi)
{
  return pi ? pi->window : NULL;
}
