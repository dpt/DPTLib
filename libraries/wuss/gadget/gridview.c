/* wuss/gadget/gridview.c -- a glyph+label grid on a caller's window */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "datastruct/bitvec.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

#include "wuss/icon.h"
#include "wuss/window.h"

#include "wuss/gadget/gridview.h"

#include "../core/impl.h"
#include "../font/font.h"
#include "../icon.h"

/* ----------------------------------------------------------------------- */

#define GRIDVIEW_ELLIPSIS "..."

struct wuss_gridview
{
  wuss_alloc_t                  alloc;   /* copied hooks; wuss_t not retained */
  wuss_window_t                 *window;
  int                            count;
  wuss_gridview_item_fn_t       *item_fn;
  wuss_gridview_activate_fn_t   *activate;
  void                          *opaque;
  bitvec_t                      *selected;
  int                            label_chars;
  size2d_t                       cell;    /* glyph+label cell size, with padding */
  int                            columns; /* set by reflow; at least 1 */
};

/* ----------------------------------------------------------------------- */

/* Cell size: the largest glyph's box combined with "label_chars" characters
 * of the window's font, plus padding, stacked glyph-over-label. Walks every
 * item once, at create time only -- the plan fixes cell size for the
 * gadget's lifetime so one long name cannot inflate the grid later. */
static size2d_t gridview_measure_cell(wuss_gridview_t *gv)
{
  wuss_t              *wuss;
  bmfont_t            *font;
  wuss_gridview_item_t item;
  bmfont_width_t       char_w, label_w;
  int                  glyph_w, glyph_h, i;

  wuss    = gv->window->task->wuss;
  font    = wuss->fonts.fonts[0];
  glyph_w = 0;
  glyph_h = 0;

  for (i = 0; i < gv->count; i++)
  {
    gv->item_fn(i, gv->opaque, &item);
    if (item.glyph == NULL)
      continue;
    glyph_w = MAX(glyph_w, item.glyph->size.w);
    glyph_h = MAX(glyph_h, item.glyph->size.h);
  }

  label_w = 0;
  if (font != NULL)
  {
    char_w = 0;
    wuss__text_measure(font, "M", 1, INT_MAX, NULL, &char_w);
    label_w = char_w * (bmfont_width_t) gv->label_chars;
  }

  return SIZE2D(MAX(glyph_w, (int) label_w) + 2 * wuss_STD_GAP,
               glyph_h + wuss__fontset_height(&wuss->fonts, 0) +
               3 * wuss_STD_GAP);
}

result_t wuss_gridview_create(wuss_gridview_t            **out,
                              wuss_window_t               *window,
                              int                          count,
                              wuss_gridview_item_fn_t     *item_fn,
                              int                          label_chars,
                              wuss_gridview_activate_fn_t *activate,
                              void                        *opaque)
{
  wuss_t          *wuss;
  wuss_gridview_t *gv;

  if (out == NULL || window == NULL || item_fn == NULL)
    return result_NULL_ARG;

  if (count < 0 || label_chars < 1)
    return result_BAD_ARG;

  wuss = window->task->wuss;

  gv = wuss->alloc.malloc(sizeof(*gv));
  if (gv == NULL)
    return result_OOM;

  gv->selected = bitvec_create((unsigned int) count);
  if (gv->selected == NULL)
  {
    wuss->alloc.free(gv);
    return result_OOM;
  }

  gv->alloc       = wuss->alloc;
  gv->window      = window;
  gv->count       = count;
  gv->item_fn     = item_fn;
  gv->activate    = activate;
  gv->opaque      = opaque;
  gv->label_chars = label_chars;
  gv->columns     = 1;
  gv->cell        = gridview_measure_cell(gv);

  *out = gv;
  return result_OK;
}

void wuss_gridview_destroy(wuss_gridview_t *doomed)
{
  if (doomed == NULL)
    return;

  bitvec_destroy(doomed->selected);
  doomed->alloc.free(doomed);
}

/* ----------------------------------------------------------------------- */

/* Recompute column count from the window's current visible content width,
 * and set the document extent to match the resulting row count. */
