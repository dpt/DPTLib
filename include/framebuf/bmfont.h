/* framebuf/bmfont.h -- proportional bitmap font engine */

#ifndef DPTLIB_BMFONT_H
#define DPTLIB_BMFONT_H

#include "base/result.h"
#include "geom/point.h"
#include "framebuf/screen.h"
#include "utils/alloc.h"

/* ----------------------------------------------------------------------- */

/** A font's .map sidecar was malformed, out of order or out of range. */
#define result_BMFONT_BAD_MAP (result_BASE_BMFONT + 0)

/** A font could not be saved: every glyph in one row of 32 cells has a zero
 *  advance width, which the PNG format cannot record. */
#define result_BMFONT_NO_ADVANCE (result_BASE_BMFONT + 1)

/* ----------------------------------------------------------------------- */

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
  int word_spacing;    /**< Added to U+0020 SPACE glyphs, on top of
                         *   letter_spacing, in pixels. */
}
bmfont_spacing_t;

/**
 * Create a new bitmap font from a PNG format font file.
 *
 * \param[in]  alloc    Allocator for the font and its working memory, or
 *                      NULL. The font keeps it for later calls.
 * \param[in]  png      Filename of the font file to load.
 * \param[out] bmfont   Newly allocated bitmap font.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfont_create(const alloc_t *alloc,
                       const char    *png,
                       bmfont_t     **bmfont);

/**
 * Callback for bmfont_enumerate(), invoked once per font found.
 *
 * \param[in] name  The family name and style, e.g. "Trinity Medium" for
 *                  "Trinity/Medium.png". Borrowed; copy it if it must
 *                  outlive the call.
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
 * Enumerate the bitmap fonts under a directory: every ".png" file in each
 * family subdirectory of \p dir is reported to \p fn (one level down, order
 * unspecified). See bmfontfamily.h for the layout.
 *
 * \param[in] dir     Directory holding the family directories.
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
 * Read the number of glyphs in the specified bitmap font: its cell count, 32
 * times the rows in its PNG. Use bmfont_lookup to find which codepoints have
 * glyphs.
 *
 * \param[in]  bmfont   Bitmap font to query.
 * \return Number of glyphs in the font.
 */
int bmfont_get_count(bmfont_t *bmfont);

/**
 * Look up the glyph for a Unicode codepoint via the font's cmap, loaded from
 * its .map sidecar or, without one, the implicit mapping from U+0020.
 *
 * \param[in]  bmfont     Bitmap font to query.
 * \param[in]  codepoint  Unicode codepoint.
 * \return Glyph ID (cell index in reading order), or -1 if unmapped.
 */
int bmfont_lookup(bmfont_t *bmfont, unsigned long codepoint);

/**
 * List the codepoints that the font's cmap maps to a glyph: the reverse of
 * bmfont_lookup().
 *
 * \param[in]  bmfont      Bitmap font to query.
 * \param[in]  gid         Glyph ID, 0 to bmfont_get_count() - 1.
 * \param[out] codepoints  Array to receive the codepoints, in ascending
 *                         order. May be NULL if \p max is zero.
 * \param[in]  max         Number of elements in \p codepoints.
 * \return The number of codepoints that map to \p gid. This can be more than
 *         \p max, in which case only the first \p max were written.
 */
int bmfont_get_codepoints(const bmfont_t *bmfont,
                          int             gid,
                          unsigned long  *codepoints,
                          int             max);

/* ----------------------------------------------------------------------- */

/* Editing. These change a font in place, for a font editor. A font that
 * other code draws with sees each change at once. */

/**
 * Create a new, blank bitmap font. Every glyph has no ink and an advance
 * width of \p width - 1. The font has the implicit cmap from U+0020.
 *
 * \param[in]  alloc    Allocator for the font, or NULL. The font keeps it
 *                      for later calls.
 * \param[in]  width    Cell width in pixels, 3 to 32.
 * \param[in]  height   Cell height in pixels, 2 or more. The height plus one
 *                      (for the advance-width row) must be no more than
 *                      three times \p width, or the font cannot be loaded
 *                      back.
 * \param[in]  ascent   Baseline offset from the top of a cell, 1 to \p
 *                      height - 2. The cell bottom is the last row.
 * \param[in]  rows     Number of rows of 32 glyphs, 1 or more.
 * \param[out] bmfont   Newly allocated bitmap font.
 * \return \ref result_OK on success, \ref result_BAD_ARG if an argument is
 *         out of range, or \ref result_OOM.
 */
result_t bmfont_create_blank(const alloc_t *alloc,
                             int            width,
                             int            height,
                             int            ascent,
                             int            rows,
                             bmfont_t     **bmfont);

/**
 * Read one ink pixel of a glyph.
 *
 * \param[in] bmfont  Bitmap font to query.
 * \param[in] gid     Glyph ID, 0 to bmfont_get_count() - 1.
 * \param[in] x       Column, 0 to the cell width - 1.
 * \param[in] y       Row, 0 to the cell height - 1.
 * \return Non-zero if the pixel is ink.
 */
int bmfont_get_ink(const bmfont_t *bmfont, int gid, int x, int y);

