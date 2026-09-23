/* framebuf/bmfont.h -- proportional bitmap font engine */

#ifndef DPTLIB_BMFONT_H
#define DPTLIB_BMFONT_H

#include "base/result.h"
#include "geom/point.h"
#include "framebuf/screen.h"

/** A bitmap font handle. */
typedef struct bmfont bmfont_t;

/** The type used for bmfont_measure() measurements. */
typedef int bmfont_width_t; /* in pixels */

/** Behaviour flags for bmfont_set_flags(). */
enum
{
  /** Force every glyph to advance by the font's widest advance width,
   *  giving a fixed-pitch layout. Glyph bitmaps are unchanged: proportional
   *  ink sits left-aligned in the wider fixed cell. */
  bmfont_FLAG_MONOSPACE = 1 << 0
};

/** A set of bmfont_FLAG_* values. */
typedef unsigned int bmfont_flags_t;

/**
 * Extra spacing to apply on top of a font's own advance widths, passed to
 * bmfont_measure(), bmfont_draw() and bmfont_draw_relief(). A NULL pointer
 * means no extra spacing (as if both fields were zero).
 */
typedef struct bmfont_spacing
{
  int letter_spacing; /**< Added to every glyph's advance width, in pixels.
                        *   May be negative to tighten; a large negative
                        *   value can make glyphs overlap or run
                        *   backwards. */
  int word_spacing;    /**< Added to space (' ') glyphs, on top of
                         *   letter_spacing, in pixels. */
}
bmfont_spacing_t;

/**
 * Create a new bitmap font from a PNG format font file.
 *
 * \param[in]  png      Filename of the font file to load.
 * \param[out] bmfont   Newly allocated bitmap font.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfont_create(const char *png, bmfont_t **bmfont);

/**
 * Callback for bmfont_enumerate(), invoked once per font found.
 *
 * \param[in] name  The font's leafname with the ".png" extension stripped,
 *                  e.g. "Trinity.Medium" for "Trinity.Medium.png". Borrowed;
 *                  copy it if it must outlive the call.
 * \param[in] path  The full path that would be passed to bmfont_create() to
 *                  load this font. Borrowed.
 * \param[in] opaque  The pointer passed to bmfont_enumerate().
 * \return \ref result_OK to continue, \ref result_STOP_WALK to stop early
 *         (bmfont_enumerate() then also returns \ref result_OK), or any
 *         other code to abort with that code.
 */
typedef result_t (bmfont_enumerate_fn)(const char *name,
                                       const char *path,
                                       void       *opaque);

/**
 * Enumerate the bitmap fonts in a directory: every ".png" file in \p dir is
 * reported to \p fn (non-recursive, order unspecified).
 *
 * \param[in] dir     Directory to scan.
 * \param[in] fn      Called once per font; see bmfont_enumerate_fn.
 * \param[in] opaque  Passed through to \p fn.
 * \return \ref result_OK on success (including a stop-walk), \ref
 *         result_FILE_NOT_FOUND if \p dir cannot be opened, \ref
 *         result_NULL_ARG, or a code propagated from \p fn.
 */
result_t bmfont_enumerate(const char          *dir,
                          bmfont_enumerate_fn *fn,
                          void                *opaque);

/**
 * Destroy a bitmap font.
 *
 * \param[in] bmfont    Bitmap font to destroy.
 */
void bmfont_destroy(bmfont_t *bmfont);

/**
 * Set behaviour flags on a font.
 *
 * \param[in] bmfont    Bitmap font to modify.
 * \param[in] flags     A set of bmfont_FLAG_* values; replaces any
 *                      previously set flags.
 */
void bmfont_set_flags(bmfont_t *bmfont, bmfont_flags_t flags);

/**
 * Read the width, height, ascent and descent of the specified bitmap font.
 *
 * \param[in]  bmfont   Bitmap font to query.
 * \param[out] width    Width of the font in pixels, or NULL if not wanted.
 * \param[out] height   Height of the font in pixels, or NULL if not wanted.
 * \param[out] ascent   Baseline offset from the top of a glyph cell, in
 *                      pixels, or NULL if not wanted.
 * \param[out] descent  Offset from the baseline to the bottom of a glyph
 *                      cell, in pixels, or NULL if not wanted.
 */
void bmfont_get_info(bmfont_t *bmfont,
                     int      *width,
                     int      *height,
                     int      *ascent,
                     int      *descent);

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
 * \param[in]  spacing      Extra letter/word spacing, or NULL for none.
 * \param[in]  target_width Target width in pixels.
 * \param[out] split_point  Split point in pixels.
 * \param[out] actual_width Actual width of the split string in pixels.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfont_measure(bmfont_t               *bmfont,
                        const char             *text,
                        int                     len,
                        const bmfont_spacing_t *spacing,
                        bmfont_width_t          target_width,
                        int                    *split_point,
                        bmfont_width_t         *actual_width);

