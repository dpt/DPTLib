/* bmfont.h -- proportional bitmap font engine */

#ifndef DPTLIB_BMFONT_H
#define DPTLIB_BMFONT_H

#include "base/result.h"
#include "geom/point.h"
#include "framebuf/screen.h"

/** A bitmap font handle. */
typedef struct bmfont bmfont_t;

/** The type used for bmfont_measure() measurements. */
typedef int bmfont_width_t; /* in pixels */

/**
 * Create a new bitmap font from a PNG format font file.
 *
 * \param[in]  png      Filename of the font file to load.
 * \param[out] bmfont   Newly allocated bitmap font.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfont_create(const char *png, bmfont_t **bmfont);

/**
 * Destroy a bitmap font.
 *
 * \param[in] bmfont    Bitmap font to destroy.
 */
void bmfont_destroy(bmfont_t *bmfont);

/**
 * Read the width and height of the specified bitmap font.
 *
 * \param[in]  bmfont   Bitmap font to query.
 * \param[out] width    Width of the font in pixels.
 * \param[out] height   Height of the font in pixels.
 */
void bmfont_get_info(bmfont_t *bmfont, int *width, int *height);

/**
 * Read the number of glyphs in the specified bitmap font. Glyphs are laid
 * out contiguously starting at ' ' (space, 0x20), so a char c has a glyph
 * iff c >= ' ' and c < ' ' + bmfont_get_count(bmfont).
 *
 * \param[in]  bmfont   Bitmap font to query.
 * \return Number of glyphs in the font.
 */
int bmfont_get_count(bmfont_t *bmfont);

/**
 * Measure the width of a string drawn with the specified font.
 *
 * \param[in]  bmfont       Bitmap font to measure.
 * \param[in]  text         String to measure.
 * \param[in]  len          Length of the string.
 * \param[in]  target_width Target width in pixels.
 * \param[out] split_point  Split point in pixels.
 * \param[out] actual_width Actual width of the split string in pixels.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfont_measure(bmfont_t       *bmfont,
                        const char     *text,
                        int             len,
                        bmfont_width_t  target_width,
                        int            *split_point,
                        bmfont_width_t *actual_width);

/**
 * Draw the given string using the specified font, position and colours.
 *
 * \param[in]   bmfont  Bitmap font to draw.
 * \param[in]   scr     Screen to draw on.
 * \param[in]   text    String to draw.
 * \param[in]   len     Length of the string.
 * \param[in]   fg      Foreground colour.
 * \param[in]   bg      Background colour.
 * \param[in]   pos     Position of the string in pixels.
 * \param[out]  end_pos End position of the string in pixels.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfont_draw(bmfont_t      *bmfont,
                     screen_t      *scr,
                     const char    *text,
                     int            len,
                     colour_t       fg,
                     colour_t       bg,
                     const point_t *pos,
                     point_t       *end_pos);

#endif /* DPTLIB_BMFONT_H */
