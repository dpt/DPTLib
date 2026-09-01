/* text/txtfmt.h -- text formatting */

/**
 * \file txtfmt.h
 *
 * Word-wraps a string to a given character width. Wraps at character widths,
 * not measured widths, so works best for monospaced text.
 *
 * - Breaks at spaces. - Forces a newline at \\n or \\r.
 */

#ifndef DATASTRUCT_TXTFMT_H
#define DATASTRUCT_TXTFMT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

/**
 * A txtfmt.
 */
typedef struct txtfmt txtfmt_t;

/* ----------------------------------------------------------------------- */

/**
 * Create a txtfmt holding a copy of the given string.
 *
 * The string is initially unwrapped.
 *
 * \param[in]  s  String to format. Copied in.
 * \param[out] tx New txtfmt.
 *
 * \return result_OK or result_OOM.
 */
result_t txtfmt_create(const char *s, txtfmt_t **tx);

/**
 * Destroy an existing txtfmt.
 *
 * \param[in] tx Txtfmt to destroy.
 */
void txtfmt_destroy(txtfmt_t *tx);

/* ----------------------------------------------------------------------- */

/**
 * Wrap the held string to the specified character width.
 *
 * \param[in] tx    Txtfmt to change.
 * \param[in] width Character width to wrap to.
 *
 * \return result_OK or result_OOM.
 */
result_t txtfmt_wrap(txtfmt_t *tx, int width);

/* ----------------------------------------------------------------------- */

/**
 * Returns the length of the string the txtfmt holds.
 *
 * \param[in] tx Txtfmt to query.
 *
 * \return Length in characters, excluding terminator.
 */
int txtfmt_get_length(const txtfmt_t *tx);

/**
 * Returns the number of lines produced by the last wrap.
 *
 * \param[in] tx Txtfmt to query.
 *
 * \return Number of lines.
 */
int txtfmt_get_nlines(const txtfmt_t *tx);

/**
 * Returns the wrapped width of a txtfmt.
 *
 * e.g. If you wrap some text containing a word 10 characters long it'll never
 * get any thinner than 10.
 *
 * \param[in] tx Txtfmt to query.
 *
 * \return Wrapped width in characters.
 */
int txtfmt_get_wrapped_width(const txtfmt_t *tx);

/**
 * Retrieve a line produced by the last wrap.
 *
 * The returned pointer refers into the txtfmt's own copy of the string and is
 * valid until the next call to txtfmt_wrap or txtfmt_destroy. It is not
 * NUL-terminated: use the returned length.
 *
 * \param[in]  tx     Txtfmt to query.
 * \param[in]  index  Line index, 0..txtfmt_get_nlines(tx)-1.
 * \param[out] line   Set to point at the line's first character.
 * \param[out] length Set to the line's length in characters.
 *
 * \return result_OK or result_BAD_ARG if index is out of range.
 */
result_t txtfmt_get_line(const txtfmt_t *tx,
                         int             index,
                         const char    **line,
                         int            *length);

/* ----------------------------------------------------------------------- */

/**
 * Print the wrapped text via printf, including line numbers (for testing and
 * debugging).
 *
 * \param[in] tx Txtfmt to print.
 *
 * \return result_OK.
 */
result_t txtfmt_print(const txtfmt_t *tx);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* DATASTRUCT_TXTFMT_H */
