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

static void gridview_reflow(wuss_gridview_t *gv);

struct wuss_gridview
{
  wuss_alloc_t                  alloc;   /* copied hooks; wuss_t not retained */
  wuss_window_t                 *window;
  int                            count;
  wuss_gridview_item_fn_t       *item_fn;
  wuss_gridview_activate_fn_t   *activate;
  wuss_gridview_drag_fn_t       *drag;    /* NULL: items are not draggable */
  int                            press_index; /* item pressed, -1 for none */
  point_t                        press_point; /* its document point */
  void                          *opaque;
  bitvec_t                      *selected;
  int                            label_chars;
  size2d_t                       cell;    /* glyph+label cell size, with padding */
  size2d_t                       content; /* widest glyph/label, tallest glyph */
  bmfont_t                      *measured_font; /* font last measured with */
  int                            measured_height; /* its height then */
  int                            pad_x;   /* cell side edge to its contents */
  int                            pad_y;   /* cell top/bottom edge to contents */
  int                            spacing; /* glyph to label */
  int                            columns; /* set by reflow; at least 1 once
                                           * reflowed, 0 before */
  size2d_t                       doc;     /* extent last given the window */
  int                            inset;   /* margin round the whole grid */
  int                            max_columns; /* 0 for no limit */
  int                            pow2;    /* columns restricted to 1, 2, 4... */
  int                            selectable;
  int                            fixed_cell; /* cell set by the caller */
  int                            has_bg;  /* bg replaces window_bg */
  colour_t                       bg;
  wuss_gridview_draw_fn_t       *draw;    /* NULL: draw glyph and label */
};

/* ----------------------------------------------------------------------- */

/* Walk every item once and record the widest content (its glyph or its
 * measured label, the label capped at "label_chars" characters of the
 * window's font) and the tallest glyph in gv->content. Costs a text measure
 * per item, so it runs only when the items or the font may have changed --
 * the label cap stops one long name inflating the grid. */
static void gridview_measure_items(wuss_gridview_t *gv)
{
  wuss_t              *wuss;
  bmfont_t            *font;
  wuss_gridview_item_t item;
  bmfont_width_t       char_w, text_w;
  int                  glyph_w, glyph_h, label_w, cap_w, i;

  wuss    = gv->window->task->wuss;
  font    = wuss->fonts.fonts[0];
  glyph_w = 0;
  glyph_h = 0;
  label_w = 0;
  cap_w   = 0;

  if (font != NULL)
  {
    char_w = 0;
    wuss__text_measure(font, "M", 1, INT_MAX, NULL, &char_w);
    cap_w = (int) char_w * gv->label_chars;
  }

  for (i = 0; i < gv->count; i++)
  {
    gv->item_fn(i, gv->opaque, &item);
    if (item.glyph != NULL)
    {
      glyph_w = MAX(glyph_w, item.glyph->size.w);
      glyph_h = MAX(glyph_h, item.glyph->size.h);
    }

    if (item.label != NULL && font != NULL)
    {
      text_w = 0;
      wuss__text_measure(font, item.label, (int) strlen(item.label), INT_MAX,
                        NULL, &text_w);
      label_w = MAX(label_w, MIN((int) text_w, cap_w));
    }
  }

  gv->content         = SIZE2D(MAX(glyph_w, label_w), glyph_h);
  gv->measured_font   = font;
  gv->measured_height = wuss__fontset_height(&wuss->fonts, 0);
}

/* Cell size from the cached content size, the padding and the font height:
 * glyph stacked over label. Cheap, so safe to call whenever padding changes. */
