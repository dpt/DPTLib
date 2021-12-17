/* bmfont.h -- bitmap font engine */

// font data:
// bitmap
// em size
// height (to baseline)
// descender (from baseline)
// some way to get advance widths
// [kerning]
//
//
// interface:
// screen ptr / base
// plot position
// colour to plot using - some defined control character interface
// string to plot
// clip rect?

#ifndef DPTLIB_BMFONT_H
#define DPTLIB_BMFONT_H

#include "base/result.h"
#include "geom/point.h"
#include "framebuf/screen.h"

typedef struct bmfont bmfont_t;
typedef int bmfont_width_t; // in pixels?

result_t bmfont_create(const char *png,
                       bmfont_t  **bmfont);

void bmfont_destroy(bmfont_t *bmfont);

void bmfont_get_info(bmfont_t *bmfont, int *width, int *height);

result_t bmfont_measure(bmfont_t       *bmfont,
                        const char     *text,
                        int             len,
                        bmfont_width_t  target_width,
                        int            *split_point,
                        bmfont_width_t *actual_width);

result_t bmfont_draw(bmfont_t      *bmfont,
                     screen_t      *scr,
                     const char    *text,
                     int            len,
                     colour_t       fg,
                     colour_t       bg,
                     const point_t *pos,
                     point_t       *end_pos);

#endif /* DPTLIB_BMFONT_H */
