/* wuss/component/info.c -- a label:value grid dialogue */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "geom/box.h"
#include "geom/size.h"

#include "wuss/icon.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "wuss/component/info.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

/* ponytail: a flat grid of LABEL icons, two per row -- no action buttons, no
 * file-type sprite icon (the RISC OS Info dialogue's first field). Add either
 * through here if a component ever needs one. The spec array is on the stack,
 * so the row count is capped; INFO_MAX_ROWS is well past any real dialogue. */

#define INFO_MARGIN       4 /* px border around the row block */
#define INFO_GAP          4 /* px between the label column and the value column */
#define INFO_ROW_PAD     12 /* px added to the font height for the row pitch */
#define INFO_ROW_LEADING  4 /* px leading between rows */
#define INFO_FIELD_PAD    4 /* px each side of the text inside a value field */
#define INFO_MAX_ROWS    16

/* The dialogue is one window on a task the caller passes in. It omits
 * wuss_WINDOW_CLOSE so a menu chain that borrows it as a
 * wuss_menu_item_t::window can't have it pulled from under it, and
 * wuss_info_destroy closes it explicitly. The task must therefore not be an
 * autoclose one -- its window list would never empty -- and must outlive the
 * handle. */
struct wuss_info
{
  wuss_alloc_t   alloc;  /* copied hooks; the wuss_t itself is not retained */
  wuss_window_t *window; /* owned; closed by wuss_info_destroy */
};

/* ----------------------------------------------------------------------- */

/* Widest of "s" (whole string, no wrap) and "cur", in pixels; "cur"
 * unchanged when the font or string is absent. bmfont_measure with no target
 * width returns the whole string's width in its last out parameter. */
static int info_widen(bmfont_t *font, const char *s, int cur)
{
  bmfont_width_t w;

  if (font == NULL || s == NULL || *s == '\0')
    return cur;

  wuss__text_measure(font, s, (int) strlen(s), INT_MAX, NULL, &w);
  return MAX(cur, (int) w);
}

result_t wuss_info_create(wuss_info_t          **out,
                          wuss_task_t           *task,
                          const char            *title,
                          const wuss_info_row_t *rows,
                          int                    nrows)
{
  result_t         rc;
  wuss_t          *wuss;
  bmfont_t        *font;
  wuss_info_t     *info;
  int              fonth;
  int              rowh;
  int              fieldh;
  int              labelw;
  int              valuew;
  int              w;
  int              h;
  box_t            content;
  wuss_icon_spec_t specs[INFO_MAX_ROWS * 2];
  int              i;

  if (out == NULL || task == NULL || title == NULL || rows == NULL)
    return result_NULL_ARG;
  if (nrows < 1 || nrows > INFO_MAX_ROWS)
    return result_BAD_ARG;

  memset(specs, 0, sizeof(specs)); /* leave every unset spec.u arm zeroed */

  wuss = task->wuss;
  font = wuss_get_font(wuss);

  /* Column widths: the widest label on the left, the widest value on the
   * right. A NULL font leaves both at 0; the icons still lay out, just with
   * nothing measured to size them. */
  labelw = 0;
  valuew = 0;
  for (i = 0; i < nrows; i++)
  {
    labelw = info_widen(font, rows[i].label, labelw);
    valuew = info_widen(font, rows[i].value, valuew);
  }

  fonth = 0;
  if (font != NULL)
    bmfont_get_info(font, NULL, &fonth, NULL, NULL);
  rowh   = MAX(fonth + INFO_ROW_PAD, 12) + INFO_ROW_LEADING;
  fieldh = rowh - INFO_ROW_LEADING; /* rowh is the pitch; leave a gap */

  labelw = MAX(labelw, 1);
  valuew = MAX(valuew, 1) + INFO_FIELD_PAD * 2; /* groove border + inset */

  w = INFO_MARGIN * 2 + labelw + INFO_GAP + valuew;
  /* rowh is the pitch (field + leading); the last row has no trailing leading */
  h = INFO_MARGIN * 2 + rowh * nrows - INFO_ROW_LEADING;

  info = wuss->alloc.malloc(sizeof(*info));
  if (info == NULL)
    return result_OOM;
  info->alloc  = wuss->alloc; /* the copy outlives the wuss_t */
  info->window = NULL;

  content = (box_t) BOX_POS_SIZE(0, 0, w, h);
  rc = wuss_window_create(task,
                          &content,
                          title,
                          wuss_WINDOW_NO_REDRAW | wuss_WINDOW_HIDDEN,
                          wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                          SIZE2D(w, h),
                          SIZE2D(w, h),
                          &info->window);
  if (rc != result_OK)
  {
    info->alloc.free(info);
    return rc;
  }

  /* One right-justified label and one centred value per row. Zeroed so the
   * icon-spec fields this component does not set (pattern, bitmap, group,
   * swatch) are their safe defaults. wuss_icon_create deep-copies each spec
   * and its text, so this stack array can go out of scope after the call. */
  memset(specs, 0, sizeof(specs));
  for (i = 0; i < nrows; i++)
  {
    wuss_icon_spec_t *label;
    wuss_icon_spec_t *value;
    int               y;

    label = &specs[i * 2];
    value = &specs[i * 2 + 1];
    y     = INFO_MARGIN + rowh * i;

    label->bbox  = (box_t) BOX_POS_SIZE(INFO_MARGIN, y, labelw, fieldh);
    label->type  = wuss_ICON_TYPE_LABEL;
    label->text  = rows[i].label;
    label->fg    = wuss_COLOUR_BLACK;
    label->bg    = wuss_NO_BACKGROUND;
    label->flags = wuss_ICON_FLAGS_JUSTIFY_RIGHT;

    value->bbox   = (box_t) BOX_POS_SIZE(INFO_MARGIN + labelw + INFO_GAP,
                                        y, valuew, fieldh);
    value->type   = wuss_ICON_TYPE_LABEL;
    value->text   = rows[i].value;
    value->fg     = wuss_COLOUR_BLACK;
    value->bg     = wuss_NO_BACKGROUND;
    value->u.label.border = wuss_ICON_BORDER_GROOVE; /* RISC OS display field */
    value->flags  = wuss_ICON_FLAGS_JUSTIFY_CENTRE;
  }

  rc = wuss_icon_create_array(info->window, specs, nrows * 2, NULL);
  if (rc != result_OK)
  {
    wuss_window_close(info->window); /* frees the window and any icons on it */
    info->alloc.free(info);
    return rc;
  }

  *out = info;
  return result_OK;
}

void wuss_info_destroy(wuss_info_t *doomed)
{
  wuss_alloc_t alloc;

  if (doomed == NULL)
    return;

  alloc = doomed->alloc;
  wuss_window_close(doomed->window); /* frees the window and its icons */
  alloc.free(doomed);
}

wuss_window_t *wuss_info_window(const wuss_info_t *info)
{
  return info ? info->window : NULL;
}