/**
 * Find where a caret should go for a pointer position within a string, e.g.
 * a mouse click in a text field. The caret snaps to the nearest character
 * boundary: a click in the left half of a glyph places it before that glyph,
 * in the right half after it. An exact midpoint goes to the lower index.
 *
 * \param[in]  bmfont   Bitmap font the string is drawn with.
 * \param[in]  text     String to search. May be NULL if \p len is zero.
 * \param[in]  len      Length of the string. May be zero.
 * \param[in]  spacing  Extra letter/word spacing, or NULL for none.
 * \param[in]  x        Pointer x relative to the string's start position, in
 *                      pixels. Negative values give index 0; values past the
 *                      end give \p len.
 * \param[out] index    Caret index in bytes, 0..len, or NULL if not wanted.
 * \param[out] caret_x  Caret x relative to the string's start position, as
 *                      bmfont_caret_x() would return for \p index, or NULL
 *                      if not wanted.
 */
void bmfont_find_caret(bmfont_t               *bmfont,
                       const char             *text,
                       int                     len,
                       const bmfont_spacing_t *spacing,
                       int                     x,
                       int                    *index,
                       bmfont_width_t         *caret_x);

/**
 * Compute the x position of a caret placed before byte \p index of a string.
 * The caret sits in the letter spacing gap after the preceding glyph, so it
 * does not overlap ink; at index 0 it sits at the start position.
 *
 * \param[in]  bmfont   Bitmap font the string is drawn with.
 * \param[in]  text     String. May be NULL if \p index is zero.
 * \param[in]  index    Caret index in bytes.
 * \param[in]  spacing  Extra letter/word spacing, or NULL for none.
 * \return Caret x relative to the string's start position, in pixels.
 */
bmfont_width_t bmfont_caret_x(bmfont_t               *bmfont,
                              const char             *text,
                              int                     index,
                              const bmfont_spacing_t *spacing);

/**
 * Draw an I-beam text caret: a 1px stem the full height of a glyph cell,
 * with 3px wide bars across its top and bottom rows.
 *
 * \param[in]  bmfont  Bitmap font whose cell height to use.
 * \param[in]  scr     Screen to draw on.
 * \param[in]  colour  Caret colour.
 * \param[in]  pos     Baseline position of the stem in pixels: the string's
 *                     start position plus bmfont_caret_x() in x.
 */
void bmfont_draw_caret(bmfont_t      *bmfont,
                       screen_t      *scr,
                       colour_t       colour,
                       const point_t *pos);

/**
 * Draw the given string using the specified font, position and colours.
 *
 * \param[in]   bmfont  Bitmap font to draw.
 * \param[in]   scr     Screen to draw on.
 * \param[in]   text    String to draw.
 * \param[in]   len     Length of the string.
 * \param[in]   fg      Foreground colour.
 * \param[in]   bg      Background colour.
 * \param[in]   spacing Extra letter/word spacing, or NULL for none.
 * \param[in]   pos     Baseline start position of the string in pixels.
 * \param[out]  end_pos Baseline end position of the string in pixels.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfont_draw(bmfont_t               *bmfont,
                     screen_t               *scr,
                     const char             *text,
                     int                     len,
                     colour_t                fg,
                     colour_t                bg,
                     const bmfont_spacing_t *spacing,
                     const point_t          *pos,
                     point_t                *end_pos);

/**
 * Draw the given string twice to produce a relief (drop-shadow) effect: a
 * shadow pass in colour \p shadow at (\p pos + \p offset), then the main
 * pass in colour \p fg at \p pos.
 *
 * Both passes are drawn with a transparent background, so this only makes
 * sense over existing content and \p fg and \p shadow should have full
 * alpha.
 *
 * \param[in]   bmfont  Bitmap font to draw.
 * \param[in]   scr     Screen to draw on.
 * \param[in]   text    String to draw.
 * \param[in]   len     Length of the string.
 * \param[in]   fg      Foreground colour, used for the main pass.
 * \param[in]   shadow  Shadow colour, used for the offset pass.
 * \param[in]   spacing Extra letter/word spacing, or NULL for none.
 * \param[in]   pos     Baseline position of the main pass in pixels.
 * \param[in]   offset  Shadow displacement in pixels, e.g. { 1, 1 }.
 * \param[out]  end_pos Baseline end position of the string in pixels.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfont_draw_relief(bmfont_t               *bmfont,
                            screen_t               *scr,
                            const char             *text,
                            int                     len,
                            colour_t                fg,
                            colour_t                shadow,
                            const bmfont_spacing_t *spacing,
                            const point_t          *pos,
                            const point_t          *offset,
                            point_t                *end_pos);

#endif /* DPTLIB_BMFONT_H */
