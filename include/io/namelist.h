/* io/namelist.h -- collect matching dir leafnames into a fixed table */

#ifndef DPTLIB_NAMELIST_H
#define DPTLIB_NAMELIST_H

#include <stddef.h>

#include "base/result.h"

/**
 * Scan a directory (via dirscan_walk) and copy the leafnames that end in \p
 * ext into a caller-owned fixed-size table, with the extension stripped.
 *
 * The table is `cap` slots of `stride` bytes each, laid out contiguously
 * (i.e. a `char names[cap][stride]`). An entry is stored only if its name
 * with \p ext removed, plus a NUL, fits in \p stride; longer names are
 * skipped silently. Scanning stops once \p cap names are stored -- this is
 * not an error, `*pcount` simply reports the cap.
 *
 * \param[in]  dir    Directory to scan.
 * \param[in]  ext    Extension to match and strip, leading dot included
 *                    (e.g. ".png"). NULL or "" matches every entry and
 *                    strips nothing.
 * \param[out] names  Base of the `cap`-by-`stride` character table.
 * \param[in]  stride Bytes per table slot (must be >= 2).
 * \param[in]  cap    Number of slots in the table (must be >= 0).
 * \param[in]  sorted Non-zero to sort the stored names ascending (strcmp).
 * \param[out] pcount Number of names stored. Set on every non-error return.
 * \return \ref result_OK (including when the cap was hit), \ref
 *         result_NULL_ARG if \p dir, \p names or \p pcount is NULL, \ref
 *         result_BAD_ARG if \p stride < 2 or \p cap < 0, \ref
 *         result_FILE_NOT_FOUND if \p dir cannot be opened.
 */
result_t namelist_scan(const char *dir,
                       const char *ext,
                       char       *names,
                       size_t      stride,
                       int         cap,
                       int         sorted,
                       int        *pcount);

#endif /* DPTLIB_NAMELIST_H */