static void gridview_reflow(wuss_gridview_t *gv)
{
  box_t    content;
  size2d_t doc;
  int      visible_w, rows;

  wuss_window_get_content_bounds(gv->window, &content);
  visible_w   = content.x1 - content.x0;
  gv->columns = MAX(visible_w / gv->cell.w, 1);

  rows  = (gv->count + gv->columns - 1) / gv->columns;
  doc.w = gv->columns * gv->cell.w;
  doc.h = MAX(rows, 1) * gv->cell.h;

  /* doc.w/doc.h are always positive (cell size, times at least 1), so this
   * cannot return result_WUSS_TOO_SMALL */
  (void) wuss_window_set_doc(gv->window, doc);
}

/* Document-space bbox of cell "index". */
static box_t gridview_cell_box(const wuss_gridview_t *gv, int index)
{
  int col, row;

  col = index % gv->columns;
  row = index / gv->columns;
  return (box_t) BOX_POS_SIZE(col * gv->cell.w, row * gv->cell.h,
                              gv->cell.w, gv->cell.h);
}

/* Item index under document point "p", or -1 if none (including gaps and
 * past the last item). */
static int gridview_hit_test(const wuss_gridview_t *gv, point_t p)
{
  int col, row, index;

  if (p.x < 0 || p.y < 0)
    return -1;

  col = p.x / gv->cell.w;
  row = p.y / gv->cell.h;
  if (col >= gv->columns)
    return -1;

  index = row * gv->columns + col;
  return (index >= 0 && index < gv->count) ? index : -1;
}

/* ----------------------------------------------------------------------- */

static void gridview_draw_cell(wuss_gridview_t    *gv,
                               const wuss_event_t *event,
                               int                 index)
{
  wuss_t              *wuss;
  screen_t            *scr;
  bmfont_t            *font;
  wuss_gridview_item_t item;
  box_t                doc_box, screen_box, clipped_clip;
  screen_t             clipped;
  colour_t             fg, bg;
  point_t              pos;
  bmfont_width_t       w;
  int                  len, split, ascent, height;
  int                  glyph_x, glyph_y, label_y;
  char                 buf[64];

  wuss = gv->window->task->wuss;
  scr  = event->data.redraw.scr;
  font = wuss->fonts.fonts[0];

  doc_box = gridview_cell_box(gv, index);
  wuss__icon_box_to_screen(event->data.redraw.bounds,
                          event->data.redraw.scroll, &doc_box, &screen_box);
  if (box_intersection(&scr->clip, &screen_box, &clipped_clip))
    return;
  clipped      = *scr;
  clipped.clip = clipped_clip;

  gv->item_fn(index, gv->opaque, &item);

  fg = wuss->palette[wuss->button_fg];
  bg = wuss_gridview_is_selected(gv, index)
     ? wuss->palette[wuss->accent]
     : wuss->palette[wuss->window_bg];
  screen_fill_rect(&clipped, screen_box.x0, screen_box.y0,
                   box_size(&screen_box), bg);
  if (wuss_gridview_is_selected(gv, index))
  {
    colour_t tmp = fg;
    fg = bg;
    bg = tmp;
  }

  if (item.glyph != NULL)
  {
    glyph_x = screen_box.x0 + (gv->cell.w - item.glyph->size.w) / 2;
    glyph_y = screen_box.y0 + wuss_STD_GAP;
    screen_copy_bitmap(&clipped, glyph_x, glyph_y, item.glyph);
  }

  if (item.label != NULL && font != NULL)
  {
    bmfont_get_info(font, NULL, &height, &ascent, NULL);

    len = (int) strlen(item.label);
    if (len >= (int) sizeof(buf))
      len = (int) sizeof(buf) - 1;
    wuss__text_measure(font, item.label, len, INT_MAX, NULL, &w);
    if ((int) w > gv->cell.w - 2 * wuss_STD_GAP)
    {
      bmfont_width_t ellipsis_w, target;

      wuss__text_measure(font, GRIDVIEW_ELLIPSIS,
                        (int) strlen(GRIDVIEW_ELLIPSIS), INT_MAX, NULL,
                        &ellipsis_w);
      target = (bmfont_width_t) MAX(gv->cell.w - 2 * wuss_STD_GAP -
                                    (int) ellipsis_w, 0);
      wuss__text_measure(font, item.label, len, target, &split, &w);
      if (split > (int) sizeof(buf) - (int) sizeof(GRIDVIEW_ELLIPSIS))
        split = (int) sizeof(buf) - (int) sizeof(GRIDVIEW_ELLIPSIS);
      memcpy(buf, item.label, (size_t) split);
      memcpy(buf + split, GRIDVIEW_ELLIPSIS, sizeof(GRIDVIEW_ELLIPSIS));
      len = split + (int) sizeof(GRIDVIEW_ELLIPSIS) - 1;
      wuss__text_measure(font, buf, len, INT_MAX, NULL, &w);
    }
    else
    {
      memcpy(buf, item.label, (size_t) len);
    }

    label_y = screen_box.y1 - wuss_STD_GAP - height;
    pos.x   = screen_box.x0 + (gv->cell.w - (int) w) / 2;
    pos.y   = label_y + ascent;
    wuss__text_draw(font, &clipped, buf, len, fg, bg, &pos, NULL);
  }
}

