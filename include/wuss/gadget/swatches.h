/* wuss/gadget/swatches.h -- a grid of colour swatches on a window */

/**
 * \file swatches.h
 *
 * A wuss gadget: a grid of flat colour swatches on a caller's window, e.g. a
 * palette display or a paint program's colour well. Like \ref gridview.h it
 * owns no icons: the task calls \ref wuss_swatches_draw from its
 * wuss_EVENT_REDRAW handler and \ref wuss_swatches_hit_test from its
 * wuss_EVENT_MOUSE handler.
 *
 * The gadget is a plain struct the caller fills and keeps. It allocates
 * nothing and needs no destroy. The colours are borrowed, so the caller may
 * repoint them (e.g. after a wuss_EVENT_PALETTE) and invalidate the window.
 *
 * Built only when WUSS_GADGETS is defined.
 */

#ifndef WUSS_GADGET_SWATCHES_H
#define WUSS_GADGET_SWATCHES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "framebuf/colour.h"
#include "geom/box.h"
#include "geom/point.h"

#include "wuss/task.h"

/* ----------------------------------------------------------------------- */

/** A swatch grid's layout and colours. */
typedef struct wuss_swatches
{
  /** The grid's box, window work area. The cells share it evenly; any
   *  remainder is left undrawn at the right and bottom. */
  box_t           bbox;
  /** Number of columns; 0 for as many as make the grid roughly square. */
  int             columns;
  /** Pixels between neighbouring cells. */
  int             gap;
  /** Non-zero to edge each cell with a one-pixel black outline. */
  int             outline;
  /** The colours, one per cell, in row order. Borrowed. */
  const colour_t *colours;
  /** Number of colours. */
  int             ncolours;
}
wuss_swatches_t;

/* ----------------------------------------------------------------------- */

/**
 * Draw the grid. Call from the window's wuss_EVENT_REDRAW handler.
 *
 * \param[in] swatches The grid.
 * \param[in] event    The wuss_EVENT_REDRAW event.
 */
void wuss_swatches_draw(const wuss_swatches_t *swatches,
                        const wuss_event_t    *event);

/**
 * Find the swatch under a point. Gaps between cells miss.
 *
 * \param[in] swatches The grid.
 * \param[in] point    Point in window work area, e.g.
 *                     wuss_event_t::data::mouse::point.
 * \return The colour index of the swatch under \p point, or -1 for none.
 */
int wuss_swatches_hit_test(const wuss_swatches_t *swatches, point_t point);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_GADGET_SWATCHES_H */