static size2d_t gridview_cell_size(const wuss_gridview_t *gv)
{
  const wuss_t *wuss;

  wuss = gv->window->task->wuss;

  return SIZE2D(gv->content.w + 2 * gv->pad_x,
               gv->content.h + wuss__fontset_height(&wuss->fonts, 0) +
               2 * gv->pad_y + gv->spacing);
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
  gv->drag        = NULL;
  gv->press_index = -1;
  gv->opaque      = opaque;
  gv->label_chars = label_chars;
  gv->columns     = 0;
  gv->doc         = SIZE2D(0, 0);
  gv->inset       = 0;
  gv->max_columns = 0;
  gv->pow2        = 0;
  gv->selectable  = 1;
  gv->fixed_cell  = 0;
  gv->pad_x       = wuss_STD_GAP * 2;
  gv->pad_y       = wuss_STD_GAP;
  gv->spacing     = wuss_STD_GAP;
  gv->has_bg      = 0;
  gv->draw        = NULL;
  gridview_measure_items(gv);
  gv->cell        = gridview_cell_size(gv);

  gridview_reflow(gv);

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

/* Columns that fit "width" under the layout policy: at least 1, at most the
 * item count and max_columns, and a power of two when pow2 is set. */
static int gridview_columns_for_width(const wuss_gridview_t *gv, int width)
{
  int cols, p;

  cols = (width - 2 * gv->inset) / gv->cell.w;
  cols = MAX(cols, 1);
  if (gv->max_columns > 0)
    cols = MIN(cols, gv->max_columns);
  else
    cols = MIN(cols, MAX(gv->count, 1));

  if (gv->pow2)
  {
    p = 1;
    while (p * 2 <= cols)
      p *= 2;
    cols = p;
  }

  return cols;
}

static int gridview_width_for_columns(const wuss_gridview_t *gv, int cols)
{
  return 2 * gv->inset + cols * gv->cell.w;
}

/* Recompute the column count from the window's current content width, and
 * set the document extent to match. The extent is as wide as the widest
 * layout this screen and item count allow, so the window can always be
 * dragged wider; it is as tall as the rows the current layout needs. Does
 * nothing if neither the columns nor the extent changed. */
static void gridview_reflow(wuss_gridview_t *gv)
{
  box_t    content;
  size2d_t doc, max;
  int      cols, max_cols, rows;

  wuss_window_get_content_bounds(gv->window, &content);
  wuss_window_get_max_content(gv->window, &max);

  cols     = gridview_columns_for_width(gv, content.x1 - content.x0);
  max_cols = MAX(gridview_columns_for_width(gv, max.w), cols);
  rows     = (gv->count + cols - 1) / cols;

  /* always positive: cell size and at least one column and row */
  doc = SIZE2D(gridview_width_for_columns(gv, max_cols),
               MAX(rows, 1) * gv->cell.h + 2 * gv->inset);

  /* cannot fail: the width is positive */
  (void) wuss_window_set_min_size(gv->window,
                                  SIZE2D(gridview_width_for_columns(gv, 1),
                                         wuss_KEEP));

  if (cols == gv->columns && doc.w == gv->doc.w && doc.h == gv->doc.h)
    return;

  gv->columns = cols;
  gv->doc     = doc;

  (void) wuss_window_set_doc(gv->window, doc);
}

/* Document-space bbox of cell "index". */
static box_t gridview_cell_box(const wuss_gridview_t *gv, int index)
{
  int col, row;

  col = index % gv->columns;
  row = index / gv->columns;
  return (box_t) BOX_POS_SIZE(gv->inset + col * gv->cell.w,
                              gv->inset + row * gv->cell.h,
                              gv->cell.w, gv->cell.h);
}

/* Item index under document point "p", or -1 if none (including gaps and
 * past the last item). */
static int gridview_hit_test(const wuss_gridview_t *gv, point_t p)
{
  int col, row, index;

  p.x -= gv->inset;
  p.y -= gv->inset;
  if (p.x < 0 || p.y < 0)
    return -1;

  col = p.x / gv->cell.w;
  row = p.y / gv->cell.h;
  if (col >= gv->columns)
    return -1;

  index = row * gv->columns + col;
  return (index >= 0 && index < gv->count) ? index : -1;
}

/* Mark one cell dirty, leaving the rest of the window alone. */
static void gridview_invalidate_cell(wuss_gridview_t *gv, int index)
{
  box_t box;

  box = gridview_cell_box(gv, index);
  wuss_window_invalidate(gv->window, &box);
}

/* The pointer moved with an item pressed: past the drag threshold, offer the
 * item to the drag callback and, if it takes it, start a core drag sized to
 * the cell. Hover moves also arrive here, so a press the pointer left the
 * window during (see wuss_EVENT_POINTER_EXIT) must already have been
 * forgotten. */
static void gridview_drag_maybe(wuss_gridview_t *gv, point_t point)
{
  wuss_t *wuss;
  box_t   cell;
  point_t hotspot;
  int     threshold;
  int     index;
  int     dx;
  int     dy;

  if (gv->press_index < 0)
    return;

  wuss      = gv->window->task->wuss;
  threshold = wuss->input.drag_threshold_px;
  dx        = point.x - gv->press_point.x;
  dy        = point.y - gv->press_point.y;
  if (dx * dx + dy * dy < threshold * threshold)
    return;

  index           = gv->press_index;
  gv->press_index = -1;

  if (!gv->drag(gv, index, gv->opaque))
    return;

  cell    = gridview_cell_box(gv, index);
  hotspot = POINT(gv->press_point.x - cell.x0, gv->press_point.y - cell.y0);
  (void) wuss_drag_start(wuss, gv->window, gv->cell, hotspot);
}

/* Deselect everything, dirtying only the cells that were selected. */
static void gridview_clear_selected(wuss_gridview_t *gv)
{
  int i;

  for (i = bitvec_next(gv->selected, -1); i >= 0;
       i = bitvec_next(gv->selected, i))
    gridview_invalidate_cell(gv, i);

  bitvec_clear_all(gv->selected);
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
  if (gv->draw != NULL)
  {
    box_t saved;

    saved    = scr->clip;
    scr->clip = clipped_clip;
    gv->draw(gv, event, index, &screen_box, gv->opaque);
    scr->clip = saved;
    return;
  }

  clipped      = *scr;
  clipped.clip = clipped_clip;

  gv->item_fn(index, gv->opaque, &item);

  fg = wuss->palette[wuss->button.fg];
  bg = wuss_gridview_is_selected(gv, index)
     ? wuss->palette[wuss->accent]
     : wuss->palette[wuss->body.window];
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
    glyph_y = screen_box.y0 + gv->pad_y;
    screen_copy_bitmap(&clipped, glyph_x, glyph_y, item.glyph);
  }

  if (item.label != NULL && font != NULL)
  {
    bmfont_get_info(font, NULL, &height, &ascent, NULL);

    len = (int) strlen(item.label);
    if (len >= (int) sizeof(buf))
      len = (int) sizeof(buf) - 1;
    wuss__text_measure(font, item.label, len, INT_MAX, NULL, &w);
    if ((int) w > gv->cell.w - 2 * gv->pad_x)
    {
      bmfont_width_t ellipsis_w, target;

      wuss__text_measure(font, GRIDVIEW_ELLIPSIS,
                        (int) strlen(GRIDVIEW_ELLIPSIS), INT_MAX, NULL,
                        &ellipsis_w);
      target = (bmfont_width_t) MAX(gv->cell.w - 2 * gv->pad_x -
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

    label_y = screen_box.y1 - gv->pad_y - height;
    pos.x   = screen_box.x0 + (gv->cell.w - (int) w) / 2;
    pos.y   = label_y + ascent;
    wuss__text_draw(font, &clipped, buf, len, fg, bg, &pos, NULL);
  }
}

/* ----------------------------------------------------------------------- */

int wuss_gridview_handle_event(wuss_gridview_t    *gv,
                               const wuss_event_t *event)
{
  int index, i, first, last;

  switch (event->kind)
  {
  case wuss_EVENT_OPEN:
    gridview_reflow(gv);
    return 0; /* other listeners may care about OPEN too */

  case wuss_EVENT_PALETTE:
  case wuss_EVENT_RELAYOUT:
    wuss_gridview_remeasure(gv);
    return 0;

  case wuss_EVENT_REDRAW:
  {
    wuss_t *wuss;
    box_t   content;

    wuss    = gv->window->task->wuss;
    content = *event->data.redraw.content;
    screen_fill_rect(event->data.redraw.scr, content.x0, content.y0,
                     box_size(&content),
                     gv->has_bg ? gv->bg : wuss->palette[wuss->body.window]);

    /* only the rows the clip reaches */
    first = (event->data.redraw.scr->clip.y0 - event->data.redraw.bounds->y0 +
             event->data.redraw.scroll.y - gv->inset) / gv->cell.h;
    last  = (event->data.redraw.scr->clip.y1 - event->data.redraw.bounds->y0 +
             event->data.redraw.scroll.y - gv->inset) / gv->cell.h;
    first = MAX(first, 0) * gv->columns;
    last  = MIN((last + 1) * gv->columns, gv->count);
    for (i = first; i < last; i++)
      gridview_draw_cell(gv, event, i);
    return 1;
  }

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action == wuss_MOUSE_MOVE)
    {
      gridview_drag_maybe(gv, event->data.mouse.point);
      return 0;
    }

    if (event->data.mouse.action == wuss_MOUSE_UP)
    {
      gv->press_index = -1;
      return 0;
    }

    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return 0;

    gv->press_index = -1;

    /* a MENU click leaves the selection and the display alone */
    if (!(event->data.mouse.button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      return 0;

    index = gridview_hit_test(gv, event->data.mouse.point);

    if (index < 0)
    {
      /* Adjust on the background leaves the selection alone */
      if (!gv->selectable || !(event->data.mouse.button & wuss_BUTTON_SELECT) ||
          bitvec_count(gv->selected) == 0)
        return 0;
      gridview_clear_selected(gv);
      return 1;
    }

    if (gv->drag != NULL)
    {
      gv->press_index = index;
      gv->press_point = event->data.mouse.point;
    }

    if (!gv->selectable)
    {
      /* no selection state to change */
    }
    else if (event->data.mouse.button & wuss_BUTTON_ADJUST)
    {
      if (bitvec_get(gv->selected, (bitvec_index_t) index))
        bitvec_clear(gv->selected, (bitvec_index_t) index);
      else
        (void) bitvec_set(gv->selected, (bitvec_index_t) index);
      gridview_invalidate_cell(gv, index);
    }
    else if ((event->data.mouse.button & wuss_BUTTON_SELECT) &&
             !bitvec_get(gv->selected, (bitvec_index_t) index))
    {
      /* a Select on an item already selected changes nothing, so the whole
       * selection can still be dragged */
      gridview_clear_selected(gv);
      (void) bitvec_set(gv->selected, (bitvec_index_t) index);
      gridview_invalidate_cell(gv, index);
    }

    if ((event->data.mouse.button & wuss_BUTTON_DOUBLE) &&
        gv->activate != NULL)
    {
      /* before the call: it may rescan or reorder the items */
      if (bitvec_get(gv->selected, (bitvec_index_t) index))
      {
        bitvec_clear(gv->selected, (bitvec_index_t) index);
        gridview_invalidate_cell(gv, index);
      }

      gv->activate(gv, index, gv->opaque);
    }

    return 1;

  case wuss_EVENT_POINTER_EXIT:
    /* no button state reaches a hover move, so a press the pointer leaves the
     * window during must end here or the next hover would start a drag */
    gv->press_index = -1;
    return 0;

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

  gridview_clear_selected(gv);
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

size2d_t wuss_gridview_get_cell_size(const wuss_gridview_t *gv)
{
  return gv->cell;
}

void wuss_gridview_remeasure(wuss_gridview_t *gv)
{
  wuss_t *wuss;

  wuss = gv->window->task->wuss;

  /* label widths only change with the font, so skip the item walk when the
   * font and its height are as they were when last measured */
  if (wuss->fonts.fonts[0] != gv->measured_font ||
      wuss__fontset_height(&wuss->fonts, 0) != gv->measured_height)
    gridview_measure_items(gv);
  if (!gv->fixed_cell)
    gv->cell = gridview_cell_size(gv);
  gridview_reflow(gv);
  wuss_window_invalidate_visible(gv->window);
}

void wuss_gridview_set_cell_size(wuss_gridview_t *gv, size2d_t cell)
{
  gv->cell       = SIZE2D(MAX(cell.w, 1), MAX(cell.h, 1));
  gv->fixed_cell = 1;

  gridview_reflow(gv);
  wuss_window_invalidate_visible(gv->window);
}

void wuss_gridview_set_background(wuss_gridview_t *gv, colour_t colour)
{
  gv->bg     = colour;
  gv->has_bg = 1;

  wuss_window_invalidate_visible(gv->window);
}

void wuss_gridview_set_layout(wuss_gridview_t *gv,
                              int              max_columns,
                              int              pow2,
                              int              inset)
{
  gv->max_columns = MAX(max_columns, 0);
  gv->pow2        = pow2 != 0;
  gv->inset       = MAX(inset, 0);

  gridview_reflow(gv);
  wuss_window_invalidate_visible(gv->window);
}

void wuss_gridview_set_padding(wuss_gridview_t *gv,
                               int              pad_x,
                               int              pad_y,
                               int              spacing)
{
  gv->pad_x   = MAX(pad_x, 0);
  gv->pad_y   = MAX(pad_y, 0);
  gv->spacing = MAX(spacing, 0);

  /* items and font are unchanged, so the cached content size still holds */
  if (!gv->fixed_cell)
    gv->cell = gridview_cell_size(gv);
  gridview_reflow(gv);
  wuss_window_invalidate_visible(gv->window);
}

void wuss_gridview_set_drag(wuss_gridview_t         *gv,
                            wuss_gridview_drag_fn_t *drag)
{
  gv->drag        = drag;
  gv->press_index = -1;
}

void wuss_gridview_set_draw(wuss_gridview_t         *gv,
                            wuss_gridview_draw_fn_t *draw,
                            int                      selectable)
{
  gv->draw       = draw;
  gv->selectable = selectable != 0;

  wuss_window_invalidate_visible(gv->window);
}

int wuss_gridview_get_columns(const wuss_gridview_t *gv)
{
  return gv->columns;
}

box_t wuss_gridview_cell_box(const wuss_gridview_t *gv, int index)
{
  return gridview_cell_box(gv, index);
}

int wuss_gridview_width_for_columns(const wuss_gridview_t *gv, int columns)
{
  return gridview_width_for_columns(gv, MAX(columns, 1));
}

size2d_t wuss_gridview_natural_size(const wuss_gridview_t *gv,
                                    int                    columns,
                                    int                    max_rows)
{
  int rows;

  columns = MIN(MAX(columns, 1), MAX(gv->count, 1));
  rows    = (gv->count + columns - 1) / columns;
  rows    = MIN(MAX(rows, 1), MAX(max_rows, 1));
  return SIZE2D(gridview_width_for_columns(gv, columns),
                rows * gv->cell.h + 2 * gv->inset);
}

int wuss_gridview_hit_test(const wuss_gridview_t *gv, point_t point)
{
  return gridview_hit_test(gv, point);
}

result_t wuss_gridview_window_create(wuss_gridview_t            **out,
                                     wuss_task_t                 *task,
                                     const char                  *title,
                                     int                          count,
                                     wuss_gridview_item_fn_t     *item_fn,
                                     int                          label_chars,
                                     wuss_gridview_activate_fn_t *activate,
                                     void                        *opaque,
                                     wuss_window_t              **window)
{
  result_t       rc;
  wuss_window_t *win;

  if (out == NULL || window == NULL)
    return result_NULL_ARG;

  /* No horizontal scrollbar: the grid reflows to the width instead. */
  rc = wuss_window_create_placed(task, SIZE2D(320, 240), title,
                                 (wuss_WINDOW_DEFAULT & ~wuss_WINDOW_HSCROLL) |
                                 wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP, SIZE2D(320, 240),
                                 SIZE2D(0, 0), &win);
  if (rc != result_OK)
    return rc;

  rc = wuss_gridview_create(out, win, count, item_fn, label_chars, activate,
                            opaque);
  if (rc != result_OK)
  {
    wuss_window_close(win);
    return rc;
  }

  *window = win;
  return result_OK;
}

void wuss_gridview_fit_window(wuss_gridview_t *gv, int columns, int max_rows)
{
  box_t    content;
  int      cols;
  size2d_t size;

  (void) wuss_window_resize(gv->window,
                            wuss_gridview_natural_size(gv, columns, max_rows));

  /* The resize is capped to the screen from the window's top-left, so it may
   * have stopped part way across a column: drop to the columns that fit. */
  wuss_window_get_content_bounds(gv->window, &content);
  cols = (content.x1 - content.x0) / gv->cell.w;
  if (cols < MIN(columns, MAX(gv->count, 1)))
  {
    size = wuss_gridview_natural_size(gv, MAX(cols, 1), max_rows);
    (void) wuss_window_resize(gv->window, size);
  }
}