/**
 * Set or clear one ink pixel of a glyph.
 *
 * \param[in] bmfont  Bitmap font to modify.
 * \param[in] gid     Glyph ID, 0 to bmfont_get_count() - 1.
 * \param[in] x       Column, 0 to the cell width - 1.
 * \param[in] y       Row, 0 to the cell height - 1.
 * \param[in] ink     Non-zero to set the pixel, zero to clear it.
 */
void bmfont_set_ink(bmfont_t *bmfont, int gid, int x, int y, int ink);

/**
 * Read a glyph's advance width as the font stores it. bmfont adds one pixel
 * of letter spacing to this when it measures or draws.
 *
 * \param[in] bmfont  Bitmap font to query.
 * \param[in] gid     Glyph ID, 0 to bmfont_get_count() - 1.
 * \return The stored advance width in pixels, 0 to the cell width.
 */
bmfont_width_t bmfont_get_advance(const bmfont_t *bmfont, int gid);

/**
 * Set a glyph's stored advance width. See bmfont_get_advance().
 *
 * \param[in] bmfont   Bitmap font to modify.
 * \param[in] gid      Glyph ID, 0 to bmfont_get_count() - 1.
 * \param[in] advance  Stored advance width in pixels, 0 to the cell width.
 */
void bmfont_set_advance(bmfont_t *bmfont, int gid, bmfont_width_t advance);

/**
 * Move the baseline. The cell bottom stays where it is, so the descent
 * changes too. Ink does not move.
 *
 * \param[in] bmfont  Bitmap font to modify.
 * \param[in] ascent  New baseline offset from the top of a cell. It must be
 *                    at least 1 and above the cell bottom.
 * \return \ref result_OK on success, or \ref result_BAD_ARG if \p ascent is
 *         out of range.
 */
result_t bmfont_set_ascent(bmfont_t *bmfont, int ascent);

/**
 * Add blank rows of 32 glyphs to the end of a font. Each new glyph has no
 * ink and an advance width of the cell width - 1. If the font has the
 * implicit cmap, the cmap grows to reach the new glyphs.
 *
 * \param[in] bmfont  Bitmap font to modify.
 * \param[in] rows    Number of rows to add, 1 or more.
 * \return \ref result_OK on success, \ref result_BAD_ARG if \p rows is less
 *         than 1, or \ref result_OOM. On failure the font is unchanged.
 */
result_t bmfont_add_rows(bmfont_t *bmfont, int rows);

/**
 * Save a font as a 2bpp paletted PNG that bmfont_create() can load. The
 * writer draws the advance-width strips left-justified, and the grid (left
 * sidebearing, baseline and cell bottom) in its standard places. So saving a
 * loaded font keeps its glyphs and metrics but can move cosmetic pixels. The
 * .map sidecar is not written.
 *
 * \param[in] bmfont  Bitmap font to save.
 * \param[in] png     Filename to write.
 * \return \ref result_OK on success, \ref result_BMFONT_NO_ADVANCE if a row
 *         of glyphs has no advance widths, \ref result_FOPEN_FAILED, \ref
 *         result_OOM, or \ref result_BAD_ARG if the write fails.
 */
result_t bmfont_save_png(const bmfont_t *bmfont, const char *png);

/**
 * Measure the width of a string drawn with the specified font.
 *
 * Text passed to bmfont is UTF-8 throughout. Lengths and indices count
 * bytes, and any index bmfont returns falls on a character boundary. Each
 * malformed byte counts as U+FFFD. A codepoint the font lacks draws its
 * U+FFFD glyph, or else a hollow box, except below U+0020 where it draws and
 * advances nothing.
 *
 * \param[in]  bmfont       Bitmap font to measure.
 * \param[in]  text         UTF-8 string to measure.
 * \param[in]  len          Length of the string in bytes.
 * \param[in]  spacing      Extra letter/word spacing, or NULL for none.
 * \param[in]  target_width Target width in pixels.
 * \param[out] split_point  Bytes of \p text which fit within \p
 *                          target_width.
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
 * \param[in]  text     UTF-8 string to search. May be NULL if \p len is
 *                      zero.
 * \param[in]  len      Length of the string in bytes. May be zero.
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
 * \param[in]  text     UTF-8 string. May be NULL if \p index is zero.
 * \param[in]  index    Caret index in bytes. An index inside a character
 *                      places the caret before that character.
 * \param[in]  spacing  Extra letter/word spacing, or NULL for none.
 * \return Caret x relative to the string's start position, in pixels.
 */
bmfont_width_t bmfont_caret_x(bmfont_t               *bmfont,
                              const char             *text,
                              int                     index,
                              const bmfont_spacing_t *spacing);

/**
 * Draw an I-beam text caret: a 1px stem spanning the glyph cell's ascent,
 * from the top of the cell down to the baseline row, extended by 2px at each
 * end, with a dot either side of the stem on its top and bottom rows.
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
 * \param[in]   text    UTF-8 string to draw.
 * \param[in]   len     Length of the string in bytes.
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
 * \param[in]   text    UTF-8 string to draw.
 * \param[in]   len     Length of the string in bytes.
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
