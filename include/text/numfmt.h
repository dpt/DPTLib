/* text/numfmt.h -- number formatting */

/**
 * \file numfmt.h
 *
 * Formats numbers for presentation to people.
 */

#ifndef TEXT_NUMFMT_H
#define TEXT_NUMFMT_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ----------------------------------------------------------------------- */

/**
 * Buffer size which holds any 64-bit value with its commas and terminator.
 */
#define numfmt_GROUPED_MAX (27)

/* ----------------------------------------------------------------------- */

/**
 * Format a number in decimal with its digits in comma separated groups of
 * three, e.g. 1234567 becomes "1,234,567".
 *
 * \param[in]  value Number to format.
 * \param[out] buf   Buffer of at least \ref numfmt_GROUPED_MAX bytes.
 *
 * \return \p buf, for convenience.
 */
char *numfmt_grouped(unsigned long long value, char *buf);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* TEXT_NUMFMT_H */