/* ----------------------------------------------------------------------- */

int wuss_gridview_handle_event(wuss_gridview_t    *gv,
                               const wuss_event_t *event)
{
  int index, i;

  switch (event->kind)
  {
  case wuss_EVENT_OPEN:
    gridview_reflow(gv);
    return 0; /* other listeners may care about OPEN too */

  case wuss_EVENT_REDRAW:
    for (i = 0; i < gv->count; i++)
      gridview_draw_cell(gv, event, i);
    return 1;

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return 0;

    index = gridview_hit_test(gv, event->data.mouse.point);

    if (index < 0)
    {
      if (bitvec_count(gv->selected) == 0)
        return 0;
      bitvec_clear_all(gv->selected);
      wuss_window_invalidate_visible(gv->window);
      return 1;
    }

    if (event->data.mouse.button & wuss_BUTTON_ADJUST)
    {
      if (bitvec_get(gv->selected, (bitvec_index_t) index))
        bitvec_clear(gv->selected, (bitvec_index_t) index);
      else
        (void) bitvec_set(gv->selected, (bitvec_index_t) index);
    }
    else if (event->data.mouse.button & wuss_BUTTON_SELECT)
    {
      bitvec_clear_all(gv->selected);
      (void) bitvec_set(gv->selected, (bitvec_index_t) index);
    }
    wuss_window_invalidate_visible(gv->window);

    if ((event->data.mouse.button & wuss_BUTTON_DOUBLE) &&
        gv->activate != NULL)
      gv->activate(gv, index, gv->opaque);

    return 1;

  default:
    return 0;
  }
}

/* ----------------------------------------------------------------------- */

int wuss_gridview_is_selected(const wuss_gridview_t *gv, int index)
{
  return bitvec_get(gv->selected, (bitvec_index_t) index);
}

void wuss_gridview_clear_selection(wuss_gridview_t *gv)
{
  if (bitvec_count(gv->selected) == 0)
    return;

  bitvec_clear_all(gv->selected);
  wuss_window_invalidate_visible(gv->window);
}

result_t wuss_gridview_set_count(wuss_gridview_t *gv, int count)
{
  bitvec_t *fresh;
  int       i;

  if (count < 0)
    return result_BAD_ARG;

  fresh = bitvec_create((unsigned int) count);
  if (fresh == NULL)
    return result_OOM;

  for (i = 0; i < count && i < gv->count; i++)
    if (bitvec_get(gv->selected, (bitvec_index_t) i))
      (void) bitvec_set(fresh, (bitvec_index_t) i);

  bitvec_destroy(gv->selected);
  gv->selected = fresh;
  gv->count    = count;

  gridview_reflow(gv);
  wuss_window_invalidate_visible(gv->window);
  return result_OK;
}
