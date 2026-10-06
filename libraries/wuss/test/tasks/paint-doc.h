/* wuss/test/tasks/paint-doc.h -- paint canvas, drawing operations and undo */

#ifndef TASKS_PAINT_DOC_H
#define TASKS_PAINT_DOC_H

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

/* the canvas is a bgrx8888 bitmap; hard-edged, opaque drawing only. every
 * drawing operation takes canvas pixel coordinates (clipped to the canvas) and
 * returns the box of canvas pixels it may have changed, empty (see
 * box_is_empty) if none. no wuss dependency, so this is testable headless. */

/* number of undo steps kept; the oldest is dropped beyond this */
#define PAINTDOC_UNDO_MAX 20

typedef struct paintdoc paintdoc_t;

/* create a white canvas of the given size. fails with result_BAD_ARG if
 * either dimension is not positive */
result_t paintdoc_create(size2d_t size, paintdoc_t **doc);

/* free a canvas and its undo history. doc may be NULL */
void paintdoc_destroy(paintdoc_t *doc);

/* the canvas pixels, for drawing to a window. owned by doc: valid until the
 * next paintdoc_undo or paintdoc_destroy */
const bitmap_t *paintdoc_bitmap(const paintdoc_t *doc);

/* save the canvas as an undo step. call before the first segment of a stroke:
 * paintdoc_stroke does not do it itself, as a stroke is many calls. fails
 * with result_OOM, leaving the history as it was */
result_t paintdoc_checkpoint(paintdoc_t *doc);

/* draw a round-ended line of the given width in pixels (1 = a single pixel
 * wide) from a to b; a == b draws a dab. also the brush and eraser stroke
 * segment */
box_t paintdoc_stroke(paintdoc_t *doc,
                      point_t     a,
                      point_t     b,
                      int         width,
                      colour_t    colour);

/* draw the rectangle with opposite corners a and b (both inclusive, any order),
 * filled or as an outline width pixels thick drawn inside it. takes its own
 * checkpoint */
box_t paintdoc_rect(paintdoc_t *doc,
                    point_t     a,
                    point_t     b,
                    int         width,
                    int         fill,
                    colour_t    colour);

/* flood fill from p, 4-connected, over pixels exactly equal to the one at p.
 * does nothing, and takes no checkpoint, if p is off the canvas or already
 * the fill colour; otherwise takes its own checkpoint */
box_t paintdoc_fill(paintdoc_t *doc, point_t p, colour_t colour);

/* non-zero if there is a step to undo */
int paintdoc_can_undo(const paintdoc_t *doc);

/* restore the canvas to the last checkpoint, discarding it. returns the box
 * that changed (the whole canvas), or an empty box if there was nothing to
 * undo or the step could not be decompressed */
box_t paintdoc_undo(paintdoc_t *doc);

#endif /* TASKS_PAINT_DOC_H */
